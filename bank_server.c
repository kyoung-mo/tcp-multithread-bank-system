#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define PORT 8080
#define MAX_WORKERS 5           // 창구(워커 스레드) 개수
#define MAX_CLIENTS 25          // 총 클라이언트 수 (pi200~pi224)
#define MAX_ACCOUNTS 5          // 클라이언트당 최대 통장 개수
#define BUFFER_SIZE 1024
#define MAX_QUEUE 20            // 대기 큐 크기

// 통장 정보 구조체
typedef struct {
    char bank_name[50];         // 은행명
    int balance;                // 잔고
    bool is_active;             // 활성화 여부
} Account;

// 클라이언트 정보 구조체
typedef struct {
    char client_id[10];         // pi200 ~ pi224
    int ip_last_digit;          // IP 마지막 숫자 (200~224) = 비밀번호
    Account accounts[MAX_ACCOUNTS]; // 통장 배열
    int account_count;          // 현재 통장 개수
} ClientInfo;

// 대기 큐 구조체
typedef struct {
    int queue[MAX_QUEUE];       // 대기 중인 client_fd들
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} WaitingQueue;

// 워커 스레드 정보
typedef struct {
    int worker_id;              // 창구 번호
    pthread_t thread;
    bool is_busy;               // 업무 중 여부
    int client_fd;              // 현재 상담 중인 클라이언트
} WorkerThread;

// 전역 변수
ClientInfo client_db[MAX_CLIENTS];      // 클라이언트 DB
pthread_mutex_t db_mutex;               // DB 접근 mutex
WaitingQueue waiting_queue;             // 대기 큐
WorkerThread workers[MAX_WORKERS];      // 워커 스레드 풀
pthread_mutex_t workers_mutex;          // 워커 관리 mutex

// 함수 선언
void init_database();
void init_waiting_queue();
void enqueue(int client_fd);
int dequeue();
ClientInfo* find_client_by_ip(char* ip);
void* worker_thread_func(void* arg);
void handle_client(int worker_id, int client_fd, ClientInfo* client);
void process_account_open(int client_fd, ClientInfo* client);
void process_deposit(int client_fd, ClientInfo* client);
void process_withdraw(int client_fd, ClientInfo* client);
void show_accounts(int client_fd, ClientInfo* client);
int get_menu_choice(char* message);

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char client_ip[INET_ADDRSTRLEN];

    // 초기화
    init_database();
    init_waiting_queue();
    pthread_mutex_init(&db_mutex, NULL);
    pthread_mutex_init(&workers_mutex, NULL);

    // 워커 스레드 풀 생성 (5개 창구 미리 준비)
    for (int i = 0; i < MAX_WORKERS; i++) {
        workers[i].worker_id = i + 1;
        workers[i].is_busy = false;
        workers[i].client_fd = -1;
        pthread_create(&workers[i].thread, NULL, worker_thread_func, &workers[i]);
        printf("✅ 창구 %d번 준비 완료\n", i + 1);
    }

    // 소켓 생성
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR 설정 (재시작 시 즉시 바인딩 가능)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 바인딩
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 리슨
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("\n🏦 ========== 은행 영업 시작 ==========\n");
    printf("📍 포트: %d\n", PORT);
    printf("👥 총 창구 수: %d개\n", MAX_WORKERS);
    printf("=====================================\n\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // 클라이언트 연결 수락
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        // 클라이언트 IP 추출
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("\n📞 새 고객 접속: %s\n", client_ip);

        // IP 확인 (10.10.16.200 ~ 10.10.16.224만 허용)
        ClientInfo* client = find_client_by_ip(client_ip);
        if (client == NULL) {
            char* error_msg = "❌ 등록되지 않은 IP입니다. 연결을 종료합니다.\n";
            send(client_fd, error_msg, strlen(error_msg), 0);
            close(client_fd);
            printf("⚠️  등록되지 않은 IP 거부: %s\n", client_ip);
            continue;
        }

        printf("✅ 인증 성공: %s\n", client->client_id);

        // 비어있는 창구 찾기
        pthread_mutex_lock(&workers_mutex);
        int assigned = -1;
        for (int i = 0; i < MAX_WORKERS; i++) {
            if (!workers[i].is_busy) {
                workers[i].is_busy = true;
                workers[i].client_fd = client_fd;
                assigned = i;
                printf("🪟 창구 %d번에 배정되었습니다.\n", i + 1);
                
                // 워커 스레드 깨우기 (broadcast로 모든 워커 깨움)
                pthread_cond_broadcast(&waiting_queue.cond);
                break;
            }
        }
        pthread_mutex_unlock(&workers_mutex);

        // 모든 창구가 사용 중이면 대기 큐에 추가
        if (assigned == -1) {
            printf("⏳ 모든 창구가 사용 중입니다. 대기 큐에 추가합니다.\n");
            char* wait_msg = "⏳ 현재 모든 창구가 사용 중입니다. 잠시만 기다려주세요...\n";
            send(client_fd, wait_msg, strlen(wait_msg), 0);
            enqueue(client_fd);
        }
    }

    close(server_fd);
    return 0;
}

// 데이터베이스 초기화
void init_database() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        sprintf(client_db[i].client_id, "pi%d", 200 + i);
        client_db[i].ip_last_digit = 200 + i;
        client_db[i].account_count = 0;
        
        for (int j = 0; j < MAX_ACCOUNTS; j++) {
            client_db[i].accounts[j].is_active = false;
            client_db[i].accounts[j].balance = 0;
            memset(client_db[i].accounts[j].bank_name, 0, 50);
        }
    }
    printf("💾 클라이언트 DB 초기화 완료 (pi200 ~ pi224)\n");
}

// 대기 큐 초기화
void init_waiting_queue() {
    waiting_queue.front = 0;
    waiting_queue.rear = 0;
    waiting_queue.count = 0;
    pthread_mutex_init(&waiting_queue.mutex, NULL);
    pthread_cond_init(&waiting_queue.cond, NULL);
}

// 대기 큐에 추가
void enqueue(int client_fd) {
    pthread_mutex_lock(&waiting_queue.mutex);
    if (waiting_queue.count < MAX_QUEUE) {
        waiting_queue.queue[waiting_queue.rear] = client_fd;
        waiting_queue.rear = (waiting_queue.rear + 1) % MAX_QUEUE;
        waiting_queue.count++;
        printf("🎫 번호표 발급: 대기 인원 %d명\n", waiting_queue.count);
    }
    pthread_mutex_unlock(&waiting_queue.mutex);
}

// 대기 큐에서 꺼내기
int dequeue() {
    int client_fd = -1;
    pthread_mutex_lock(&waiting_queue.mutex);
    if (waiting_queue.count > 0) {
        client_fd = waiting_queue.queue[waiting_queue.front];
        waiting_queue.front = (waiting_queue.front + 1) % MAX_QUEUE;
        waiting_queue.count--;
        printf("📢 대기 고객 호출: 남은 대기 인원 %d명\n", waiting_queue.count);
    }
    pthread_mutex_unlock(&waiting_queue.mutex);
    return client_fd;
}

// IP로 클라이언트 찾기
ClientInfo* find_client_by_ip(char* ip) {
    // IP 형식: 10.10.16.XXX
    int last_octet;
    if (sscanf(ip, "10.10.16.%d", &last_octet) != 1) {
        // 로컬 테스트를 위해 127.0.0.1도 허용 (pi200으로 매핑)
        if (strcmp(ip, "127.0.0.1") == 0) {
            return &client_db[0]; // pi200
        }
        return NULL;
    }
    
    if (last_octet < 200 || last_octet > 224) {
        return NULL;
    }
    
    return &client_db[last_octet - 200];
}

// 워커 스레드 함수
void* worker_thread_func(void* arg) {
    WorkerThread* worker = (WorkerThread*)arg;
    
    while (1) {
        // 업무 대기
        pthread_mutex_lock(&workers_mutex);
        while (!worker->is_busy) {
            pthread_cond_wait(&waiting_queue.cond, &workers_mutex);
        }
        int client_fd = worker->client_fd;
        pthread_mutex_unlock(&workers_mutex);

        if (client_fd == -1) continue;

        // 클라이언트 IP로 정보 찾기
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        getpeername(client_fd, (struct sockaddr*)&addr, &addr_len);
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        ClientInfo* client = find_client_by_ip(client_ip);
        if (client) {
            handle_client(worker->worker_id, client_fd, client);
        }

        // 업무 종료
        close(client_fd);
        printf("🪟 창구 %d번 업무 종료. 대기 상태로 전환.\n", worker->worker_id);
        
        // 다음 대기 고객 확인
        pthread_mutex_lock(&workers_mutex);
        int next_client = dequeue();
        if (next_client != -1) {
            // 대기 중인 고객이 있으면 바로 처리 (is_busy 상태 유지)
            worker->client_fd = next_client;
            worker->is_busy = true;  // 명시적으로 유지
            printf("📢 창구 %d번: 대기 고객 즉시 배정\n", worker->worker_id);
            pthread_mutex_unlock(&workers_mutex);
            // 다음 루프에서 바로 처리됨
        } else {
            // 대기 고객이 없으면 대기 모드로 전환
            worker->is_busy = false;
            worker->client_fd = -1;
            pthread_mutex_unlock(&workers_mutex);
        }
    }
    
    return NULL;
}

// 클라이언트 처리
void handle_client(int worker_id, int client_fd, ClientInfo* client) {
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    // 환영 메시지
    snprintf(response, BUFFER_SIZE, 
        "\n🏦 ========== 은행 업무 시작 ==========\n"
        "👤 고객님: %s\n"
        "🪟 담당 창구: %d번\n"
        "=====================================\n",
        client->client_id, worker_id);
    send(client_fd, response, strlen(response), 0);
    
    // 업무 처리 루프
    while (1) {
        // 업무 선택 요청
        char* prompt = "💬 어떤 업무를 도와드릴까요?\n"
                      "   (통장 개설 / 입금 / 출금 중 원하시는 업무를 말씀해주세요)\n\n"
                      "입력: ";
        send(client_fd, prompt, strlen(prompt), 0);
        
        // 클라이언트 요청 받기
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client_fd, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) {
            printf("⚠️  [창구 %d] %s 연결 종료\n", worker_id, client->client_id);
            return;
        }
        
        printf("💬 [창구 %d] %s: %s", worker_id, client->client_id, buffer);
        
        // 키워드 파싱하여 메뉴 선택
        int menu = get_menu_choice(buffer);
        
        switch (menu) {
            case 1: // 통장 개설
                process_account_open(client_fd, client);
                break;
            case 2: // 입금
                process_deposit(client_fd, client);
                break;
            case 3: // 출금
                process_withdraw(client_fd, client);
                break;
            default:
                snprintf(response, BUFFER_SIZE, 
                    "❌ 요청하신 업무를 찾을 수 없습니다.\n"
                    "   '통장 개설', '입금', '출금' 중 하나를 말씀해주세요.\n\n");
                send(client_fd, response, strlen(response), 0);
                continue; // 다시 업무 선택으로
        }
        
        // 추가 업무 여부 확인
        char* ask_more = "\n💡 추가로 처리하실 업무가 있으신가요? (예/아니오): ";
        send(client_fd, ask_more, strlen(ask_more), 0);
        printf("📤 [창구 %d] 추가 업무 질문 전송\n", worker_id);
        
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = read(client_fd, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) {
            printf("⚠️  [창구 %d] %s 연결 종료\n", worker_id, client->client_id);
            return;
        }
        
        printf("📥 [창구 %d] 추가 업무 응답: %s", worker_id, buffer);
        
        // "아니오", "아니요", "없어", "없습니다", "종료", "끝" 등으로 종료
        if (strstr(buffer, "아니") != NULL || 
            strstr(buffer, "없") != NULL || 
            strstr(buffer, "종료") != NULL || 
            strstr(buffer, "끝") != NULL ||
            strstr(buffer, "no") != NULL ||
            strstr(buffer, "No") != NULL ||
            strstr(buffer, "NO") != NULL) {
            break; // 업무 종료
        }
        
        // "예", "네", "있어요", "yes" 등으로 계속
        printf("🔄 [창구 %d] %s 추가 업무 진행\n", worker_id, client->client_id);
    }
    
    // 종료 메시지
    char* goodbye = "\n✅ 업무가 완료되었습니다. 감사합니다!\n";
    send(client_fd, goodbye, strlen(goodbye), 0);
    
    printf("✅ [창구 %d] %s 고객 업무 완료\n", worker_id, client->client_id);
}

// 메뉴 선택 (키워드 기반)
int get_menu_choice(char* message) {
    // 1번: "통장" AND "개설" 둘 다 포함
    if (strstr(message, "통장") != NULL && strstr(message, "개설") != NULL) {
        return 1;
    }
    // 2번: "입금" 포함
    if (strstr(message, "입금") != NULL) {
        return 2;
    }
    // 3번: "출금" 포함
    if (strstr(message, "출금") != NULL) {
        return 3;
    }
    return 0; // 알 수 없는 요청
}

// 통장 개설 처리
void process_account_open(int client_fd, ClientInfo* client) {
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    pthread_mutex_lock(&db_mutex);
    
    // 이미 5개 통장이 있는지 확인
    if (client->account_count >= MAX_ACCOUNTS) {
        snprintf(response, BUFFER_SIZE, 
            "❌ 더 이상 통장을 개설할 수 없습니다.\n"
            "   (최대 %d개까지만 가능합니다)\n", MAX_ACCOUNTS);
        send(client_fd, response, strlen(response), 0);
        pthread_mutex_unlock(&db_mutex);
        return;
    }
    
    pthread_mutex_unlock(&db_mutex);
    
    // 은행명 입력 요청
    char* prompt = "\n💳 개설할 통장의 은행명을 입력하세요: ";
    send(client_fd, prompt, strlen(prompt), 0);
    
    // 은행명 받기
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = read(client_fd, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) {
        return;
    }
    
    // 개행 문자 제거
    buffer[strcspn(buffer, "\n")] = 0;
    
    // DB에 통장 추가
    pthread_mutex_lock(&db_mutex);
    
    int idx = client->account_count;
    strncpy(client->accounts[idx].bank_name, buffer, 49);
    client->accounts[idx].balance = 0;
    client->accounts[idx].is_active = true;
    client->account_count++;
    
    snprintf(response, BUFFER_SIZE, 
        "\n✅ 통장 개설이 완료되었습니다!\n"
        "   📌 은행명: %s\n"
        "   💰 초기 잔고: 0원\n"
        "   📊 현재 통장 개수: %d/%d\n",
        client->accounts[idx].bank_name, 
        client->account_count, 
        MAX_ACCOUNTS);
    send(client_fd, response, strlen(response), 0);
    
    pthread_mutex_unlock(&db_mutex);
    
    printf("💳 [통장 개설] %s - %s 통장 개설 완료\n", 
        client->client_id, client->accounts[idx].bank_name);
}

// 통장 목록 보여주기
void show_accounts(int client_fd, ClientInfo* client) {
    char response[BUFFER_SIZE * 2];
    int offset = 0;
    
    pthread_mutex_lock(&db_mutex);
    
    offset += sprintf(response + offset, "\n📋 보유 통장 목록:\n");
    offset += sprintf(response + offset, "=====================================\n");
    
    if (client->account_count == 0) {
        offset += sprintf(response + offset, "   (보유한 통장이 없습니다)\n");
    } else {
        for (int i = 0; i < client->account_count; i++) {
            if (client->accounts[i].is_active) {
                offset += sprintf(response + offset, 
                    "   %d. %s - 잔고: %d원\n", 
                    i + 1, 
                    client->accounts[i].bank_name, 
                    client->accounts[i].balance);
            }
        }
    }
    offset += sprintf(response + offset, "=====================================\n");
    
    pthread_mutex_unlock(&db_mutex);
    
    send(client_fd, response, strlen(response), 0);
}

// 입금 처리
void process_deposit(int client_fd, ClientInfo* client) {
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    // 입금 대상 ID 입력 요청
    char* prompt = "\n💵 입금할 대상의 ID를 입력하세요 (예: pi200): ";
    send(client_fd, prompt, strlen(prompt), 0);
    
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = read(client_fd, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) return;
    buffer[strcspn(buffer, "\n")] = 0;
    
    // 대상 클라이언트 찾기
    ClientInfo* target = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(client_db[i].client_id, buffer) == 0) {
            target = &client_db[i];
            break;
        }
    }
    
    if (target == NULL) {
        snprintf(response, BUFFER_SIZE, "❌ 존재하지 않는 ID입니다.\n");
        send(client_fd, response, strlen(response), 0);
        return;
    }
    
    // 대상의 통장 목록 보여주기
    pthread_mutex_lock(&db_mutex);
    
    if (target->account_count == 0) {
        snprintf(response, BUFFER_SIZE, 
            "❌ %s님은 개설된 통장이 없습니다.\n", target->client_id);
        send(client_fd, response, strlen(response), 0);
        pthread_mutex_unlock(&db_mutex);
        return;
    }
    
    int offset = 0;
    offset += sprintf(response + offset, "\n📋 %s님의 통장 목록:\n", target->client_id);
    for (int i = 0; i < target->account_count; i++) {
        if (target->accounts[i].is_active) {
            offset += sprintf(response + offset, "   %d. %s\n", 
                i + 1, target->accounts[i].bank_name);
        }
    }
    offset += sprintf(response + offset, "\n입금할 통장 번호를 선택하세요: ");
    send(client_fd, response, strlen(response), 0);
    
    pthread_mutex_unlock(&db_mutex);
    
    // 통장 번호 선택
    memset(buffer, 0, BUFFER_SIZE);
    bytes_read = read(client_fd, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) return;
    
    int account_num = atoi(buffer) - 1;
    if (account_num < 0 || account_num >= target->account_count) {
        snprintf(response, BUFFER_SIZE, "❌ 잘못된 통장 번호입니다.\n");
        send(client_fd, response, strlen(response), 0);
        return;
    }
    
    // 입금액 입력
    prompt = "\n입금액을 입력하세요: ";
    send(client_fd, prompt, strlen(prompt), 0);
    
    memset(buffer, 0, BUFFER_SIZE);
    bytes_read = read(client_fd, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) return;
    
    int amount = atoi(buffer);
    if (amount <= 0) {
        snprintf(response, BUFFER_SIZE, "❌ 올바른 금액을 입력하세요.\n");
        send(client_fd, response, strlen(response), 0);
        return;
    }
    
    // 입금 처리
    pthread_mutex_lock(&db_mutex);
    target->accounts[account_num].balance += amount;
    
    snprintf(response, BUFFER_SIZE, 
        "\n✅ 입금이 완료되었습니다!\n"
        "   📌 입금 대상: %s\n"
        "   🏦 은행: %s\n"
        "   💰 입금액: %d원\n"
        "   📊 입금 후 잔고: %d원\n",
        target->client_id,
        target->accounts[account_num].bank_name,
        amount,
        target->accounts[account_num].balance);
    send(client_fd, response, strlen(response), 0);
    
    pthread_mutex_unlock(&db_mutex);
    
    printf("💵 [입금] %s → %s (%s 통장) %d원\n", 
        client->client_id, target->client_id, 
        target->accounts[account_num].bank_name, amount);
}

// 출금 처리
void process_withdraw(int client_fd, ClientInfo* client) {
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    // 본인 통장 확인
    pthread_mutex_lock(&db_mutex);
    
    if (client->account_count == 0) {
        snprintf(response, BUFFER_SIZE, 
            "❌ 개설된 통장이 없습니다.\n"
            "   먼저 통장을 개설해주세요.\n");
        send(client_fd, response, strlen(response), 0);
        pthread_mutex_unlock(&db_mutex);
        return;
    }
    
    pthread_mutex_unlock(&db_mutex);
    
    // 통장 목록 보여주기
    show_accounts(client_fd, client);
    
    // 통장 선택
    char* prompt = "\n출금할 통장 번호를 선택하세요: ";
    send(client_fd, prompt, strlen(prompt), 0);
    
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = read(client_fd, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) return;
    
    int account_num = atoi(buffer) - 1;
    if (account_num < 0 || account_num >= client->account_count) {
        snprintf(response, BUFFER_SIZE, "❌ 잘못된 통장 번호입니다.\n");
        send(client_fd, response, strlen(response), 0);
        return;
    }
    
    // 비밀번호 확인 (IP 마지막 3자리)
    prompt = "\n비밀번호를 입력하세요 (ID 뒷 3자리): ";
    send(client_fd, prompt, strlen(prompt), 0);
    
    memset(buffer, 0, BUFFER_SIZE);
    bytes_read = read(client_fd, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) return;
    
    int password = atoi(buffer);
    if (password != client->ip_last_digit) {
        snprintf(response, BUFFER_SIZE, "❌ 비밀번호가 일치하지 않습니다.\n");
        send(client_fd, response, strlen(response), 0);
        printf("⚠️  [출금 실패] %s - 비밀번호 불일치\n", client->client_id);
        return;
    }
    
    // 출금액 입력
    prompt = "\n출금액을 입력하세요: ";
    send(client_fd, prompt, strlen(prompt), 0);
    
    memset(buffer, 0, BUFFER_SIZE);
    bytes_read = read(client_fd, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) return;
    
    int amount = atoi(buffer);
    if (amount <= 0) {
        snprintf(response, BUFFER_SIZE, "❌ 올바른 금액을 입력하세요.\n");
        send(client_fd, response, strlen(response), 0);
        return;
    }
    
    // 잔고 확인 및 출금 처리
    pthread_mutex_lock(&db_mutex);
    
    if (client->accounts[account_num].balance < amount) {
        snprintf(response, BUFFER_SIZE, 
            "❌ 잔고가 부족합니다.\n"
            "   현재 잔고: %d원\n"
            "   출금 요청액: %d원\n",
            client->accounts[account_num].balance, amount);
        send(client_fd, response, strlen(response), 0);
        pthread_mutex_unlock(&db_mutex);
        return;
    }
    
    client->accounts[account_num].balance -= amount;
    
    snprintf(response, BUFFER_SIZE, 
        "\n✅ 출금이 완료되었습니다!\n"
        "   🏦 은행: %s\n"
        "   💰 출금액: %d원\n"
        "   📊 출금 후 잔고: %d원\n",
        client->accounts[account_num].bank_name,
        amount,
        client->accounts[account_num].balance);
    send(client_fd, response, strlen(response), 0);
    
    pthread_mutex_unlock(&db_mutex);
    
    printf("💸 [출금] %s - %s 통장에서 %d원 출금\n", 
        client->client_id, client->accounts[account_num].bank_name, amount);
}
