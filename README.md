# 🏦 TCP Multithread Bank System

> A multithreaded banking system implementation using TCP socket programming and POSIX threads in C

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![POSIX](https://img.shields.io/badge/POSIX-Threads-green?style=for-the-badge)
![TCP](https://img.shields.io/badge/TCP-Socket-blue?style=for-the-badge)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)

## 📖 Overview

This project demonstrates a **real-time banking system** where multiple clients can simultaneously perform banking operations through a server managing **5 worker threads (teller windows)**.

### Key Features

- ✅ **Thread Pool Pattern**: 5 pre-created worker threads for efficient resource management
- ✅ **Waiting Queue System**: FIFO queue with priority-based allocation
- ✅ **IP-based Authentication**: Client identification using IP address (10.10.16.200~224)
- ✅ **Mutex Synchronization**: Thread-safe operations on shared resources
- ✅ **Multi-session Support**: Continuous banking operations in a single session

### Banking Operations

1. **Account Creation** 📝 - Create up to 5 bank accounts per client
2. **Deposit** 💰 - Deposit to own or others' accounts
3. **Withdrawal** 💸 - Withdraw from own accounts with password authentication

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────┐
│              Main Thread (Bank)                  │
│  - Accepts client connections                    │
│  - IP authentication (10.10.16.200~224)         │
│  - Assigns to worker threads or waiting queue   │
└──────────┬──────────────────────────────────────┘
           │
           ├─► Worker Thread 1 (Window 1) ─► Client A
           ├─► Worker Thread 2 (Window 2) ─► Client B
           ├─► Worker Thread 3 (Window 3) ─► Client C
           ├─► Worker Thread 4 (Window 4) ─► Client D
           ├─► Worker Thread 5 (Window 5) ─► Client E
           │
           └─► Waiting Queue (Circular Queue)
                ├─ Client F (1st in queue)
                ├─ Client G (2nd in queue)
                └─ ...
```

---

## 🚀 Getting Started

### Prerequisites

- GCC compiler with pthread support
- Linux/Unix operating system
- Basic understanding of C programming and networking

### Installation

```bash
# Clone the repository
git clone https://github.com/yourusername/tcp-multithread-bank-system.git
cd tcp-multithread-bank-system

# Compile
make

# Or compile manually
gcc -Wall -pthread -o bank_server bank_server.c
gcc -Wall -pthread -o bank_client bank_client.c
```

### Running the System

**Terminal 1: Start Server**
```bash
./bank_server
```

**Terminal 2: Start Client**
```bash
./bank_client
```

---

## 💻 Usage Example

### Account Creation
```
Input: I want to open an account
> KB Bank

✅ Account created successfully!
   📌 Bank: KB Bank
   💰 Initial balance: 0 KRW
   📊 Total accounts: 1/5
```

### Deposit
```
Input: Deposit
> pi222 (Target ID)
> 1 (Select account)
> 100000 (Amount)

✅ Deposit completed!
   💰 Amount: 100,000 KRW
   📊 New balance: 100,000 KRW
```

### Withdrawal
```
Input: Withdraw
> 1 (Select account)
> 222 (Password: last 3 digits of IP)
> 50000 (Amount)

✅ Withdrawal completed!
   💰 Amount: 50,000 KRW
   📊 New balance: 50,000 KRW
```

---

## 📊 Technical Details

### Data Structures

```c
// Client Information
typedef struct {
    char client_id[10];         // pi200 ~ pi224
    int ip_last_digit;          // Last 3 digits of IP = password
    Account accounts[5];        // Max 5 accounts
    int account_count;          // Current account count
} ClientInfo;

// Account Information
typedef struct {
    char bank_name[50];         // Bank name
    int balance;                // Balance
    bool is_active;             // Active status
} Account;

// Waiting Queue
typedef struct {
    int queue[MAX_QUEUE];       // Waiting client file descriptors
    int front, rear, count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} WaitingQueue;
```

### Synchronization Mechanisms

#### Mutex Protection
- **db_mutex**: Protects client database operations
- **workers_mutex**: Manages worker thread states
- **queue_mutex**: Guards waiting queue operations

#### Condition Variables
- **waiting_queue.cond**: Signals worker threads on new client arrival

```c
// Worker thread waiting
pthread_cond_wait(&waiting_queue.cond, &workers_mutex);

// Main thread signaling
pthread_cond_broadcast(&waiting_queue.cond);
```

---

## 🛠️ Configuration

### Adjustable Parameters

```c
#define PORT 8080              // Server port
#define MAX_WORKERS 5          // Number of worker threads
#define MAX_CLIENTS 25         // Total clients (pi200~pi224)
#define MAX_ACCOUNTS 5         // Max accounts per client
#define MAX_QUEUE 20           // Waiting queue capacity
```

### IP Range
- Valid IPs: `10.10.16.200` ~ `10.10.16.224`
- Local testing: `127.0.0.1` (mapped to pi200)

---

## 🐛 Known Issues & Solutions

### Issue 1: Menu not appearing on second connection
**Cause**: Using `pthread_cond_signal()` wakes only one thread  
**Solution**: Use `pthread_cond_broadcast()` to wake all threads

### Issue 2: stdin buffer timing issue
**Cause**: User typing before prompt appears  
**Solution**: 
- Detect empty input and re-prompt
- Display usage warning at startup

---

## 🔍 Project Structure

```
tcp-multithread-bank-system/
├── bank_server.c          # Server implementation
├── bank_client.c          # Client implementation
├── Makefile              # Build automation
├── README.md             # This file
├── EXAMPLES.md           # Usage examples
├── USER_GUIDE.md         # User manual
├── CHANGELOG.md          # Version history
└── BLOG_POST.md          # Technical blog post (Korean)
```

---

## 📚 Learning Outcomes

### Networking
- TCP socket programming (`socket`, `bind`, `listen`, `accept`, `connect`)
- Client-server architecture
- Network byte order handling

### Multithreading
- POSIX threads (`pthread_create`, `pthread_mutex`, `pthread_cond`)
- Thread pool pattern
- Race condition prevention
- Deadlock avoidance

### Data Structures
- Circular queue implementation
- Thread-safe data structures
- In-memory database design

### System Programming
- IP address conversion (`inet_ntop`, `inet_pton`)
- Socket options (`SO_REUSEADDR`)
- Signal handling

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🤝 Contributing

Contributions, issues, and feature requests are welcome!

1. Fork the project
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 👨‍💻 Author

**구영모 (Koo Youngmo)**
- Blog: [Velog](https://velog.io/@mommers)
- GitHub: [@yourusername](https://github.com/kyoung-mo)

---

## 📖 References

- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [POSIX Threads Programming](https://computing.llnl.gov/tutorials/pthreads/)
- [Linux System Programming](https://www.oreilly.com/library/view/linux-system-programming/9781449341527/)
- [TCP/IP Illustrated, Volume 1](https://www.amazon.com/TCP-Illustrated-Vol-Addison-Wesley-Professional/dp/0201633469)

---

## ⭐ Star History

If you find this project useful, please consider giving it a star! ⭐

---

**Built with ❤️ using C, TCP Sockets, and POSIX Threads**
