// Demonstrates a simple speed test using a custom exception‑based
// communication channel between two processes.
// Launch two instances with the "speed" argument; they negotiate roles
// via a named shared memory mapping and perform two rounds of message
// exchange. Round 1: server raises exceptions, client debugs; Round 2: roles swap.

#include <Windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <string_view>

// Named shared memory for role negotiation.
static constexpr char kSharedMappingName[] = "Global\\UDCommMapping";

// Custom exception code for communication.
static constexpr DWORD kExceptionCommsCode = 0x1337;

// Number of messages to send per round.
static constexpr int kTestIterations = 10000;

struct SharedData {
    DWORD server_pid;  // PID of the process that created the mapping.
    DWORD client_pid;  // PID of the second process to join.
};

// Raises an exception carrying the message pointer and length.
// The receiving process (debugger) will count these exception events.
void SendMessageException(std::string_view message) {
    ULONG_PTR params[2] = {
      reinterpret_cast<ULONG_PTR>(message.data()),
      static_cast<ULONG_PTR>(message.size() + 1)  // include null terminator
    };

    __try {
        RaiseException(kExceptionCommsCode, 0, 2, params);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Exception handled; continue execution.
    }
}

// Attaches as a debugger to `partner_pid` and counts `iterations` exceptions.
void DebugPartnerProcess(DWORD partner_pid,
    std::string_view role,
    int iterations) {
    std::cout << role << " debugger: attaching to PID " << partner_pid << "\n";
    if (!DebugActiveProcess(partner_pid)) {
        std::cerr << role << " debugger: attach failed (Error: "
            << GetLastError() << ")\n";
        return;
    }

    int count = 0;
    DEBUG_EVENT event = {};
    while (count < iterations) {
        if (WaitForDebugEvent(&event, INFINITE)) {
            if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
                auto& record = event.u.Exception.ExceptionRecord;
                if (record.ExceptionCode == kExceptionCommsCode &&
                    record.NumberParameters >= 2) {
                    ++count;
                }
            }
            ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
        }
        else {
            std::cerr << role << " debugger: wait failed (Error: "
                << GetLastError() << ")\n";
            break;
        }
    }

    if (!DebugActiveProcessStop(partner_pid)) {
        std::cerr << role << " debugger: detach failed (Error: "
            << GetLastError() << ")\n";
    }
    else {
        std::cout << role << " debugger: detached from PID "
            << partner_pid << "\n";
    }
}

// Performs one round of the speed test:
// - If is_server is true: send exceptions.
// - Otherwise: debug and count incoming exceptions.
void RunSpeedTestRound(bool is_server, DWORD partner_pid) {
    using clock = std::chrono::steady_clock;
    if (is_server) {
        std::this_thread::sleep_for(std::chrono::seconds(3));  // allow attach
        std::cout << "Round 1 (server sends, client debugs):\n";
        auto start = clock::now();
        for (int i = 0; i < kTestIterations; ++i) {
            SendMessageException("hello, world!");
        }
        auto end = clock::now();
        std::chrono::duration<double> elapsed = end - start;
        double rate = kTestIterations / elapsed.count();
        std::cout << "Server: sent " << kTestIterations << " messages in "
            << elapsed.count() << "s (" << rate << " msg/s)\n";
    }
    else {
        std::cout << "Round 1 (client debugs server):\n";
        DebugPartnerProcess(partner_pid, "Client", kTestIterations);
    }
}

// Swaps roles for the second round.
void RunSwapRoleRound(bool is_server, DWORD partner_pid) {
    using clock = std::chrono::steady_clock;
    if (is_server) {
        std::cout << "Round 2 (server debugs client):\n";
        DebugPartnerProcess(partner_pid, "Server", kTestIterations);
    }
    else {
        std::this_thread::sleep_for(std::chrono::seconds(3));  // allow attach
        std::cout << "Round 2 (client sends, server debugs):\n";
        auto start = clock::now();
        for (int i = 0; i < kTestIterations; ++i) {
            SendMessageException("hello, world!");
        }
        auto end = clock::now();
        std::chrono::duration<double> elapsed = end - start;
        double rate = kTestIterations / elapsed.count();
        std::cout << "Client: sent " << kTestIterations << " messages in "
            << elapsed.count() << "s (" << rate << " msg/s)\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::strcmp(argv[1], "speed") != 0) {
        std::cout << "Usage: ExceptionCommsSpeedTest.exe speed\n";
        return EXIT_SUCCESS;
    }

    // Create or open the shared memory mapping.
    HANDLE mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
        sizeof(SharedData), kSharedMappingName);
    if (!mapping) {
        std::cerr << "Error creating file mapping ("
            << GetLastError() << ")\n";
        return EXIT_FAILURE;
    }

    auto shared = static_cast<SharedData*>(
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData)));
    if (!shared) {
        std::cerr << "Error mapping view ("
            << GetLastError() << ")\n";
        CloseHandle(mapping);
        return EXIT_FAILURE;
    }

    bool is_server = (GetLastError() != ERROR_ALREADY_EXISTS);
    DWORD my_pid = GetCurrentProcessId();
    DWORD partner_pid = 0;

    if (is_server) {
        std::cout << "Role: server (PID " << my_pid << ")\n";
        shared->server_pid = my_pid;
        shared->client_pid = 0;
        // Wait up to 10 seconds for client to join.
        for (int i = 0; i < 10 && shared->client_pid == 0; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        partner_pid = shared->client_pid;
        if (!partner_pid) {
            std::cerr << "Client did not join; exiting.\n";
            UnmapViewOfFile(shared);
            CloseHandle(mapping);
            return EXIT_FAILURE;
        }
    }
    else {
        std::cout << "Role: client (PID " << my_pid << ")\n";
        partner_pid = shared->server_pid;
        shared->client_pid = my_pid;
    }

    std::cout << "Connected to PID " << partner_pid << "\n\n";

    RunSpeedTestRound(is_server, partner_pid);
    RunSwapRoleRound(is_server, partner_pid);

    UnmapViewOfFile(shared);
    CloseHandle(mapping);

    std::cout << "\nSpeed test complete.\n";
    return EXIT_SUCCESS;
}
