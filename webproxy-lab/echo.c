#include "csapp.h"

// echo:
// 이미 연결된 클라이언트 소켓(connfd)에서 한 줄을 읽고,
// 그 줄을 그대로 다시 같은 소켓으로 보내는 함수
void echo(int connfd);  // connfd: 현재 연결된 클라이언트와 통신하는 소켓 fd

// ./echo 8080 -> argc = 2, argv[0] = "./echo", argv[1] = "8080"
int main(int argc, char **argv)     // argc: 인자 개수, argv: 인자 문자열 배열
{
    int listenfd, connfd;   // listenfd: 대기용 소켓 fd, connfd: 실제 통신용 소켓 fd
    socklen_t clientlen;    // clientaddr 구조체 크기
    struct sockaddr_storage clientaddr;     // 접속한 클라이언트 주소 원본(바이너리 형식)
    char hostname[MAXLINE], port[MAXLINE];  // clientaddr를 문자열로 바꿔 담을 버퍼

    if (argc != 2)  // 인자 개수가 2개가 아니면
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]); // stderr: 표준 에러 출력
        exit(1);    // 프로그램 종료, 1: 정상종료 아님
    }

    // 지정한 포트에서 클라이언트 접속을 기다리는 리스닝 소켓을 만든다.
    listenfd = Open_listenfd(argv[1]);

    // 서버는 보통 한 번 실행되면 계속 살아 있으므로 무한 루프로 동작한다.
    // 이 루프 1번이 "클라이언트 1명 처리"라고 보면 된다.
    while (1)
    {
        // Accept가 clientaddr에 주소를 써 넣을 수 있도록 버퍼 크기를 먼저 알려준다.
        clientlen = sizeof(clientaddr);

        // 리스닝 소켓에서 접속 요청 하나를 받아들인다.
        // 성공하면:
        // - connfd: 실제 데이터 송수신에 사용할 새 소켓 fd
        // - clientaddr: 접속한 상대의 주소 원본
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // clientaddr는 구조체 형태라 사람이 보기 불편하므로
        // host 문자열과 port 문자열로 바꿔서 로그를 찍기 쉽게 만든다.
        Getnameinfo((SA *)&clientaddr, clientlen,
                    hostname, MAXLINE, port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", hostname, port);

        // 이 연결에서 클라이언트가 보내는 줄을 읽고 그대로 되돌려준다.
        // 클라이언트가 연결을 끊으면 echo 함수가 끝난다.
        echo(connfd);

        // 한 클라이언트와의 통신이 끝났으므로 연결 전용 소켓을 닫는다.
        // 그 다음 while 루프가 다시 돌면서 다음 클라이언트를 기다린다.
        Close(connfd);
    }
}

// echo 함수의 동작 순서:
// 1. connfd를 읽기용 RIO 구조체와 연결한다.
// 2. 클라이언트가 보낸 한 줄을 읽는다.
// 3. 읽은 바이트 수를 출력한다.
// 4. 읽은 내용을 그대로 다시 클라이언트에게 보낸다.
// 5. 연결이 끊길 때까지 2~4를 반복한다.
void echo(int connfd)   // connfd: 현재 연결된 클라이언트 소켓 fd
{
    size_t n;   // 읽은 바이트 수
    char buf[MAXLINE];  // 읽은 데이터를 담는 버퍼
    rio_t rio;  // connfd에서 안정적으로 읽기 위한 RIO 상태 구조체

    // rio가 앞으로 connfd에서 데이터를 읽도록 초기화한다.
    Rio_readinitb(&rio, connfd);

    // Rio_readlineb:
    // - 한 줄(\n 포함)을 읽어 buf에 저장
    // - 읽은 바이트 수를 n에 반환
    // - 클라이언트가 연결을 끊으면 0 반환
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        // n은 글자 수가 아니라 바이트 수다.
        // 예: "abc\n"은 4바이트, "안녕\n"은 UTF-8 기준 7바이트
        printf("server received %d bytes\n", (int)n);

        // buf에 읽어 둔 내용을 n바이트 그대로 같은 connfd로 보낸다.
        // 그래서 클라이언트는 자기가 보낸 줄을 다시 받게 된다.
        Rio_writen(connfd, buf, n);
    }
}
