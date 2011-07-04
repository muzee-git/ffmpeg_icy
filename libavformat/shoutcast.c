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

static int wait_fd(int fd, int can_write)
{
    int timeout = 50, ret = 0;
    struct pollfd p = {fd, can_write == WRITABLE ? POLLOUT : POLLIN, 0};
    while(timeout--) {
        if (url_interrupt_cb()) 
        {
            return -1;
        }
        ret = poll(&p, 1, 100);
        if (ret > 0)
            break;
    }
    return ret;
}

struct ICYContext {
    int fd;
};

static int shoutcast_open(URLContext *h, const char *uri, int flags)
{
    struct addrinfo hints, *ai;
    int port, fd = -1;
    char hostname[1024], proto[1024], path[1024];
    char portstr[10];

    av_url_split(proto, sizeof(proto), NULL, 0, hostname, sizeof(hostname),
            &port, path, sizeof(path), uri);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof(portstr), "%d", port == -1 ? 80 : port);

    int ret = getaddrinfo(hostname, portstr, &hints, &ai);
    if(ret)
    {
        av_log(h, AV_LOG_ERROR, "Failed to resolve hostname %s:%s (err: %d)\n", hostname, portstr, ret);
        return -1;
    }

    int timeout = 50;
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    ret = connect(fd, ai->ai_addr, ai->ai_addrlen);
    freeaddrinfo(ai);
    if(ret < 0)
    {
        ret = wait_fd(fd, WRITABLE);
        if(ret <=0)
        {
            av_log(h, AV_LOG_ERROR, "cannot get writable fd\n");
            goto fail_after_connect;
        }
    }

    /* check ICY header */
    char http_req[]="GET / HTTP/1.1\r\n\r\n";
    ret = send(fd, http_req, strlen(http_req), 0);
    if(ret < 0)
    {
        av_log(h, AV_LOG_ERROR, "sent http request failure\n");
        goto fail_after_connect;
    }

    char buf[128];
    ret = wait_fd(fd, READABLE);
    if(ret <=0)
    {
        goto fail_after_connect;
    }
    ret = recv(fd, buf, 4, 0);
    buf[4] = '\0';
    av_log(h, AV_LOG_INFO, "first chars >>>%s<<<\n", buf);

    if(ret < 0)
    {
        av_log(h, AV_LOG_ERROR, "read http response failure\n");
        goto fail_after_connect;
    }

    int skip_bytes = 1024;
    if( tolower(buf[0]) == 'i' &&
            tolower(buf[1]) == 'c' &&
            tolower(buf[2]) == 'y')
    {
        while(skip_bytes >0)
        {
            ret = wait_fd(fd, READABLE);
            if(ret <=0)
            {
                goto fail_after_connect;
            }
            ret = recv(fd, buf, 128, 0);
            if(av_stristr(buf, "\r\n\r\n") != NULL)
            {
                av_log(h, AV_LOG_INFO, "ICY headers skipped.\n");
                skip_bytes = 0;
                break ;
            }
            if(ret < 0)
            {
                break;
            }
            skip_bytes -= ret;
        }
    }

    if(skip_bytes <= 0)
    {
        struct ICYContext* ctx = (struct ICYContext*) av_malloc(sizeof(struct ICYContext));
        ctx->fd = fd;
        h->priv_data = ctx;
        h->is_streamed = 1;
        return 0;
    }

fail_after_connect:
    if(fd > 0)
        closesocket(fd);
    return -1;

}

static int shoutcast_read(URLContext *h, uint8_t *buf, int size)
{
    struct ICYContext* ctx = h->priv_data;
    int fd = ctx->fd;
    int ret = wait_fd(fd, READABLE);
    if(ret <=0)
    {
        return -1;
    }
    ret = recv(fd, buf, size, 0);
    return ret;
}

static int shoutcast_close(URLContext *h)
{
    struct ICYContext* ctx = h->priv_data;
    int fd = ctx->fd;
    if(fd > 0)
        closesocket(fd);
    av_free(ctx);
    return 0;
}

URLProtocol ff_shoutcast_protocol = {
    .name      = "shoutcast",
    .url_open  = shoutcast_open,
    .url_read  = shoutcast_read,
    .url_close = shoutcast_close,
};
