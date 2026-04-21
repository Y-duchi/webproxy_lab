/* $begin tinymain */
/*
 * tiny.c
 * 아주 작은 HTTP/1.0 웹 서버의 핵심 흐름만 담은 예제 파일이다.
 * 이 서버는 GET 요청만 처리하고, 정적 파일 또는 CGI 프로그램을 실행해 응답한다.
 */

#include "csapp.h" // CS:APP 책에서 제공하는 소켓, 파일, I/O 도우미 함수를 사용하기 위해 헤더를 포함한다.

void doit(int fd);                                         // 클라이언트와 연결된 소켓 하나를 받아서 HTTP 요청 하나를 처리한다.
void read_requesthdrs(rio_t *rp);                          // 첫 줄을 제외한 나머지 HTTP 헤더를 읽어 넘긴다.
int parse_uri(char *uri, char *filename, char *cgiargs);   // URI를 보고 정적 요청인지 동적 요청인지 판단한다.
void serve_static(int fd, char *filename, int filesize);   // 정적 파일을 읽어서 브라우저로 보내는 함수다.
void get_filetype(char *filename, char *filetype);         // 파일 확장자를 보고 Content-type 문자열을 만든다.
void serve_dynamic(int fd, char *filename, char *cgiargs); // CGI 프로그램을 실행해서 동적 응답을 만든다.
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg); // 오류가 생겼을 때 브라우저에 HTTP 에러 응답을 보낸다.

int main(int argc, char **argv) // 프로그램이 시작되면 가장 먼저 main 함수가 실행된다.
{
  int listenfd, connfd;                  // listenfd는 연결을 기다리는 소켓이고, connfd는 실제 클라이언트와 통신하는 소켓이다.
  char hostname[MAXLINE], port[MAXLINE]; // 접속한 클라이언트의 호스트 이름과 포트 번호를 문자열로 저장할 공간이다.
  socklen_t clientlen;                   // 클라이언트 주소 구조체의 크기를 저장하는 변수다.
  struct sockaddr_storage clientaddr;    // 클라이언트 주소 정보를 담는 구조체다.

  if (argc != 2) // 실행 인자가 프로그램 이름과 포트 번호, 이렇게 2개가 아니면 잘못 실행한 것이다.
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // 사용법을 표준 에러로 출력해서 사용자에게 알려준다.
    exit(1);                                        // 잘못 실행했으므로 프로그램을 바로 종료한다.
  }

  listenfd = Open_listenfd(argv[1]); // 사용자가 준 포트 번호로 서버용 리스닝 소켓을 만든다.
  while (1)                          // 웹 서버는 종료될 때까지 계속 요청을 받아야 하므로 무한 반복한다.
  {
    clientlen = sizeof(clientaddr); // Accept가 주소 정보를 채울 수 있도록 구조체 크기를 먼저 넣어 둔다.
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // 새로운 클라이언트 연결을 받아서 통신용 소켓 connfd를 얻는다.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);                                                // 방금 접속한 클라이언트의 주소를 사람이 읽기 쉬운 호스트명과 포트 문자열로 바꾼다.
    printf("Accepted connection from (%s, %s)\n", hostname, port); // 어떤 클라이언트가 접속했는지 서버 콘솔에 출력한다.
    doit(connfd);                                                  // 이 클라이언트가 보낸 HTTP 요청 하나를 실제로 처리한다.
    Close(connfd);                                                 // 요청 처리가 끝났으므로 이 클라이언트와의 연결을 닫는다.
  }
}

void doit(int fd) // 클라이언트 요청 한 건을 읽고, 분석하고, 적절한 응답을 보내는 핵심 함수다.
{
  int is_static;                                                      // 정적 콘텐츠 요청이면 1, 동적 콘텐츠 요청이면 0 같은 구분 결과를 담는다.
  struct stat sbuf;                                                   // 요청한 파일의 존재 여부, 크기, 권한 같은 파일 정보를 저장한다.
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 요청 첫 줄에서 메서드, URI, HTTP 버전을 꺼내 저장한다.
  char filename[MAXLINE], cgiargs[MAXLINE];                           // 실제 파일 경로와 CGI 인자를 따로 저장할 버퍼다.
  rio_t rio;                                                          // Robust I/O를 사용해서 소켓에서 안전하게 한 줄씩 읽기 위한 버퍼 구조체다.

  Rio_readinitb(&rio, fd);                       // fd 소켓을 rio 버퍼와 연결해서 읽기 준비를 한다.
  Rio_readlineb(&rio, buf, MAXLINE);             // 요청의 첫 줄(예: GET /index.html HTTP/1.0)을 읽어 buf에 저장한다.
  printf("Request headers:\n");                  // 지금부터 요청 내용을 출력한다는 안내 문구를 서버 콘솔에 찍는다.
  printf("%s", buf);                             // 방금 읽은 요청 첫 줄 자체를 서버 콘솔에 출력한다.
  sscanf(buf, "%s %s %s", method, uri, version); // 첫 줄에서 메서드, URI, 버전을 공백 기준으로 잘라 각각 저장한다.

  if (strcasecmp(method, "GET")) // method가 GET과 다르면 0이 아닌 값이 나오므로 이 서버가 처리할 수 없는 요청이다.
  {
    clienterror(fd, method, "501", "Not implemented",
                "Tiny does not implement this method"); // 브라우저에 501 에러를 보내서 GET만 지원한다고 알려준다.
    return;                                             // 더 진행할 수 없으므로 현재 요청 처리를 여기서 끝낸다.
  }

  read_requesthdrs(&rio);                        // 첫 줄 뒤에 붙은 나머지 요청 헤더들을 읽어서 소비한다.
  is_static = parse_uri(uri, filename, cgiargs); // URI를 분석해서 정적 요청인지 동적 요청인지 판단하고, 필요한 경로 정보도 만든다.

  if (stat(filename, &sbuf) < 0) // filename에 해당하는 파일이 없거나 접근할 수 없으면 음수가 반환된다.
  {
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file"); // 브라우저에 404 에러를 보내서 요청한 대상을 찾지 못했다고 알려준다.
    return;                                      // 파일이 없으므로 더 진행하지 않고 함수에서 빠져나온다.
  }

  if (is_static) // parse_uri 결과가 참이면 정적 파일을 보내야 하는 요청이다.
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) // 일반 파일이 아니거나, 서버 프로세스에 읽기 권한이 없으면 전송할 수 없다.
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file"); // 브라우저에 403 에러를 보내서 접근이 허용되지 않는다고 알려준다.
      return;                                     // 권한 문제가 있으므로 요청 처리를 중단한다.
    }
    serve_static(fd, filename, sbuf.st_size); // 파일 내용과 HTTP 헤더를 만들어서 브라우저로 전송한다.
  }
  else // is_static이 거짓이면 CGI 프로그램을 실행하는 동적 요청으로 본다.
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) // 동적 콘텐츠는 실행 파일이어야 하므로 실행 권한이 있는지 확인한다.
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program"); // 실행 대상에 접근할 수 없으면 403 에러를 응답한다.
      return;                                     // 접근 불가이므로 함수 실행을 멈춘다.
    }
    serve_dynamic(fd, filename, cgiargs); // CGI 프로그램을 실행해서 나온 결과를 브라우저에 전달한다.
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) // 에러가 났을 때 브라우저로 HTTP 에러 응답을 보내는 함수다.
{
  char buf[MAXLINE], body[MAXBUF]; // buf는 HTTP 헤더 한 줄씩 담는 용도, body는 브라우저에 보여줄 HTML 본문이다.

  sprintf(body, "<html><title>Tiny Error</title>");                     // HTML 문서 시작 부분과 브라우저 탭 제목을 body에 처음 써 넣는다.
  sprintf(body + strlen(body), "<body bgcolor=\"ffffff\">\r\n");        // 지금까지 만든 문자열 맨 뒤에 body 태그를 이어 붙인다.
  sprintf(body + strlen(body), "%s: %s\r\n", errnum, shortmsg);         // 예: "404: Not found" 같은 핵심 에러 문장을 본문에 추가한다.
  sprintf(body + strlen(body), "<p>%s: %s\r\n", longmsg, cause);        // 에러가 왜 났는지 더 자세한 설명을 한 줄 추가한다.
  sprintf(body + strlen(body), "<hr><em>The Tiny Web server</em>\r\n"); // 페이지 아래쪽에 구분선과 서버 이름 문구를 붙인다.

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);          // HTTP 상태줄을 만든다. 예: "HTTP/1.0 404 Not found"
  Rio_writen(fd, buf, strlen(buf));                              // 방금 만든 상태줄을 클라이언트(브라우저)에게 보낸다.
  sprintf(buf, "Content-type: text/html\r\n");                   // 지금부터 보내는 본문 데이터가 HTML 형식이라는 헤더를 만든다.
  Rio_writen(fd, buf, strlen(buf));                              // Content-type 헤더를 브라우저에 보낸다.
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); // HTML 본문 길이와 헤더 끝 표시(\r\n\r\n)를 만든다.
  Rio_writen(fd, buf, strlen(buf));                              // Content-length 헤더까지 브라우저에 보낸다.

  Rio_writen(fd, body, strlen(body)); // 마지막으로 실제 HTML 에러 페이지 내용을 브라우저에 보낸다.
}

void read_requesthdrs(rio_t *rp) // 요청 헤더를 빈 줄이 나올 때까지 읽는다.
{
  char buf[MAXLINE]; // 헤더 한 줄을 저장할 버퍼다.

  Rio_readlineb(rp, buf, MAXLINE); // 첫 번째 헤더 줄을 읽는다.
  while (strcmp(buf, "\r\n"))      // 빈 줄이 아니면 아직 헤더가 남아 있다는 뜻이다.
  {
    printf("%s", buf);               // 읽은 헤더를 출력한다.
    Rio_readlineb(rp, buf, MAXLINE); // 다음 헤더 줄을 읽는다.
  }
  return; // 헤더를 다 읽었으니 함수 종료.
}

int parse_uri(char *uri, char *filename, char *cgiargs) // 요청한 URI를 보고 정적/동적 콘텐츠를 구분하는 함수
{
  char *ptr; // URI 안에서 '?' 위치를 가리킬 포인터 변수

  if (!strstr(uri, "cgi-bin"))
  {                        // URI에 "cgi-bin"이 없으면 정적 콘텐츠 요청으로 본다
    strcpy(cgiargs, "");   // 정적 콘텐츠는 CGI 인자가 없으므로 빈 문자열로 만든다
    strcpy(filename, "."); // filename을 현재 디렉터리(".")부터 시작하게 만든다
    strcat(filename, uri); // "." 뒤에 uri를 붙여서 실제 파일 경로를 만든다. 예: "./home.html"

    if (uri[strlen(uri) - 1] == '/') // URI가 '/'로 끝나면 사용자가 디렉터리만 요청한 상태다
      strcat(filename, "home.html"); // 기본 페이지 파일인 home.html을 뒤에 붙인다

    return 1; // 1을 반환해서 "정적 콘텐츠다"라고 알려준다
  }
  else
  {                         // URI에 "cgi-bin"이 있으면 동적 콘텐츠 요청으로 본다
    ptr = strchr(uri, '?'); // URI 안에서 '?' 문자의 위치를 찾는다

    if (ptr)
    {                           // '?'가 있으면, 뒤에 CGI 인자가 붙어 있다는 뜻이다
      strcpy(cgiargs, ptr + 1); // '?' 다음 문자부터 끝까지를 cgiargs에 복사한다
      *ptr = '\0';              // '?' 자리에 문자열 끝 표시를 넣어서 uri를 파일 경로까지만 남긴다
    }
    else
    {
      strcpy(cgiargs, ""); // '?'가 없으면 CGI 인자가 없으므로 빈 문자열로 만든다
    }

    strcpy(filename, "."); // filename을 현재 디렉터리(".")부터 시작하게 만든다
    strcat(filename, uri); // "." 뒤에 uri를 붙여서 실제 CGI 프로그램 경로를 만든다. 예: "./cgi-bin/adder"

    return 0; // 0을 반환해서 "동적 콘텐츠다"라고 알려준다
  }
}

void get_filetype(char *filename, char *filetype) // 파일 이름을 보고 Content-type 값을 정하는 함수다.
{
  // strstr(전체문자열, 찾을문자열)
  if (strstr(filename, ".html"))   // 파일 이름에 ".html"이 들어 있으면 HTML 파일로 본다
    strcpy(filetype, "text/html"); // 브라우저가 HTML 문서로 해석하도록 타입을 저장한다

  else if (strstr(filename, ".gif")) // 파일 이름에 ".gif"가 들어 있으면 GIF 이미지로 본다
    strcpy(filetype, "image/gif");   // 브라우저가 GIF 이미지로 해석하도록 타입을 저장한다

  else if (strstr(filename, ".jpg")) // 파일 이름에 ".jpg"가 들어 있으면 JPG 이미지로 본다
    strcpy(filetype, "image/jpeg");  // 브라우저가 JPEG 이미지로 해석하도록 타입을 저장한다

  else                              // 위 조건들에 해당하지 않으면
    strcpy(filetype, "text/plain"); // 일단 일반 텍스트 파일로 처리한다
}

void serve_static(int fd, char *filename, int filesize) // 정적 파일을 브라우저에 보내는 함수다.
{
  int srcfd;                                  // 보낼 파일을 열었을 때 돌려받는 파일 디스크립터다.
  char *srcp, filetype[MAXLINE], buf[MAXBUF]; // srcp는 파일 내용을 가리킬 포인터, filetype은 MIME 타입, buf는 응답 헤더를 담는 버퍼다.

  get_filetype(filename, filetype);                          // 파일 이름을 보고 Content-type 값을 정한다. 예: text/html, image/jpeg
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                                  // 요청이 성공했음을 알리는 상태줄을 버퍼 맨 앞에 먼저 쓴다.
  sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");            // 현재 문자열 끝 뒤에 Server 헤더를 이어 붙인다.
  sprintf(buf + strlen(buf), "Connection: close\r\n");                  // 현재 문자열 끝 뒤에 연결 종료 헤더를 이어 붙인다.
  sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);       // 현재 문자열 끝 뒤에 본문 길이 헤더를 이어 붙인다.
  sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype);     // 현재 문자열 끝 뒤에 타입 헤더와 헤더 끝 빈 줄을 이어 붙인다.
  Rio_writen(fd, buf, strlen(buf));                          // 지금까지 만든 HTTP 응답 헤더 전체를 클라이언트에게 보낸다.

  srcfd = Open(filename, O_RDONLY, 0);                        // 전송할 파일을 읽기 전용으로 연다.
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일 내용을 메모리에 연결해서 쉽게 읽을 수 있게 만든다. 파일을 통째로 읽기 쉽게 메모리에 붙인다
  Close(srcfd);                                               // 파일 내용은 이미 메모리에 연결했으므로 파일 디스크립터는 닫아도 된다.
  Rio_writen(fd, srcp, filesize);                             // 파일 내용을 클라이언트에게 그대로 보낸다.
  Munmap(srcp, filesize);                                     // 메모리에 연결했던 파일 내용을 해제한다.
}

void serve_dynamic(int fd, char *filename, char *cgiargs) // CGI 프로그램을 실행해서 동적인 웹 응답을 만드는 함수다.
{
  char buf[MAXLINE], *emptylist[] = {NULL}; // buf는 HTTP 헤더를 잠깐 담는 문자열 버퍼이고, emptylist는 실행 파일에 따로 넘길 명령줄 인자가 없다는 뜻이다.

  sprintf(buf, "HTTP/1.0 200 OK\r\n");         // 브라우저에게 "요청이 정상 처리되기 시작했다"는 상태줄을 만든다.
  Rio_writen(fd, buf, strlen(buf));            // 방금 만든 상태줄을 클라이언트 소켓으로 보낸다.
  sprintf(buf, "Server: Tiny Web Server\r\n"); // 어떤 서버가 응답하는지 알려주는 헤더 한 줄을 만든다.
  Rio_writen(fd, buf, strlen(buf));            // Server 헤더를 클라이언트에게 보낸다.

  if (Fork() == 0) // 새 프로세스를 하나 만들고, 그중 자식 프로세스만 이 블록 안으로 들어온다.
  {
    setenv("QUERY_STRING", cgiargs, 1);   // URL의 ? 뒤에 붙은 인자들을 QUERY_STRING 환경변수에 저장해서 CGI 프로그램이 읽을 수 있게 만든다.
    Dup2(fd, STDOUT_FILENO);              // 원래 터미널로 가던 표준출력을 클라이언트 소켓으로 바꿔서 printf 결과가 브라우저로 가게 만든다.
    Execve(filename, emptylist, environ); // filename에 있는 CGI 프로그램으로 현재 자식 프로세스를 바꿔 실행한다.
  }

  Wait(NULL); // 부모 프로세스는 자식 프로세스가 CGI 실행을 끝낼 때까지 기다렸다가 정리한다.
}
