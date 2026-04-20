#include "csapp.h"

void echo(int connfd);  // connfd: 연결된 클라이언트 소켓 fd

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

    listenfd = Open_listenfd(argv[1]);  // argv[1] 포트에서 접속을 기다리는 리스닝 소켓 생성

    while (1)   // 요청이 들어올 때까지 무한 반복
    {
        clientlen = sizeof(clientaddr);   // Accept에 넘길 주소 버퍼 크기
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);   // 새 연결을 받아 connfd에 저장, 상대 주소는 clientaddr에 저장

        Getnameinfo((SA *)&clientaddr, clientlen,  // clientaddr를 사람이 읽는 host/port 문자열로 변환
                    hostname, MAXLINE, port, MAXLINE, 0);   
        printf("Connected to (%s, %s)\n", hostname, port);

        echo(connfd);   // 이 연결에서 한 줄씩 읽어 그대로 다시 보냄
        Close(connfd);  // 클라이언트가 끊으면 연결 종료
    }
}

// 클라이언트가 보낸 한 줄을 읽어 그대로 돌려보내는 함수
void echo(int connfd)   // connfd: 현재 연결된 클라이언트 소켓 fd
{
    size_t n;   // 읽은 바이트 수
    char buf[MAXLINE];  // 읽은 데이터를 담는 버퍼
    rio_t rio;  // connfd에서 안정적으로 읽기 위한 RIO 상태 구조체

    Rio_readinitb(&rio, connfd);    // rio를 connfd와 연결

    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) // 한 줄씩 읽고, 연결이 끊길 때까지 반복
    {
        printf("server received %d bytes\n", (int)n);   // 글자 수가 아니라 바이트 수
        Rio_writen(connfd, buf, n); // 읽은 내용을 그대로 다시 보냄
    }
}
