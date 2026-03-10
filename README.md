# coinbase-advanced-l2-cpp
C++20 client for Coinbase Advanced Trade WebSocket level2 channel – real-time order book snapshots, incremental updates, and full book synchronization with JWT (ES256) authentication. 

# coinbase-advanced-l2-cpp

**C++20 client for Coinbase Advanced Trade WebSocket level2 channel** – real-time order book snapshots, incremental updates, and full book synchronization with JWT (ES256) authentication.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/YOUR_USERNAME/coinbase-advanced-l2-cpp/actions) <!-- Update with real badge later -->

Modern, efficient C++20 implementation for subscribing to and maintaining the **Level 2** (full depth) order book from Coinbase Advanced Trade via WebSocket.

It connects to `wss://advanced-trade-ws.coinbase.com`, authenticates using a JWT signed with your EC PRIVATE KEY (secp256r1 / ES256), subscribes to the `level2` channel (guaranteed delivery per Coinbase docs), processes snapshots and incremental updates, and keeps an in-sync bid/ask order book.

## Features

- Secure JWT authentication (ES256 with PEM-loaded private key)
- Real-time handling of `snapshot` and `update` messages from the `level2` channel
- Efficient order book maintenance (bids descending, asks ascending)
- Minimal external dependencies (header-only where possible)
- Handles large initial snapshots (e.g., BTC-USD depth)

## Requirements

- C++20 compliant compiler (GCC 11+, Clang 13+, MSVC 19.29+)
- CMake 3.18+
- OpenSSL (for crypto / ECDSA signing) or Botan
- pybind11
- Boost async io websockets
- jsoncpp

## Installation

### Clone the repo

```bash
git clone https://github.com/wdsnyc/coinbase-advanced-l2-cpp.git
cd coinbase-advanced-l2-cpp
