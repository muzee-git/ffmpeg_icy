#include "avformat.h"
#include "url.h"
#include "internal.h"
#include "avio_internal.h"
#include "os_support.h"
#include "libavutil/avstring.h"

#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

enum {READABLE=0, WRITABLE=1};

static int wait_fd(int fd, int can_write, URLContext *h)
{
    int timeout = 50, ret = 0;
    struct pollfd p = {fd, can_write == WRITABLE ? POLLOUT : POLLIN, 0};
    while(timeout--) {
        if (ff_check_interrupt(&h->interrupt_callback)) 
        {
            return -1;
        }
        ret = poll(&p, 1, 100);
        if (ret > 0)
            break;
    }
    return ret;
}

struct StupidHTTPContext {
    int fd;
    uint8_t buf[4096];
    int buf_length;
    int http_status;
    uint8_t location[4096];
};

static struct StupidHTTPContext* createContext(URLContext *h, int sock_fd){
    struct StupidHTTPContext* ctx = (struct StupidHTTPContext*) av_malloc(sizeof(struct StupidHTTPContext));
    ctx->fd = sock_fd;
    ctx->buf_length = 0;
    ctx->http_status = 0;
    memset(ctx->buf, 0, sizeof(ctx->buf));
    memset(ctx->location, 0, sizeof(ctx->location));
    h->priv_data = ctx;
    h->is_streamed = 1;
    return ctx;
}

static int read_line(const char *source, const int offset, const int length, char *output, int output_length)
{
    int i, last_index = -1, next_offset = -1;
    for(i = offset; i < length; i++)
    {
        if(source[i] == '\r')
        {
            if(i + 1 < length && source[i + 1] == '\n' && (i - 1 >= 0))
            {
                last_index = i - 1;
                next_offset = i + 2;

                /* next_offset must be between 0 to (length -1) */
                if(next_offset >= length)
                {
                    next_offset = -1;
                }
                break ;
            }
        }
        /* 
         * TODO if only see '\r' no more data in buffer, we will do ....
         */
    }

    memset(output, 0, output_length);
    if(last_index >=0 && offset <= last_index)
    {
        int l = 0;
        for(i = offset; i <= last_index; i++, l++)
        {
            if(l < output_length)
            {
                output[l] = source[i];
            }
            else
            {
                av_log(NULL, AV_LOG_WARNING, "get line out of range at index: %d\n", l);
            }
        }
    }
    return next_offset;
}


static struct StupidHTTPContext* stupidhttp_open_ctx(URLContext *h, const char *uri)
{
    struct addrinfo hints, *ai;
    int port, fd = -1;
    char hostname[1024], proto[1024], path[1024];
    char portstr[10];
    char request[1024];

    av_url_split(proto, sizeof(proto), NULL, 0, hostname, sizeof(hostname),
            &port, path, sizeof(path), uri);

    av_log(h, AV_LOG_WARNING, "%s => proto: %s, hostname: %s, path: %s\n", uri, proto, hostname, path);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof(portstr), "%d", port == -1 ? 80 : port);

    int ret = getaddrinfo(hostname, portstr, &hints, &ai);
    if(ret)
    {
        av_log(h, AV_LOG_WARNING, "Failed to resolve hostname %s:%s (err: %d)\n", hostname, portstr, ret);
        return NULL;
    }

    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    ret = connect(fd, ai->ai_addr, ai->ai_addrlen);
    freeaddrinfo(ai);
    if(ret < 0)
    {
        ret = wait_fd(fd, WRITABLE, h);
        if(ret <=0)
        {
            av_log(h, AV_LOG_WARNING, "cannot get writable fd\n");
            goto fail_after_connect;
        }
    }

    /* make request string */
    request[0] = '\0';
    av_strlcatf(request, sizeof(request), "GET %s HTTP/1.0\r\n", path);
    av_strlcatf(request, sizeof(request), "Host: %s\r\n", hostname);
    av_strlcatf(request, sizeof(request), "User-Agent: %s\r\n", "MPlayer SVN-r34197-4.2.1");
    av_strlcatf(request, sizeof(request), "Connection: close\r\n");
    av_strlcatf(request, sizeof(request), "\r\n");
    av_log(h, AV_LOG_WARNING, "raw-http-request: %s", request);
    ret = send(fd, request, strlen(request), 0);
    if(ret < 0)
    {
        av_log(h, AV_LOG_WARNING, "sent http request failure\n");
        goto fail_after_connect;
    }
    
    struct StupidHTTPContext* http_ctx = createContext(h, fd);
    uint8_t buf[1024];
    int buf_len = 0;
    char line[1024];
    int found_http_body = 0;
    int read_zero_limit_count_down = 3;
    while(read_zero_limit_count_down > 0)
    {
        ret = wait_fd(fd, READABLE, h);
        if(ret <=0)
        {
            break ;
        }

        int offset = 0, next_offset = 0;
        memset(buf, 0, sizeof(buf));
        buf_len = recv(http_ctx->fd, buf, sizeof(buf) - 1, 0);
        av_log(h, AV_LOG_WARNING, "buf length: %d\n", buf_len);
        if(buf_len == 0)
        {
            read_zero_limit_count_down --;
        }
        int line_number = 0;
        while(next_offset >= 0)
        {
            line_number ++;
            next_offset = read_line(buf, offset, buf_len, line, sizeof(line));
            offset = next_offset;
            av_log(h, AV_LOG_WARNING, "http response: >>>%s<<<\n", line);

            if(line_number == 1)
            {
                /* parse http status code */
                if(av_stristr(line, "http/1.") != 0)
                {
                    int c = 0;
                    char *ss = NULL;
                    char *ee = NULL;
                    for(c=0; c<strlen(line) - 1; c++)
                    {
                        if(line[c + 1] == ' ' && ss == NULL)
                        {
                            ss = &line[c + 1];
                            continue ;
                        }

                        if(line[c + 1] == ' ' && ee == NULL)
                        {
                            ee = &line[c + 1];
                        }
                    }
                    if(ss != NULL && ee != NULL)
                    {
                        int st = strtol(ss, &ee, 10);
                        av_log(h, AV_LOG_WARNING, "http status: %ld\n", st);
                        http_ctx->http_status = st;
                    }
                }
            }
            else
            {
                char *location = av_stristr(line, "location:");
                if(location != NULL)
                {

                    int c;
                    char *s = NULL;
                    for(c = strlen("location:"); c < strlen(line); c++)
                    {
                        if(s == NULL && line[c] != ' ')
                        {
                            s = &line[c];
                            av_strlcpy(http_ctx->location, s, sizeof(http_ctx->location));
                            av_log(h, AV_LOG_WARNING, "found location header: %s\n", http_ctx->location);
                            break ;
                        }
                    }
                }
            }

            if(strlen(line) == 0)
            {
                found_http_body = 1;
                break ;
            }
        }

        if(found_http_body && http_ctx->http_status == 200)
        {
            int i, buf_cnt = 0;
            av_log(h, AV_LOG_WARNING, "write tail buffer to context offset: %d, length: %d\n", next_offset, buf_len);
            if(next_offset != -1)
            {
                for(i = next_offset; i < buf_len; i++)
                {
                    /*
                     * av_log(h, AV_LOG_WARNING, "%d >>%d<<\n", i, buf[i]);
                     */
                    http_ctx->buf[i - next_offset] = buf[i];
                    buf_cnt ++;
                }
                http_ctx->buf_length = buf_cnt;
            }
            break ;
        }
    }

    if(found_http_body && read_zero_limit_count_down > 0)
    {
        av_log(h, AV_LOG_WARNING, "body found.\n");
        return http_ctx;
    }

    av_log(h, AV_LOG_WARNING, "no body found.\n");
    av_free(http_ctx);
    return NULL;

fail_after_connect:
    if(fd > 0)
    {
        closesocket(fd);
    }
    if(http_ctx != NULL)
    {
        av_free(http_ctx);
    } 
    return NULL;

}

static int stupidhttp_open(URLContext *h, const char *uri, int flags)
{
    struct StupidHTTPContext *http_ctx = NULL;
    int reopen_count = 10;
    http_ctx = stupidhttp_open_ctx(h, uri);
    while(reopen_count -- > 0)
    {
        if(http_ctx == NULL)
            return -1;
        if(http_ctx->http_status == 200)
            return 0;
        if(http_ctx->http_status == 302 && strlen(http_ctx->location) != 0)
        {
            av_log(h, AV_LOG_WARNING, "redirect to %s\n", http_ctx->location);
            char loc[4096];
            av_strlcpy(loc, http_ctx->location, sizeof(loc));
            av_free(http_ctx);
            http_ctx = stupidhttp_open_ctx(h, loc);
        }
    }

    return 0;
}

static int stupidhttp_read(URLContext *h, uint8_t *buf, int size)
{
    struct StupidHTTPContext* http_ctx = h->priv_data;
    int length = 0;
    if(http_ctx->buf_length != 0)
    {
        av_log(h, AV_LOG_WARNING, "flush preload buffer in ctx\n");
        memcpy(buf, http_ctx->buf, http_ctx->buf_length);
        length = http_ctx->buf_length;
        http_ctx->buf_length = 0;
        return length;
    }

    int fd = http_ctx->fd;
    int ret = wait_fd(fd, READABLE, h);
    if(ret <=0)
    {
        return -1;
    }
    ret = recv(fd, buf, size, 0);
    return ret;
}

static int stupidhttp_close(URLContext *h)
{
    struct StupidHTTPContext* ctx = h->priv_data;
    int fd = ctx->fd;
    if(fd > 0)
        closesocket(fd);
    av_free(ctx);
    return 0;
}

URLProtocol ff_stupidhttp_protocol = {
    .name      = "stupidhttp",
    .url_open  = stupidhttp_open,
    .url_read  = stupidhttp_read,
    .url_close = stupidhttp_close,
};
