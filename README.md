# exception-based-comms

A proof‑of‑concept speed test demonstrating IPC via custom Windows exceptions and the Debugging API.  
**WARNING:** This is VERY hacky and fragile.

---

## Table of Contents

- [Overview](#overview)  
- [Features](#features)  
- [Prerequisites](#prerequisites)  
- [Building](#building)  
- [Usage](#usage)  
- [How It Works](#how-it-works)  
- [Performance Results](#performance-results)  
- [Limitations & Warnings](#limitations--warnings)  
- [Contributing](#contributing)  
- [License](#license)  

---

## Overview

This project launches two copies of the same executable in “speed” mode. They negotiate roles (server/client) through a shared memory mapping, then perform two rounds of message exchange:

1. **Round 1**: Server raises a custom exception repeatedly; client attaches as debugger and counts exceptions.  
2. **Round 2**: Roles swap.

The tool prints the throughput (messages per second) for each round.

---

## Features

- Role negotiation via a named `FileMapping`  
- Custom exception code (`0x1337`) to carry a pointer+length  
- Two‑phase speed test with bidirectional measurements  
- Outputs precise timing (using `QueryPerformanceCounter`)  

---

## Prerequisites

- Windows 7 or later  
- MSVC (Visual Studio) or any compiler supporting Win32 APIs  
- Administrator privileges may be required to debug another process

---

## Building

1. Clone the repo:  
   ```bash
   git clone https://github.com/Smellon69/exception-based-comms.git
   cd exception-based-comms
   ```
2. Open the solution/existing project in Visual Studio, or compile via command line:  
   ```bash
   cl.exe /EHsc main.cpp /link /out:main.exe
   ```

---

## Usage

```bash
# Launch first instance (server):
exception-based-comms.exe speed

# Launch second instance (client) within 10 seconds:
exception-based-comms.exe speed
```

You’ll see output like:

```
Role: server (PID 1234)
Connected to PID 5678

Round 1 (server sends, client debugs):
Server: sent 10000 messages in 0.12s (83333 msg/s)

Round 2 (server debugs client):
Server debugger: attached to PID 5678
Server debugger: detached from PID 5678

Speed test complete.
```

---

## How It Works

1. **Shared Memory**  
   A `CreateFileMapping` named `"Global\\UDCommMapping"` holds two PIDs for role negotiation.  
2. **Exception‑Based Messaging**  
   `RaiseException(0x1337, …)` carries the message pointer/length.  
3. **Debug Loop**  
   The debugging process uses `WaitForDebugEvent`/`ContinueDebugEvent` to catch and count exception events.  
4. **Timing**  
   Uses `QueryPerformanceCounter` for high‑resolution measurement.

---

## Performance Results

| Round       | Role        | Throughput (msg/s) |
| ----------- | ----------- | ------------------ |
| Round 1     | Server send | 83 333             |
| Round 2     | Client send | 79 214             |

_Results will vary significantly based on CPU, OS version, and debugger overhead._

---

## Contributing

1. Fork the repo  
2. Create a new branch (`git checkout -b feature/your‑idea`)  
3. Commit your changes  
4. Open a Pull Request  

Please keep comments concise and follow Google’s C++ Style Guide.

---

## License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.
