# FileSync: A Tool for Efficiently Synchronizing Files Over a Network

**FileSync** is a lightweight and modular tool designed to **synchronize files between machines over a network with minimal data transfer**.  
Instead of copying entire files, FileSync intelligently detects changes at the block level and transfers only the modified parts — saving bandwidth, time, and system resources.

## ✨ Key Features

- 🔍 **Delta-based synchronization**: Transfers only changed file segments using block-level diffing.
- ⚡ **High performance**: Optimized for large files and efficient memory usage.
- 🌐 **Network-aware design**: Built for low-latency synchronization between remote systems.

## 🚀 Use Cases

- Keeping remote directories in sync during development or deployment.
- Updating backups incrementally.
- Efficient file sharing over unreliable or slow networks.

## 🛠️ Build Instructions

### Prerequisites
- C++17 or later
- CMake (version 3.10+)
- g++ / clang++
- Linux (WSL or native)

### 🔧 Build Steps

```bash
# Clone the repository
git clone https://github.com/pranjal1702/fileSync
cd filesync

# Create a build directory
mkdir build && cd build

# Configure and build
cmake ..
make
```

### 🚀 Run Instructions

### 🖥️ Start the Server (Remote Endpoint)
- 🛰️ The server simply listens and responds to incoming client requests.
```bash
./syncApp server
# Waits for incoming connections from clients, Handles storage, file retrieval, and synchronization requests.
```

### 🖥️ Start the Client
- 🧭 The client is the controller and must initiate all operations (connect, push, pull, etc.).
```bash
./syncApp client

## use cases for client
```bash
```bash
connect <ip> <port>                            Connect to the server (supports up to 3 concurrent sessions)
push <session_id> <local_path> <remote_path>   Push the local file to the server, efficiently overwriting the remote file
pull <session_id> <remote_path> <local_path>   Pull the remote file from the server, efficiently overwriting the local file
disconnect <session_id>                        Terminate the specified session with the server
list                                           View all active session IDs with their connection details
help                                           Display all supported client commands
exit                                           Exit the client application gracefully
```

## 🧠 Sync Strategy: Rolling Hash + Delta Encoding

FileSync minimizes data transfer using a block-based sync strategy, combining:

- **Rolling Hash (Weak)**: Fast window-based detection (O(1) update per shift).
- **Strong Hash (MD5)**: Used to confirm exact block matches and avoid collisions.

---
1. **Destination** divides its file into blocks (e.g, 64 KB) and sends `[rolling_hash, strong_hash]` for each block.
2. **Source** slides an window (of size exactly equal to block size used by destination) over its file:
   - Computes rolling hash at each offset.
   - Checks for match in destination's weak hash set.
   - If match found, verifies with strong hash (MD5).
3. Based on comparisons delta instructions are formed and they are of two type:
   - Copy `offset_of_block`
   - Insert `data`
4. Destination reconstructs file using delta instructions

---

## 🌐 Network Protocol

FileSync uses a lightweight, reliable, and clearly structured TCP protocol between the source and destination. The entire sync session flows through these phases:

### 🔗 1. Handshake & Version Check
Both sides exchange a magic identifier and protocol version to verify compatibility.

### 📢 2. Command Declaration
The source declares its intent (`PUSH` or `PULL`) so the destination can follow the correct workflow.

### 📂 3. File Path Exchange
Source sends the file path. Destination acknowledges readiness for hash or data exchange.

### 📊 4. Hash Blueprint Transfer
The side with the latest file sends a compact list of block hashes (rolling + strong).  
The other side uses this to determine which blocks are already synchronized.

### 🧩 5. Delta Instruction Stream
A stream of `CopyBlock` and `InsertData` instructions is sent to reconstruct the target file with minimal data.

### ✅ 6. Final Acknowledgment
A status message confirms success or reports any failure.  
Both sides cleanly close the session afterward.

> ❗ All operations include **status reporting** to handle errors gracefully and ensure synchronization integrity.








