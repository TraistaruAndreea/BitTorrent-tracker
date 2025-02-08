# MPI Peer-to-Peer File Sharing System

## Overview
This project implements a **peer-to-peer (P2P) file sharing system** using **MPI** for distributed communication and **Pthreads** for concurrency. The system consists of multiple peers that share and request file chunks from one another, coordinated by a central tracker.

## Features
- **MPI-Based Communication**: Uses **MPI** for inter-process messaging.
- **Multithreading with Pthreads**: Each peer handles uploads and downloads concurrently.
- **Swarm-Based File Sharing**: Peers exchange file chunks dynamically.
- **Tracker-Based Coordination**: A designated tracker node manages file availability.
- **Graceful Shutdown**: Ensures all peers finish before exiting.

## System Components
1. **Tracker (Rank 0)**:
   - Maintains file availability information.
   - Handles swarm requests from peers.
   - Sends shutdown signals upon completion.
2. **Peers (All Other Ranks)**:
   - Read input files to determine owned and wanted files.
   - Request chunks from other peers.
   - Upload chunks upon request.

## Code Structure
- **`tracker()`**: Handles file indexing and peer coordination.
- **`peer()`**: Manages file requests and chunk transfers.
- **`download_thread_func()`**: Handles downloading of file chunks.
- **`upload_thread_func()`**: Serves chunk requests from other peers.
- **`initialize_client_data()`**: Allocates and initializes peer data structures.
- **`read_input_file()`**: Reads peer-owned and wanted files.

## Thread Synchronization
- **Mutexes**: Protect shared data structures.
- **MPI Barrier**: Ensures synchronization between peers and tracker.


