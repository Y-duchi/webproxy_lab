/*
 * csapp.c - CS:APP 3판에서 제공하는 보조 함수 모음
 *
 * 2016-10 수정:
 *   - sio_ltoa가 음수를 빠뜨리던 버그 수정
 *
 * 2016-02 수정:
 *   - open_clientfd / open_listenfd가 더 자연스럽게 실패하도록 개선
 *
 * 2014-08 수정:
 *   - open_clientfd / open_listenfd를 재진입 가능하고
 *     프로토콜 독립적인 버전으로 교체
 *   - 오래된 inet_ntoa / inet_aton 대신
 *     inet_ntop / inet_pton 추가
 *
 * 2014-07 수정:
 *   - 재진입 가능한 SIO(signal-safe I/O) 루틴 추가
 *
 * 2013-04 수정:
 *   - rio_readlineb의 경계 조건 버그 수정
 *   - rio_readnb의 중복 EINTR 검사 제거
 */
/* 책 예제 시작: csapp.c */
#include "csapp.h"

/*
 * ================================================================
 * 한국어 읽기 가이드
 * ================================================================
 *
 * 이 파일은 CS:APP 책에서 제공하는 "자주 쓰는 시스템 함수 모음집"이다.
 * 핵심 목적은 2가지다.
 *
 * 1. 원래 복잡하고 에러 처리가 귀찮은 시스템 콜을
 *    "실패하면 바로 종료하는 안전한 감싼 함수(래퍼)" 형태로 감싼다.
 *
 * 2. 네트워크 프로그래밍에서 자주 쓰는 RIO(강건한 입출력),
 *    소켓 생성/연결/리스닝 도우미 함수(helper)를 한곳에 모아둔다.
 *
 * 이 파일을 읽을 때는 아래 순서로 보면 이해가 쉽다.
 *
 * [1] 에러 처리 함수
 *     unix_error / posix_error / gai_error / app_error / dns_error
 *     - "무슨 종류의 에러인지"에 맞춰 메시지를 출력하고 종료한다.
 *
 * [2] 프로세스 / 시그널 래퍼
 *     Fork / Execve / Wait / Kill / Signal ...
 *     - 실제 시스템 함수를 한 번 감싸서,
 *       실패하면 매번 if 문을 쓰지 않아도 되게 만든다.
 *
 * [3] SIO(signal-safe I/O)
 *     sio_puts / sio_putl / sio_error
 *     - 시그널 핸들러 안에서도 비교적 안전하게 쓸 수 있는 출력 함수들이다.
 *
 * [4] 일반 I/O / 메모리 / 디렉터리 / stdio wrapper
 *     Open / Read / Write / Malloc / Fopen ...
 *     - "원래 함수 호출 + 실패 시 종료" 패턴이 반복된다.
 *
 * [5] 소켓 / 주소 변환 / DNS
 *     Socket / Bind / Listen / Accept / Connect
 *     Getaddrinfo / Getnameinfo / Inet_pton / Inet_ntop
 *     - 네트워크 주소를 만들고, 연결하고, 사람이 읽는 문자열로 바꾸는 도구들이다.
 *
 * [6] 스레드 / 세마포어
 *     Pthread_* / Sem_init / P / V
 *     - 멀티스레드와 동기화 도구의 래퍼다.
 *
 * [7] RIO(robust I/O)
 *     rio_readn / rio_writen / rio_read / rio_readnb / rio_readlineb
 *     Rio_readn / Rio_writen / Rio_readlineb ...
 *     - 네트워크에서 "덜 읽히거나", 시그널에 끊겨도
 *       다시 시도하면서 안정적으로 읽고 쓰게 해준다.
 *
 * [8] 클라이언트/서버 도우미 함수
 *     open_clientfd / open_listenfd
 *     Open_clientfd / Open_listenfd
 *     - 소켓 생성 + 주소 찾기 + connect/bind/listen 절차를 묶어둔
 *       네트워크 프로그래밍의 핵심 도우미 함수다.
 *
 * 이 주석용 파일에서는 특히 [7], [8]을 자세히 보면
 * echo 서버 / tiny / proxy 코드를 읽기가 쉬워진다.
 *
 * ------------------------------------------------
 * 반복되는 래퍼 함수를 읽는 공통 공식
 * ------------------------------------------------
 * 대부분의 래퍼 함수는 아래 3단계로 읽으면 된다.
 *
 * 1. 원래 시스템 함수 또는 라이브러리 함수를 호출한다.
 * 2. 반환값이 실패인지 검사한다.
 * 3. 실패면 unix_error / posix_error / gai_error로 종료한다.
 *
 * 예를 들어 Open, Read, Write, Socket, Bind, Listen, Accept 같은 함수는
 * 전부 "원래 함수 호출 -> 실패 검사 -> 결과 반환" 패턴의 반복이다.
 * 그래서 아래에서는 복잡한 함수(RIO, open_clientfd, open_listenfd)를 더 자세히
 * 설명하고, 단순 래퍼는 짧고 반복 가능한 방식으로 주석을 달아둔다.
 */

/**************************
 * 에러 처리 함수들
 **************************/
/* 책 예제 시작: errorfuns */
/* 책 예제 시작: unixerror */
void unix_error(char *msg) /* Unix 계열 에러 출력 함수 */
{
    /* errno에 들어 있는 최근 Unix 에러를 사람이 읽는 문자열로 바꿔 출력한다. */
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    /* 에러가 난 상태이므로 프로그램을 바로 끝낸다. */
    exit(0);
}
/* 책 예제 끝: unixerror */

void posix_error(int code, char *msg) /* POSIX 반환 코드 기반 에러 출력 함수 */
{
    /* pthread 계열처럼 errno 대신 "반환값 자체"로 에러 코드를 주는 함수용이다. */
    fprintf(stderr, "%s: %s\n", msg, strerror(code));
    exit(0);
}

void gai_error(int code, char *msg) /* getaddrinfo 계열 에러 출력 함수 */
{
    /* getaddrinfo/getnameinfo는 gai_strerror로 해석해야 한다. */
    fprintf(stderr, "%s: %s\n", msg, gai_strerror(code));
    exit(0);
}

void app_error(char *msg) /* 프로그램 로직 에러 출력 함수 */
{
    /* 운영체제 에러가 아니라, 우리가 만든 프로그램 로직 에러를 출력할 때 쓴다. */
    fprintf(stderr, "%s\n", msg);
    exit(0);
}
/* 책 예제 끝: errorfuns */

void dns_error(char *msg) /* 오래된 DNS 함수용 에러 출력 함수 */
{
    /* 오래된 DNS 함수(gethostbyname/gethostbyaddr)용 에러 출력 함수다. */
    fprintf(stderr, "%s\n", msg);
    exit(0);
}


/*********************************************
 * Unix 프로세스 제어 함수 래퍼
 ********************************************/

/* 책 예제 시작: forkwrapper */
pid_t Fork(void) 
{
    pid_t pid;

    /* 자식 프로세스를 하나 만든다. */
    if ((pid = fork()) < 0)
	unix_error("Fork error");
    /* 부모에게는 자식 pid, 자식에게는 0이 반환된다. */
    return pid;
}
/* 책 예제 끝: forkwrapper */

void Execve(const char *filename, char *const argv[], char *const envp[]) 
{
    /* 현재 프로세스 이미지를 새 프로그램으로 통째로 바꾼다. */
    if (execve(filename, argv, envp) < 0)
	unix_error("Execve error");
}

/* 책 예제 시작: wait */
pid_t Wait(int *status) 
{
    pid_t pid;

    /* 종료된 자식 프로세스 하나를 기다린다. */
    if ((pid  = wait(status)) < 0)
	unix_error("Wait error");
    return pid;
}
/* 책 예제 끝: wait */

/*
 * 함수 요약: waitpid를 안전하게 감싼다.
 * - 특정 자식 프로세스 또는 조건에 맞는 자식을 기다린다.
 * - 실패하면 즉시 종료한다.
 */
pid_t Waitpid(pid_t pid, int *iptr, int options) 
{
    pid_t retpid;

    if ((retpid  = waitpid(pid, iptr, options)) < 0) 
	unix_error("Waitpid error");
    return(retpid);
}

/* 책 예제 시작: kill */
void Kill(pid_t pid, int signum) 
{
    int rc;

    if ((rc = kill(pid, signum)) < 0)
	unix_error("Kill error");
}
/* 책 예제 끝: kill */

/*
 * 함수 요약: pause를 호출해 시그널이 올 때까지 잠든다.
 */
void Pause() 
{
    (void)pause();
    return;
}

/*
 * 함수 요약: 지정한 초만큼 잠든다.
 * - 실패하면 종료하고, 아니면 남은 시간을 반환한다.
 */
unsigned int Sleep(unsigned int secs) 
{
    unsigned int rc;

    if ((rc = sleep(secs)) < 0)
	unix_error("Sleep error");
    return rc;
}

/*
 * 함수 요약: 일정 시간 뒤 SIGALRM을 예약한다.
 */
unsigned int Alarm(unsigned int seconds) {
    return alarm(seconds);
}
 
/*
 * 함수 요약: 프로세스 그룹을 설정한다.
 */
void Setpgid(pid_t pid, pid_t pgid) {
    int rc;

    if ((rc = setpgid(pid, pgid)) < 0)
	unix_error("Setpgid error");
    return;
}

/*
 * 함수 요약: 현재 프로세스 그룹 ID를 반환한다.
 */
pid_t Getpgrp(void) {
    return getpgrp();
}

/************************************
 * Unix 시그널 함수 래퍼
 ***********************************/

/* 책 예제 시작: sigaction */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    /* "이 시그널이 오면 어떤 함수를 실행할지"를 등록한다. */
    action.sa_handler = handler;  
    /* 현재 처리 중인 시그널과 같은 종류의 시그널은 잠시 막는다. */
    sigemptyset(&action.sa_mask); /* 현재 처리 중인 종류의 시그널은 잠시 막는다 */
    /* 시그널 때문에 끊긴 시스템 콜을 가능하면 자동 재시작한다. */
    action.sa_flags = SA_RESTART; /* 가능하면 끊긴 시스템 콜을 자동 재시작한다 */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    /* 예전 핸들러를 돌려주면, 필요할 때 복원할 수 있다. */
    return (old_action.sa_handler);
}
/* 책 예제 끝: sigaction */

/*
 * 함수 요약: 시그널 마스크를 바꾼다.
 * - 어떤 시그널을 막을지/풀지를 제어할 때 쓴다.
 */
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
	unix_error("Sigprocmask error");
    return;
}

/*
 * 함수 요약: 비어 있는 시그널 집합을 만든다.
 */
void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
	unix_error("Sigemptyset error");
    return;
}

/*
 * 함수 요약: 모든 시그널이 들어 있는 집합을 만든다.
 */
void Sigfillset(sigset_t *set)
{ 
    if (sigfillset(set) < 0)
	unix_error("Sigfillset error");
    return;
}

/*
 * 함수 요약: 시그널 집합에 특정 시그널을 추가한다.
 */
void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
	unix_error("Sigaddset error");
    return;
}

/*
 * 함수 요약: 시그널 집합에서 특정 시그널을 제거한다.
 */
void Sigdelset(sigset_t *set, int signum)
{
    if (sigdelset(set, signum) < 0)
	unix_error("Sigdelset error");
    return;
}

/*
 * 함수 요약: 특정 시그널이 집합 안에 들어 있는지 검사한다.
 */
int Sigismember(const sigset_t *set, int signum)
{
    int rc;
    if ((rc = sigismember(set, signum)) < 0)
	unix_error("Sigismember error");
    return rc;
}

/*
 * 함수 요약: 시그널 마스크를 잠깐 바꾸고 시그널이 올 때까지 잠든다.
 */
int Sigsuspend(const sigset_t *set)
{
    int rc = sigsuspend(set); /* 이 함수는 항상 -1을 반환한다 */
    if (errno != EINTR)
        unix_error("Sigsuspend error");
    return rc;
}

/*************************************************************
 * SIO(signal-safe I/O) 패키지
 * - 시그널 핸들러 안에서도 비교적 안전하게 쓸 수 있는
 *   단순한 재진입 가능 출력 함수들
 *************************************************************/

/* SIO 내부 전용 보조 함수들 */

/* 책 예제 시작: sioprivate */
/* sio_reverse - 문자열을 뒤집는다 (K&R 코드 기반) */
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* sio_ltoa - long 값을 b진수 문자열로 바꾼다 (K&R 코드 기반) */
static void sio_ltoa(long v, char s[], int b) 
{
    int c, i = 0;
    int neg = v < 0;

    if (neg)
	v = -v;

    do {  
        s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);

    if (neg)
	s[i++] = '-';

    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen - 문자열 길이를 반환한다 (K&R 코드 기반) */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
/* 책 예제 끝: sioprivate */

/* 외부에서 직접 부를 수 있는 SIO 함수들 */
/* 책 예제 시작: siopublic */

ssize_t sio_puts(char s[]) /* 문자열을 표준 출력으로 보낸다 */
{
    return write(STDOUT_FILENO, s, sio_strlen(s));
}

ssize_t sio_putl(long v) /* long 값을 문자열로 바꿔 출력한다 */
{
    char s[128];
    
    sio_ltoa(v, s, 10); /* K&R 스타일 itoa 아이디어를 이용해 10진 문자열로 변환 */
    return sio_puts(s);
}

void sio_error(char s[]) /* 에러 문자열을 출력하고 즉시 종료한다 */
{
    sio_puts(s);
    _exit(1);
}
/* 책 예제 끝: siopublic */

/*******************************
 * SIO 함수용 래퍼
 ******************************/
ssize_t Sio_putl(long v)
{
    ssize_t n;
  
    if ((n = sio_putl(v)) < 0)
	sio_error("Sio_putl error");
    return n;
}

ssize_t Sio_puts(char s[])
{
    ssize_t n;
  
    if ((n = sio_puts(s)) < 0)
	sio_error("Sio_puts error");
    return n;
}

void Sio_error(char s[])
{
    sio_error(s);
}

/********************************
 * Unix I/O 함수 래퍼
 ********************************/

/*
 * 함수 요약: open의 안전한 래퍼다.
 * - 파일을 열고, 실패하면 종료한다.
 */
int Open(const char *pathname, int flags, mode_t mode) 
{
    int rc;

    if ((rc = open(pathname, flags, mode))  < 0)
	unix_error("Open error");
    return rc;
}

/*
 * 함수 요약: read의 안전한 래퍼다.
 * - fd에서 count바이트까지 읽는다.
 */
ssize_t Read(int fd, void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = read(fd, buf, count)) < 0) 
	unix_error("Read error");
    return rc;
}

/*
 * 함수 요약: write의 안전한 래퍼다.
 * - buf 내용을 fd에 쓴다.
 */
ssize_t Write(int fd, const void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = write(fd, buf, count)) < 0)
	unix_error("Write error");
    return rc;
}

off_t Lseek(int fildes, off_t offset, int whence) 
{
    off_t rc;

    if ((rc = lseek(fildes, offset, whence)) < 0)
	unix_error("Lseek error");
    return rc;
}

/*
 * 함수 요약: close의 안전한 래퍼다.
 * - 파일 디스크립터를 닫는다.
 */
void Close(int fd) 
{
    int rc;

    if ((rc = close(fd)) < 0)
	unix_error("Close error");
}

int Select(int  n, fd_set *readfds, fd_set *writefds,
	   fd_set *exceptfds, struct timeval *timeout) 
{
    int rc;

    if ((rc = select(n, readfds, writefds, exceptfds, timeout)) < 0)
	unix_error("Select error");
    return rc;
}

int Dup2(int fd1, int fd2) 
{
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
	unix_error("Dup2 error");
    return rc;
}

void Stat(const char *filename, struct stat *buf) 
{
    if (stat(filename, buf) < 0)
	unix_error("Stat error");
}

void Fstat(int fd, struct stat *buf) 
{
    if (fstat(fd, buf) < 0)
	unix_error("Fstat error");
}

/*********************************
 * 디렉터리 함수 래퍼
 *********************************/

DIR *Opendir(const char *name) 
{
    DIR *dirp = opendir(name); 

    if (!dirp)
        unix_error("opendir error");
    return dirp;
}

struct dirent *Readdir(DIR *dirp)
{
    struct dirent *dep;
    
    errno = 0;
    dep = readdir(dirp);
    if ((dep == NULL) && (errno != 0))
        unix_error("readdir error");
    return dep;
}

int Closedir(DIR *dirp) 
{
    int rc;

    if ((rc = closedir(dirp)) < 0)
        unix_error("closedir error");
    return rc;
}

/***************************************
 * 메모리 매핑 함수 래퍼
 ***************************************/
void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) 
{
    void *ptr;

    if ((ptr = mmap(addr, len, prot, flags, fd, offset)) == ((void *) -1))
	unix_error("mmap error");
    return(ptr);
}

void Munmap(void *start, size_t length) 
{
    if (munmap(start, length) < 0)
	unix_error("munmap error");
}

/***************************************************
 * 동적 메모리 할당 함수 래퍼
 ***************************************************/

void *Malloc(size_t size) 
{
    void *p;

    if ((p  = malloc(size)) == NULL)
	unix_error("Malloc error");
    return p;
}

void *Realloc(void *ptr, size_t size) 
{
    void *p;

    if ((p  = realloc(ptr, size)) == NULL)
	unix_error("Realloc error");
    return p;
}

void *Calloc(size_t nmemb, size_t size) 
{
    void *p;

    if ((p = calloc(nmemb, size)) == NULL)
	unix_error("Calloc error");
    return p;
}

void Free(void *ptr) 
{
    free(ptr);
}

/******************************************
 * 표준 I/O 함수 래퍼
 ******************************************/
void Fclose(FILE *fp) 
{
    if (fclose(fp) != 0)
	unix_error("Fclose error");
}

FILE *Fdopen(int fd, const char *type) 
{
    FILE *fp;

    if ((fp = fdopen(fd, type)) == NULL)
	unix_error("Fdopen error");

    return fp;
}

char *Fgets(char *ptr, int n, FILE *stream) 
{
    char *rptr;

    if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
	app_error("Fgets error");

    return rptr;
}

FILE *Fopen(const char *filename, const char *mode) 
{
    FILE *fp;

    if ((fp = fopen(filename, mode)) == NULL)
	unix_error("Fopen error");

    return fp;
}

void Fputs(const char *ptr, FILE *stream) 
{
    if (fputs(ptr, stream) == EOF)
	unix_error("Fputs error");
}

size_t Fread(void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    size_t n;

    if (((n = fread(ptr, size, nmemb, stream)) < nmemb) && ferror(stream)) 
	unix_error("Fread error");
    return n;
}

void Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    if (fwrite(ptr, size, nmemb, stream) < nmemb)
	unix_error("Fwrite error");
}


/****************************
 * 소켓 인터페이스 래퍼
 ****************************/

/*
 * 함수 요약: socket의 안전한 래퍼다.
 * - 소켓 하나를 생성한다.
 */
int Socket(int domain, int type, int protocol) 
{
    int rc;

    if ((rc = socket(domain, type, protocol)) < 0)
	unix_error("Socket error");
    return rc;
}

void Setsockopt(int s, int level, int optname, const void *optval, int optlen) 
{
    int rc;

    if ((rc = setsockopt(s, level, optname, optval, optlen)) < 0)
	unix_error("Setsockopt error");
}

void Bind(int sockfd, struct sockaddr *my_addr, int addrlen) 
{
    int rc;

    if ((rc = bind(sockfd, my_addr, addrlen)) < 0)
	unix_error("Bind error");
}

void Listen(int s, int backlog) 
{
    int rc;

    if ((rc = listen(s,  backlog)) < 0)
	unix_error("Listen error");
}

/*
 * 함수 요약: accept의 안전한 래퍼다.
 * - 대기 중인 연결 하나를 받아 새 연결 소켓을 만든다.
 */
int Accept(int s, struct sockaddr *addr, socklen_t *addrlen) 
{
    int rc;

    if ((rc = accept(s, addr, addrlen)) < 0)
	unix_error("Accept error");
    return rc;
}

/*
 * 함수 요약: connect의 안전한 래퍼다.
 * - 클라이언트 소켓을 서버 주소에 연결한다.
 */
void Connect(int sockfd, struct sockaddr *serv_addr, int addrlen) 
{
    int rc;

    if ((rc = connect(sockfd, serv_addr, addrlen)) < 0)
	unix_error("Connect error");
}

/*******************************
 * 프로토콜 독립 주소 처리 래퍼
 *******************************/
/* 책 예제 시작: getaddrinfo */
void Getaddrinfo(const char *node, const char *service, 
                 const struct addrinfo *hints, struct addrinfo **res)
{
    int rc;

    if ((rc = getaddrinfo(node, service, hints, res)) != 0) 
        gai_error(rc, "Getaddrinfo error");
}
/* 책 예제 끝: getaddrinfo */

/*
 * 함수 요약: 바이너리 형태의 주소를 사람이 읽는 문자열(host, service)로 바꾼다.
 */
void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, 
                 size_t hostlen, char *serv, size_t servlen, int flags)
{
    int rc;

    if ((rc = getnameinfo(sa, salen, host, hostlen, serv, 
                          servlen, flags)) != 0) 
        gai_error(rc, "Getnameinfo error");
}

/*
 * 함수 요약: getaddrinfo가 할당한 주소 리스트 메모리를 해제한다.
 */
void Freeaddrinfo(struct addrinfo *res)
{
    freeaddrinfo(res);
}

/*
 * 함수 요약: 바이너리 IP 주소를 사람이 읽는 문자열로 바꾼다.
 */
void Inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (!inet_ntop(af, src, dst, size))
        unix_error("Inet_ntop error");
}

/*
 * 함수 요약: 사람이 읽는 IP 문자열을 바이너리 주소로 바꾼다.
 */
void Inet_pton(int af, const char *src, void *dst) 
{
    int rc;

    rc = inet_pton(af, src, dst);
    if (rc == 0)
	app_error("inet_pton error: invalid dotted-decimal address");
    else if (rc < 0)
        unix_error("Inet_pton error");
}

/*******************************************
 * DNS 인터페이스 래퍼
 *
 * 참고:
 * 아래 함수들은 스레드 안전(thread-safe)하지 않아서 오래된 방식으로 취급된다.
 * 가능하면 getaddrinfo / getnameinfo를 쓰는 편이 좋다.
 ***********************************/

/* 책 예제 시작: gethostbyname */
struct hostent *Gethostbyname(const char *name) 
{
    struct hostent *p;

    if ((p = gethostbyname(name)) == NULL)
	dns_error("Gethostbyname error");
    return p;
}
/* 책 예제 끝: gethostbyname */

struct hostent *Gethostbyaddr(const char *addr, int len, int type) 
{
    struct hostent *p;

    if ((p = gethostbyaddr(addr, len, type)) == NULL)
	dns_error("Gethostbyaddr error");
    return p;
}

/************************************************
 * pthread 스레드 제어 함수 래퍼
 ************************************************/

void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp, 
		    void * (*routine)(void *), void *argp) 
{
    int rc;

    if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
	posix_error(rc, "Pthread_create error");
}

void Pthread_cancel(pthread_t tid) {
    int rc;

    if ((rc = pthread_cancel(tid)) != 0)
	posix_error(rc, "Pthread_cancel error");
}

void Pthread_join(pthread_t tid, void **thread_return) {
    int rc;

    if ((rc = pthread_join(tid, thread_return)) != 0)
	posix_error(rc, "Pthread_join error");
}

/* 책 예제 시작: detach */
void Pthread_detach(pthread_t tid) {
    int rc;

    if ((rc = pthread_detach(tid)) != 0)
	posix_error(rc, "Pthread_detach error");
}
/* 책 예제 끝: detach */

void Pthread_exit(void *retval) {
    pthread_exit(retval);
}

pthread_t Pthread_self(void) {
    return pthread_self();
}
 
void Pthread_once(pthread_once_t *once_control, void (*init_function)()) {
    pthread_once(once_control, init_function);
}

/*******************************
 * POSIX 세마포어 래퍼
 *******************************/

/*
 * 함수 요약: 세마포어를 초기화한다.
 */
void Sem_init(sem_t *sem, int pshared, unsigned int value) 
{
    if (sem_init(sem, pshared, value) < 0)
	unix_error("Sem_init error");
}

/*
 * 함수 요약: 세마포어를 wait(P)해서 자원을 하나 얻는다.
 */
void P(sem_t *sem) 
{
    if (sem_wait(sem) < 0)
	unix_error("P error");
}

/*
 * 함수 요약: 세마포어를 post(V)해서 자원을 하나 반납한다.
 */
void V(sem_t *sem) 
{
    if (sem_post(sem) < 0)
	unix_error("V error");
}

/****************************************
 * RIO 패키지 - 강건한(robust) I/O 함수들
 ****************************************/

/*
 * rio_readn - 버퍼 없이 n바이트를 최대한 안정적으로 읽는다
 */
/* 책 예제 시작: rio_readn */
ssize_t rio_readn(int fd, void *usrbuf, size_t n) 
{
    /* 아직 읽어야 하는 바이트 수 */
    size_t nleft = n;
    /* 이번 read가 실제로 읽어온 바이트 수 */
    ssize_t nread;
    /* 사용자 버퍼 안에서 "다음에 써야 할 위치" */
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nread = read(fd, bufp, nleft)) < 0) {
            /* 시그널 때문에 read가 중간에 끊겼으면, 실패로 보지 않고 다시 시도한다. */
	    if (errno == EINTR) /* 시그널 핸들러에서 잠깐 끊긴 경우 */
		nread = 0;      /* read를 다시 시도하도록 0으로 둔다 */
	    else
		return -1;      /* read가 설정한 errno를 유지한 채 실패 반환 */ 
	} 
        /* 0은 EOF. 즉 상대가 더 이상 보낼 데이터가 없다는 뜻이다. */
	else if (nread == 0)
	    break;              /* 파일 끝 또는 연결 종료 */
        /* 읽은 만큼 남은 양을 줄인다. */
	nleft -= nread;
        /* 버퍼 포인터를 앞으로 이동해서, 다음 데이터가 이어붙게 한다. */
	bufp += nread;
    }
    /* 원래 원했던 양에서 "못 읽은 양"을 빼면 실제 읽은 양이 된다. */
    return (n - nleft);         /* 실제 읽은 바이트 수를 0 이상으로 반환 */
}
/* 책 예제 끝: rio_readn */

/*
 * rio_writen - 버퍼 없이 n바이트를 최대한 안정적으로 쓴다
 */
/* 책 예제 시작: rio_writen */
ssize_t rio_writen(int fd, void *usrbuf, size_t n) 
{
    /* 아직 써야 하는 바이트 수 */
    size_t nleft = n;
    /* 이번 write가 실제로 쓴 바이트 수 */
    ssize_t nwritten;
    /* 사용자 버퍼에서 "다음에 읽어갈 위치" */
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            /* 시그널 때문에 잠깐 끊긴 경우는 다시 시도한다. */
	    if (errno == EINTR)  /* 시그널 핸들러에서 잠깐 끊긴 경우 */
		nwritten = 0;    /* write를 다시 시도하도록 0으로 둔다 */
	    else
		return -1;       /* write가 설정한 errno를 유지한 채 실패 반환 */
	}
        /* 쓴 만큼 남은 양을 줄인다. */
	nleft -= nwritten;
        /* 다음에 써야 할 위치로 버퍼 포인터를 옮긴다. */
	bufp += nwritten;
    }
    /* 끝까지 다 쓰면 원래 요청한 n을 반환한다. */
    return n;
}
/* 책 예제 끝: rio_writen */


/*
 * rio_read - Unix read()를 내부 버퍼와 함께 쓰기 쉽게 감싼 핵심 내부 함수
 *    사용자가 원하는 양 n과 내부 버퍼 잔량 중 작은 만큼만 꺼내 준다.
 *    내부 버퍼가 비어 있으면 read()로 다시 채운다.
 */
/* 책 예제 시작: rio_read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    /*
     * 내부 버퍼에 읽어둔 데이터가 없으면
     * 커널의 read()를 다시 호출해서 내부 버퍼를 채운다.
     */
    while (rp->rio_cnt <= 0) {  /* 내부 버퍼가 비어 있으면 다시 채운다 */
	rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, 
			   sizeof(rp->rio_buf));
	if (rp->rio_cnt < 0) {
            /* EINTR이면 read가 신호 때문에 끊긴 것뿐이라 다시 돈다. */
	    if (errno != EINTR) /* 시그널 핸들러에서 잠깐 끊긴 경우 */
		return -1;
	}
        /* 0이면 EOF이므로 더 읽을 데이터가 없다. */
	else if (rp->rio_cnt == 0)  /* 파일 끝 또는 연결 종료 */
	    return 0;
	else 
            /* 새로 채운 내부 버퍼의 시작 위치로 포인터를 되돌린다. */
	    rp->rio_bufptr = rp->rio_buf; /* 내부 버퍼 포인터를 맨 앞으로 되돌린다 */
    }

    /* 사용자 요청량 n과 내부 버퍼 잔량 중 더 작은 만큼만 복사한다. */
    cnt = n;          
    if (rp->rio_cnt < n)   
	cnt = rp->rio_cnt;
    /* 내부 버퍼 -> 사용자 버퍼 복사 */
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    /* 내부 버퍼 안에서 다음 읽기 위치를 앞으로 이동 */
    rp->rio_bufptr += cnt;
    /* 내부 버퍼 잔량 감소 */
    rp->rio_cnt -= cnt;
    /* 이번에 실제로 사용자에게 넘겨준 바이트 수 */
    return cnt;
}
/* 책 예제 끝: rio_read */

/*
 * rio_readinitb - 파일 디스크립터와 읽기 버퍼를 연결하고 초기화한다
 */
/* 책 예제 시작: rio_readinitb */
void rio_readinitb(rio_t *rp, int fd) 
{
    /* 이 rio 구조체가 앞으로 어떤 fd를 읽을지 기록 */
    rp->rio_fd = fd;  
    /* 아직 내부 버퍼에 읽어온 데이터가 없으므로 0 */
    rp->rio_cnt = 0;  
    /* 내부 버퍼 포인터를 버퍼 맨 앞으로 초기화 */
    rp->rio_bufptr = rp->rio_buf;
}
/* 책 예제 끝: rio_readinitb */

/*
 * rio_readnb - 내부 버퍼를 이용해 n바이트를 안정적으로 읽는다
 */
/* 책 예제 시작: rio_readnb */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
    /* 아직 더 읽어야 하는 양 */
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;
    
    while (nleft > 0) {
        /* 실제 읽기는 rio_read가 담당하고, 우리는 반복 제어만 한다. */
	if ((nread = rio_read(rp, bufp, nleft)) < 0) 
            return -1;          /* read가 설정한 errno를 유지한 채 실패 반환 */ 
	else if (nread == 0)
	    break;              /* 파일 끝 또는 연결 종료 */
	nleft -= nread;
	bufp += nread;
    }
    /* 요청량에서 못 읽은 양을 빼서 실제 읽은 양 반환 */
    return (n - nleft);         /* 실제 읽은 바이트 수를 0 이상으로 반환 */
}
/* 책 예제 끝: rio_readnb */

/*
 * rio_readlineb - 내부 버퍼를 이용해 텍스트 한 줄을 안정적으로 읽는다
 */
/* 책 예제 시작: rio_readlineb */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    /* n은 "현재까지 몇 글자를 버퍼에 넣었는지" 추적한다. */
    int n, rc;
    /* 한 글자씩 읽기 위해 임시 문자 하나를 둔다. */
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) { 
        /* 내부 버퍼에서 글자 1개를 꺼내 온다. */
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            /* 읽은 글자를 사용자 버퍼에 적는다. */
	    *bufp++ = c;
            /* 줄바꿈을 만나면 "한 줄"을 다 읽은 것이므로 종료한다. */
	    if (c == '\n') {
                n++;
     		break;
            }
	} else if (rc == 0) {
            /* 맨 처음부터 EOF면 읽은 줄이 없는 것이므로 0 반환 */
	    if (n == 1)
		return 0; /* 아무 데이터도 못 읽은 상태에서 EOF */
	    else
                /* 중간까지는 읽었는데 EOF면, 지금까지 읽은 줄을 반환 */
		break;    /* 일부는 읽었지만 도중에 EOF */
	} else
	    return -1;	  /* 읽기 에러 */
    }
    /* C 문자열로 쓰기 위해 마지막에 널 종료 문자를 붙인다. */
    *bufp = 0;
    /* 실제 읽은 문자의 수를 돌려준다. 널 종료 문자는 제외한다. */
    return n-1;
}
/* 책 예제 끝: rio_readlineb */

/**********************************
 * Robust I/O 함수 래퍼
 **********************************/
ssize_t Rio_readn(int fd, void *ptr, size_t nbytes) 
{
    ssize_t n;
  
    if ((n = rio_readn(fd, ptr, nbytes)) < 0)
	unix_error("Rio_readn error");
    return n;
}

void Rio_writen(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n)
	unix_error("Rio_writen error");
}

void Rio_readinitb(rio_t *rp, int fd)
{
    rio_readinitb(rp, fd);
} 

ssize_t Rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
	unix_error("Rio_readnb error");
    return rc;
}

ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
	unix_error("Rio_readlineb error");
    return rc;
} 

/********************************
 * 클라이언트/서버 helper 함수
 ********************************/
/*
 * open_clientfd - <hostname, port> 서버에 연결하고,
 *     읽기/쓰기가 가능한 클라이언트 소켓 디스크립터를 반환한다.
 *     이 함수는 재진입 가능하고, 프로토콜 독립적으로 동작한다.
 *
 *     에러 시 반환값:
 *       -2 : getaddrinfo 에러
 *       -1 : 그 외 에러 (errno 설정됨)
 */
/* 책 예제 시작: open_clientfd */
int open_clientfd(char *hostname, char *port) {
    int clientfd, rc;
    struct addrinfo hints, *listp, *p;

    /* getaddrinfo에 넘길 힌트 구조체를 0으로 초기화한다. */
    memset(&hints, 0, sizeof(struct addrinfo));
    /* 우리는 TCP 연결을 원하므로 SOCK_STREAM을 사용한다. */
    hints.ai_socktype = SOCK_STREAM;  /* TCP 연결을 열기 위한 스트림 소켓 */
    /* port 인자는 문자열이지만 실제론 숫자 포트라는 뜻 */
    hints.ai_flags = AI_NUMERICSERV;  /* 포트 인자를 숫자로 해석 */
    /* 현재 머신 환경에 맞는 주소 체계만 추천받는다. */
    hints.ai_flags |= AI_ADDRCONFIG;  /* 현재 환경에 맞는 주소 체계만 사용 */
    /* hostname:port에 대해 가능한 서버 주소 후보들을 받아온다. */
    if ((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port, gai_strerror(rc));
        return -2;
    }
  
    /* 후보 주소들을 하나씩 시도하면서 실제로 연결 가능한 주소를 찾는다. */
    for (p = listp; p; p = p->ai_next) {
        /* 현재 후보 주소 형식에 맞는 소켓을 하나 만든다. */
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
            continue; /* 소켓 생성 실패: 다음 후보 주소 시도 */

        /* 그 후보 주소로 실제 연결을 시도한다. */
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) 
            break; /* 연결 성공 */
        /* 연결 실패 시, 방금 만든 소켓은 닫고 다음 후보로 넘어간다. */
        if (close(clientfd) < 0) { /* connect 실패: 현재 소켓을 닫고 다음 후보로 이동 */
            fprintf(stderr, "open_clientfd: close failed: %s\n", strerror(errno));
            return -1;
        } 
    } 

    /* 주소 후보 리스트 메모리 해제 */
    freeaddrinfo(listp);
    /* p가 NULL이면 모든 후보에서 connect가 실패한 것이다. */
    if (!p) /* 모든 후보 주소에서 connect 실패 */
        return -1;
    else    /* 마지막으로 시도한 connect가 성공 */
        /* 성공적으로 연결된 소켓 디스크립터를 반환 */
        return clientfd;
}
/* 책 예제 끝: open_clientfd */

/*
 * open_listenfd - 주어진 포트에서 연결을 기다리는 listening socket을 만들고 반환한다.
 *     이 함수는 재진입 가능하고, 프로토콜 독립적으로 동작한다.
 *
 *     에러 시 반환값:
 *       -2 : getaddrinfo 에러
 *       -1 : 그 외 에러 (errno 설정됨)
 */
/* 책 예제 시작: open_listenfd */
int open_listenfd(char *port) 
{
    struct addrinfo hints, *listp, *p;
    int listenfd, rc, optval=1;

    /* getaddrinfo 힌트 초기화 */
    memset(&hints, 0, sizeof(struct addrinfo));
    /* TCP 서버 소켓을 만들 것이므로 SOCK_STREAM */
    hints.ai_socktype = SOCK_STREAM;             /* TCP 연결을 받기 위한 스트림 소켓 */
    /* NULL 주소도 허용해서 "내 모든 IP에서 대기"할 수 있게 한다. */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* 내 모든 IP 주소에서 대기 가능 */
    /* 서비스 이름이 아니라 숫자 포트를 쓰겠다는 뜻 */
    hints.ai_flags |= AI_NUMERICSERV;            /* 포트 인자를 숫자로 해석 */
    /* 이 포트에서 바인드 가능한 주소 후보들을 받아온다. */
    if ((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(rc));
        return -2;
    }

    /* 여러 주소 후보 중 bind 가능한 주소를 찾는다. */
    for (p = listp; p; p = p->ai_next) {
        /* 현재 후보에 맞는 소켓 생성 */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
            continue;  /* 소켓 생성 실패: 다음 후보 주소 시도 */

        /* 재실행 직후에도 같은 포트를 다시 bind하기 쉽게 해 준다. */
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int));

        /* 이 소켓을 특정 주소/포트에 묶는다. */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break; /* 연결 성공 */
        /* bind 실패 시 현재 소켓을 닫고 다음 후보를 본다. */
        if (close(listenfd) < 0) { /* bind 실패: 현재 소켓을 닫고 다음 후보로 이동 */
            fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
            return -1;
        }
    }


    /* 주소 후보 리스트 메모리 해제 */
    freeaddrinfo(listp);
    /* 모든 주소 후보에서 bind가 실패하면 서버를 열 수 없다. */
    if (!p) /* 어떤 주소 후보로도 bind하지 못함 */
        return -1;

    /* 이제 이 소켓을 "손님을 기다리는 소켓" 상태로 바꾼다. */
    if (listen(listenfd, LISTENQ) < 0) {
        close(listenfd);
	return -1;
    }
    /* 손님을 받을 준비가 끝난 listening socket 반환 */
    return listenfd;
}
/* 책 예제 끝: open_listenfd */

/****************************************************
 * 재진입 가능 / 프로토콜 독립 helper의 래퍼
 ****************************************************/
/*
 * 함수 요약: open_clientfd의 안전한 래퍼다.
 * - 실패하면 unix_error로 종료한다.
 */
int Open_clientfd(char *hostname, char *port) 
{
    int rc;

    if ((rc = open_clientfd(hostname, port)) < 0) 
	unix_error("Open_clientfd error");
    return rc;
}

/*
 * 함수 요약: open_listenfd의 안전한 래퍼다.
 * - 실패하면 unix_error로 종료한다.
 */
int Open_listenfd(char *port) 
{
    int rc;

    if ((rc = open_listenfd(port)) < 0)
	unix_error("Open_listenfd error");
    return rc;
}

/* 책 예제 끝: csapp.c */
