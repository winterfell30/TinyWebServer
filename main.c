#include "helper.h"
#include "sbuf.h"

#define M_GET 1
#define M_POST 2
#define M_HEAD 3
#define M_NONE 0

const int MAXThreads = 4;
const int buffersize = 1000;

sbuf_t sbuf;

void doit(int fd);
void read_requesthdrs(rio_t *rp, char *post_content, int mtd);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filetype, int mtd);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, int mtd);
void clienterror(int fd, char *cause, char *errnum,
        char *shortmsg, char *longmsg, int mtd);
void *thread(void *vargp);


int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    //检查输入合法性
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, buffersize);          //初始化buf

    for (int i = 0; i < MAXThreads; i++)
        Pthread_create(&tid, NULL, thread, NULL);

    while (1)
    {
        clientlen = sizeof(port);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        sbuf_insert(&sbuf, connfd);        //将描述符插入buffer
    }
    sbuf_deinit(&sbuf);                    //释放buf
    return 0;
}

void *thread(void *vargp)
{
    Pthread_detach(pthread_self());        //分离当前线程
    while (1)
    {
        int connfd = sbuf_remove(&sbuf);   //从buffer中取描述符
        doit(connfd);
        Close(connfd);                     //关闭描述符连接
    }
    return NULL;
}

//处理http事务
//支持GET POST HEAD
void doit(int fd)
{
    int is_static, mtd;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    
    char post_content[MAXLINE];

    rio_t rio;

    //读取请求和header
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (!strcmp(method, "GET")) mtd = M_GET;
    else if (!strcmp(method, "POST")) mtd = M_POST;
    else if (!strcmp(method, "HEAD")) mtd = M_HEAD;
    else mtd = M_NONE;
    
    if (mtd == M_NONE)
    {
        clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method", mtd);
        return;
    }
    read_requesthdrs(&rio, post_content, mtd);

    //解析请求
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0)
    {
        clienterror(fd, filename, "404", "Not found",
                "Tiny coundn't find this file", mtd);
        return;
    }

    if (is_static)
    {
        if (mtd == M_POST)
        {
            clienterror(fd, filename, "405", "Method Not Allowed",
                    "Request method POST is not allowed for the url", mtd);
            return;
        }
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        {
            clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't read the file", mtd);
            return;
        }
        serve_static(fd, filename, sbuf.st_size, mtd);
    }
    else
    {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        {
            clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't run the GET program", mtd);
            return;
        }
        if (mtd == M_POST) strcpy(cgiargs, post_content);
        serve_dynamic(fd, filename, cgiargs, mtd);
    }
}

//将错误信息返回给客户端
void clienterror(int fd, char *cause, char *errnum,
        char *shortmsg, char *longmsg, int mtd)
{
    char buf[MAXLINE], body[MAXBUF];

    //设置错误页面的body
    sprintf(body, "<HTML><TITLE>Web Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Tiny Web server</em>\r\n", body);

    //输出错误页面
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    if (mtd != M_HEAD)
    {
        Rio_writen(fd, buf, strlen(buf));
        Rio_writen(fd, body, strlen(body));
    }
}

//get则忽略报文post读取
void read_requesthdrs(rio_t *rp, char *post_content, int mtd)
{
    char buf[MAXLINE];
    int contentlen = 0;

    Rio_readlineb(rp, buf, MAXLINE);
    if (mtd == M_POST && strstr(buf, "Content-Length: ") == buf)
        contentlen = atoi(buf + strlen("Content-Length: "));
    while (strcmp(buf, "\r\n"))
    {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("233 %s", buf);

        //取得post的参数长度
        if (mtd == M_POST && strstr(buf, "Content-Length: ") == buf)
            contentlen = atoi(buf + strlen("Content-Length: "));
    }
    if (mtd == M_POST)
    {
        contentlen = Rio_readnb(rp, post_content, contentlen);
        post_content[contentlen] = '\0';
        printf("POST_CONTENT: %s\n", post_content);
    }
    return;
}

//判断请求内容
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    //static server
    if (!strstr(uri, "cgi-bin"))
    {
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri)-1] == '/')
            strcat(filename, "home.html");
        return 1;
    }
    else  //dynamic server
    {
        ptr = index(uri, '?');
        if (ptr)
        {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        }
        else
        {
            strcpy(cgiargs, "");
        }
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

//静态服务
//可以使用HTML、纯文本文件、GIF、JPG格式到文件
void serve_static(int fd, char *filename, int filesize, int mtd)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXLINE];

    //给客户端发送响应报头
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));

    if (mtd == M_HEAD) return;

    //给客户端发送响应报文
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

//从文件名中获取文件类型
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, int mtd)
{
    char buf[MAXLINE], *emptylist[] = {NULL};

    //返回前一部分报文
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (mtd == M_HEAD) return;

    if (Fork() == 0)
    {
        //正式的server会在这里设置所有到CGI环境
        setenv("QUERY_STRING", cgiargs, 1); //这里只设置QUERY_STRING
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
    Wait(NULL);
}
