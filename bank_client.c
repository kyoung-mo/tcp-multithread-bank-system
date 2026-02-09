#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void clear_input_buffer();

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char input[BUFFER_SIZE] = {0};

    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║      🏦 온라인 뱅킹 시스템 🏦       ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("\n");
    printf("⚠️  사용 안내: 서버 메시지를 먼저 확인한 후 입력해주세요.\n");
    printf("   (입력 프롬프트가 나타나기 전에 타이핑하지 마세요)\n");
    printf("\n");

    // 소켓 생성
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("❌ 소켓 생성 실패\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 서버 주소 설정 (로컬 테스트: 127.0.0.1 / 실제: 10.10.16.1 등)
    printf("서버 IP 주소를 입력하세요 (10.10.16.222 입력): ");
    fgets(input, BUFFER_SIZE, stdin);
    input[strcspn(input, "\n")] = 0;
    
    char* server_ip = (strlen(input) == 0) ? "127.0.0.1" : input;
    
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("❌ 잘못된 주소입니다.\n");
        return -1;
    }

    // 서버 연결
    printf("\n🔄 은행 서버에 연결 중...\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("❌ 연결 실패! 서버가 실행 중인지 확인하세요.\n");
        return -1;
    }

    printf("✅ 연결 성공!\n\n");

    // 서버로부터 환영 메시지 수신
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = read(sock, buffer, BUFFER_SIZE);
    if (bytes_read > 0) {
        printf("%s", buffer);
    }

    // 대기 상태 메시지 확인 (창구가 모두 사용 중인 경우)
    if (strstr(buffer, "대기") != NULL || strstr(buffer, "기다려") != NULL) {
        // 추가 메시지 대기
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = read(sock, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            printf("%s", buffer);
        }
    }

    // 대화형 통신 시작 (업무 처리 루프)
    while (1) {
        // 서버 응답 수신
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = read(sock, buffer, BUFFER_SIZE);
        
        if (bytes_read <= 0) {
            printf("\n⚠️  서버와의 연결이 종료되었습니다.\n");
            break;
        }

        printf("%s", buffer);

        // 업무 완료 메시지 확인
        if (strstr(buffer, "업무가 완료") != NULL || 
            strstr(buffer, "감사합니다") != NULL) {
            break;
        }

        // 연결 종료 메시지 확인
        if (strstr(buffer, "연결을 종료") != NULL) {
            break;
        }

        // 프롬프트 확인 (입력이 필요한 경우)
        if (strstr(buffer, "입력:") != NULL ||
            strstr(buffer, "입력하세요") != NULL || 
            strstr(buffer, "선택하세요") != NULL ||
            strstr(buffer, "예/아니오") != NULL) {
            
            // 입력 대기를 명확히 표시
            fflush(stdout);
            
            // 사용자 입력
            memset(input, 0, BUFFER_SIZE);
            if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
                break;
            }
            
            // 빈 입력(개행만 있는 경우) 재시도
            while (input[0] == '\n' && strlen(input) <= 1) {
                printf("(입력해주세요): ");
                fflush(stdout);
                if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
                    break;
                }
            }
            
            // 서버로 전송
            send(sock, input, strlen(input), 0);
        }
    }

    // 연결 종료
    close(sock);
    printf("\n👋 은행 업무를 종료합니다.\n\n");

    return 0;
}

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}
