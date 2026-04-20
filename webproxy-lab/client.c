#include "csapp.h" // CS:APP에서 제공한 소켓/RIO 함수 선언들

int main(int argc, char **argv)
{
    int clientfd;      // 서버와 연결된 클라이언트 소켓 fd
    char buf[MAXLINE]; // 키보드 입력과 서버 응답을 담는 버퍼
    rio_t rio;         // clientfd에서 안정적으로 읽기 위한 RIO 상태 구조체

    if (argc != 3) // 실행 형식이 ./client <host> <port> 가 아니면
    {
        // argv[0] = "./client"
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(1);
    }

    clientfd = Open_clientfd(argv[1], argv[2]);
    // argv[1]: 서버 host 예: "localhost"
    // argv[2]: 서버 port 예: "8080"
    // 이 함수가 socket + connect를 해서
    // 서버와 실제로 연결된 소켓 fd를 clientfd에 넣어 준다.

    Rio_readinitb(&rio, clientfd);
    // rio가 앞으로 clientfd에서 데이터를 읽도록 초기화한다.

    printf("Connected to %s:%s\n", argv[1], argv[2]);
    printf("Type a message and press Enter. Ctrl+D exits.\n");

    while (Fgets(buf, MAXLINE, stdin) != NULL)
    // buf: 읽은 글자를 저장할 통
    // MAXLINE: 최대 얼마나 읽을지
    // stdin: 어디서 읽을지. 여기서는 키보드

    // Fgets(...)가 키보드에서 한 줄 읽는다
    // 읽은 내용을 buf에 넣는다
    // 읽기에 성공하면 NULL이 아니니까 반복문 안으로 들어간다
    // 더 이상 읽을 게 없으면 NULL이 되어서 반복 종료
    // Ctrl+D를 누르면 EOF가 들어와서 반복 종료
    {
        Rio_writen(clientfd, buf, strlen(buf));
        // buf에 들어 있는 입력을 서버로 그대로 보낸다. -> 내가 방금 키보드에 친 한 줄을 서버로 보낸다
        // strlen(buf)만큼 보내므로 보통 '\n'도 같이 전송된다. -> 문자열의 길이를 잼

        if (Rio_readlineb(&rio, buf, MAXLINE) == 0)
        // 서버가 보낸 응답 한 줄을 다시 buf에 읽어온다.
        // 0이면 서버가 연결을 끊은 것이다.
        {
            printf("Server closed the connection.\n");
            break;
        }

        printf("Echo from server: %s", buf);
        // 서버가 돌려준 내용을 화면에 출력
    }

    Close(clientfd); // 연결 종료
    return 0;        // 프로그램 정상 종료
}
