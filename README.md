# fileShare

## Description
FileShare is a Peer-2-Peer secure file sharing platform utilizing a centralized tracker for network management and discovery.

## How it works
### First-Time Setup
On first launch, the user is prompted to:
   - Select a directory that will be public (All files inside it will be shared).
   - These file paths are saved to a local index (e.g., `shared_files.txt`).
   - Choose a port for incoming connections.
### Runtime Menu
After setup, the user is presented with an interactive menu:
- **Add a File**: Add a file to the local sharing index.
- **Download a File**: Connect to a peer and request a file.
- **View Online Peers**: List currently known online peers from the tracker.
- **Browse Available Files**: View shared files across the network.


## Architecture

- **Tracker**:
  - Maintains a registry of online peers and their shared files.
  - Facilitates discovery but does not host any files.

- **Peer Client**:
  - Acts both as a server (for uploads) and client (for downloads).
  - Registers itself and its shared files with the tracker at startup.

## Tech
* C++
* Boost library

## Project Structure
```
fileShare/
├── include/                    Header files
├── src/                        Source files
├── shared_files.txt            File records
├── tracker/
├── CMakeLists.txt
└── README.md
```