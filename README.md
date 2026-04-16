# FSM Cache Controller Simulator  

## 1. Overview
This project is a direct-mapped cache controller simulator implemented in C++. It simulates a simple cache using a Moore Finite-State Machine (FSM) architecture, inspired by the four-state model presented in *Computer Organization and Design* (Patterson & Hennessy). 

To ensure clean, scalable logic, the FSM is built using the **State Design Pattern**, isolating each state into its own dedicated class.

## 2. Cache Architecture
The simulator strictly adheres to the following architectural parameters:

| Parameter | Value | Notes |
| :--- | :--- | :--- |
| **Cache Type** | Direct-mapped | `index = addr[13:4]` |
| **Write Policy** | Write-back | Memory is updated only when a dirty block is evicted |
| **Miss Policy** | Write-allocate | Allocates a block first, then writes |
| **Cache Size** | 16 KiB | 16,384 bytes total |
| **Block Size** | 16 bytes | 4 x 32-bit words (128 bits) per block |
| **Number of Blocks** | 1,024 | $16 \text{ KiB} \div 16 \text{ B}$ |
| **Address Width** | 32 bits | RV32 architecture standard |
| **Tag Field** | 18 bits | Bits [31:14] |
| **Index Field** | 10 bits | Bits [13:4] |
| **Offset Field** | 4 bits | Bits [3:0] |
| **Metadata** | 1 Valid bit, 1 Dirty bit | Per-block metadata accurately simulated via C++ bit-fields |
| **Memory Latency** | 100 cycles | Simulated delay for Main Memory access |
| **Cache Latency** | 1 cycle | Simulated delay for Tag Comparison / Hits |

## 3. Build Instructions

Ensure you have a C++ compiler installed (GCC/g++ or Clang).

- Linux: ```sudo apt install build-essential ```
- macOS: ```brew install gcc ```
- Windows: MinGW or WSL (Windows Subsystem for Linux).


#### Windows / Linux / MacOS
Open your terminal, navigate to the project directory, and compile the code using `g++`:

```
g++ -std=c++17 -O2 -o cache_sim cache_sim.cpp
./cache_sim
```