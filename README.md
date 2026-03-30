# 🏦 TCP Multithread Bank System

> TCP 소켓 프로그래밍과 POSIX 스레드를 활용한 멀티스레드 은행 시스템 구현 (C 언어)

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![POSIX](https://img.shields.io/badge/POSIX-Threads-green?style=for-the-badge)
![TCP](https://img.shields.io/badge/TCP-Socket-blue?style=for-the-badge)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)

## 📖 Overview

여러 클라이언트가 **5개의 워커 스레드(창구)** 를 통해 동시에 은행 업무를 처리할 수 있는 **실시간 은행 시스템**입니다.

### Key Features

- ✅ **Thread Pool 패턴**: 5개의 사전 생성 워커 스레드로 효율적인 자원 관리
- ✅ **대기 큐 시스템**: 우선순위 기반 배정을 지원하는 FIFO 큐
- ✅ **IP 기반 인증**: IP 주소를 통한 클라이언트 식별 (10.10.16.200~224)
- ✅ **Mutex 동기화**: 공유 자원에 대한 스레드 안전 처리
- ✅ **멀티 세션 지원**: 단일 세션에서 연속적인 은행 업무 처리

### Banking Operations

1. **계좌 개설** 📝 - 클라이언트당 최대 5개 계좌 생성
2. **입금** 💰 - 본인 또는 타인 계좌에 입금
3. **출금** 💸 - 비밀번호 인증 후 본인 계좌에서 출금

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────┐
│              메인 스레드 (은행)                   │
│  - 클라이언트 접속 수락                           │
│  - IP 인증 (10.10.16.200~224)                   │
│  - 워커 스레드 또는 대기 큐에 배정               │
└──────────┬──────────────────────────────────────┘
           │
           ├─► 워커 스레드 1 (창구 1) ─► 클라이언트 A
           ├─► 워커 스레드 2 (창구 2) ─► 클라이언트 B
           ├─► 워커 스레드 3 (창구 3) ─► 클라이언트 C
           ├─► 워커 스레드 4 (창구 4) ─► 클라이언트 D
           ├─► 워커 스레드 5 (창구 5) ─► 클라이언트 E
           │
           └─► 대기 큐 (원형 큐)
                ├─ 클라이언트 F (1번째 대기)
                ├─ 클라이언트 G (2번째 대기)
                └─ ...
```

---

## 🚀 Getting Started

### Prerequisites

- pthread를 지원하는 GCC 컴파일러
- Linux/Unix 운영체제
- C 프로그래밍 및 네트워킹 기초 지식

### Installation

```bash
# 레포지토리 클론
git clone https://github.com/kyoung-mo/tcp-multithread-bank-system.git
cd tcp-multithread-bank-system

# Make로 빌드
make

# 또는 수동 컴파일
gcc -Wall -pthread -o bank_server bank_server.c
gcc -Wall -pthread -o bank_client bank_client.c
```

### Running the System

**터미널 1: 서버 실행**
```bash
./bank_server
```

**터미널 2: 클라이언트 실행**
```bash
./bank_client
```

---

## 💻 Usage Example

### Account Creation
```
입력: 계좌를 개설하고 싶습니다
> KB은행

✅ 계좌가 성공적으로 개설되었습니다!
   📌 은행: KB은행
   💰 초기 잔액: 0원
   📊 총 계좌 수: 1/5
```

### Deposit
```
입력: 입금
> pi222 (대상 ID)
> 1 (계좌 선택)
> 100000 (금액)

✅ 입금 완료!
   💰 입금액: 100,000원
   📊 잔액: 100,000원
```

### Withdrawal
```
입력: 출금
> 1 (계좌 선택)
> 222 (비밀번호: IP 끝 3자리)
> 50000 (금액)

✅ 출금 완료!
   💰 출금액: 50,000원
   📊 잔액: 50,000원
```

---

## 📊 Technical Details

### Data Structures

```c
// 클라이언트 정보
typedef struct {
    char client_id[10];         // pi200 ~ pi224
    int ip_last_digit;          // IP 끝 3자리 = 비밀번호
    Account accounts[5];        // 최대 5개 계좌
    int account_count;          // 현재 계좌 수
} ClientInfo;

// 계좌 정보
typedef struct {
    char bank_name[50];         // 은행 이름
    int balance;                // 잔액
    bool is_active;             // 활성 상태
} Account;

// 대기 큐
typedef struct {
    int queue[MAX_QUEUE];       // 대기 클라이언트 파일 디스크립터
    int front, rear, count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} WaitingQueue;
```

### Synchronization Mechanisms

#### Mutex Protection
- **db_mutex**: 클라이언트 데이터베이스 접근 보호
- **workers_mutex**: 워커 스레드 상태 관리
- **queue_mutex**: 대기 큐 접근 보호

#### Condition Variables
- **waiting_queue.cond**: 새 클라이언트 도착 시 워커 스레드에 신호 전달

```c
// 워커 스레드 대기
pthread_cond_wait(&waiting_queue.cond, &workers_mutex);

// 메인 스레드 신호 전달
pthread_cond_broadcast(&waiting_queue.cond);
```

---

## 🛠️ Configuration

### Adjustable Parameters

```c
#define PORT 8080              // 서버 포트
#define MAX_WORKERS 5          // 워커 스레드 수
#define MAX_CLIENTS 25         // 전체 클라이언트 수 (pi200~pi224)
#define MAX_ACCOUNTS 5         // 클라이언트당 최대 계좌 수
#define MAX_QUEUE 20           // 대기 큐 최대 크기
```

### IP Range
- 유효 IP: `10.10.16.200` ~ `10.10.16.224`
- 로컬 테스트: `127.0.0.1` (pi200으로 매핑)

---

## 🐛 Known Issues & Solutions

### Issue 1: 두 번째 접속 시 메뉴가 나타나지 않음
**원인**: `pthread_cond_signal()` 사용 시 하나의 스레드만 깨움  
**해결**: `pthread_cond_broadcast()`로 모든 스레드를 깨우도록 변경

### Issue 2: stdin 버퍼 타이밍 문제
**원인**: 프롬프트 출력 전에 사용자가 입력을 시작하는 경우  
**해결**:
- 빈 입력 감지 후 재출력 처리
- 시작 시 사용 안내 메시지 표시

---

## 🔍 Project Structure

```
tcp-multithread-bank-system/
├── bank_server.c          # 서버 구현
├── bank_client.c          # 클라이언트 구현
├── Makefile               # 빌드 자동화
└── README.md              # 이 파일
```

---

## 📚 Learning Outcomes

### Networking
- TCP 소켓 프로그래밍 (`socket`, `bind`, `listen`, `accept`, `connect`)
- 클라이언트-서버 아키텍처
- 네트워크 바이트 오더 처리

### Multithreading
- POSIX 스레드 (`pthread_create`, `pthread_mutex`, `pthread_cond`)
- Thread Pool 패턴
- Race Condition 방지
- 데드락 회피

### Data Structures
- 원형 큐 구현
- 스레드 안전 자료구조
- 인메모리 데이터베이스 설계

### System Programming
- IP 주소 변환 (`inet_ntop`, `inet_pton`)
- 소켓 옵션 (`SO_REUSEADDR`)
- 시그널 처리

---

## 👨‍💻 Author

**구영모 (Koo Youngmo)**
- Blog: [Velog](https://velog.io/@mommers)
- GitHub: [@kyoung-mo](https://github.com/kyoung-mo)
