#include "stdint.h"
#include "ota.h"
#include "lib.h"

/* external globals from net_dev.c */
extern net_drv_ops_t drv_ops;
extern ring_buffer_t *rb;

/*构造get命令并且发送get请求，通过发送一个接收0字节的range的请求，目的是获取返回响应体中的固件包的总字节*/
static int ota_send_get_req(const char* addr)
{
    char request[384];
    int req_len;

    /* build HTTP GET with Range: bytes=0-0 to request minimal data */
    req_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Range: bytes=0-0\r\n"
        "\r\n",
        addr);

    if (req_len < 0 || req_len >= (int)sizeof(request)) {
        return -1;
    }

    if (drv_ops.send == NULL) {
        return -1;
    }

    if (drv_ops.send((const uint8_t *)request, (size_t)req_len) < 0) {
        return -1;
    }

    return 0;
}

/* skip HTTP headers and return pointer to body start in ring.
 * reads from ring buffer dynamically until \r\n\r\n found.
 * Returns body length, -1 on error. */
static int skip_http_header(ring_buffer_t *rb)  // 需要传入 rb
{
    int state = 0;  /* 0=idle, 1=\r, 2=\r\n, 3=\r\n\r, 4=done */
    uint8_t c;
    
    while (state != 4) {
        // 读取单个字节（非阻塞）
        if (ring_buffer_read(rb, &c, 1) != 1) {
            return -1;  // 数据不足
        }
        
        // 状态机
        switch (state) {
            case 0: 
                state = (c == '\r') ? 1 : 0; 
                break;
            case 1: 
                state = (c == '\n') ? 2 : ((c == '\r') ? 1 : 0); 
                break;
            case 2: 
                state = (c == '\r') ? 3 : 0; 
                break;
            case 3: 
                state = (c == '\n') ? 4 : ((c == '\r') ? 1 : 0); 
                break;
        }
    }
    return 0;
}


/*获取固件包总字节数*/
int ota_get_total_size(const char *url)
{
    uint8_t resp_buf[512];
    uint32_t avail;
    uint32_t prev_avail;
    uint32_t total;
    int timeout;

    /* 清空接收缓冲区 */
    ring_buffer_clear(rb);

    /* 调用ota_send_get_req，发送 Range: bytes=0-0 */
    if (ota_send_get_req(url) != 0) {
        return -1;
    }

    /*
     * 等待响应数据到达环形缓冲区。
     * HTTP/1.1: 服务器先回响应行和头部，再回 body。
     * 外部网络模块通过 UART 中断调用 net_read_msg() 写入 rb。
     * 采用空闲检测：连续一段时间无新数据则判定传输完成。
     */
    timeout      = 10000;   /* 总超时 */
    prev_avail   = 0;
    {
        int idle = 500;     /* 空闲超时 */

        while (timeout > 0) {
            avail = ring_buffer_available(rb);
            if (avail != prev_avail) {
                /* 有新数据到达，重置空闲计时 */
                prev_avail = avail;
                idle       = 500;
            } else if (avail > 0) {
                /* 有数据但不再增长，空闲倒计时 */
                idle--;
                if (idle == 0) {
                    break;
                }
            }
            timeout--;
        }
    }

    if (timeout == 0) {
        return -1;          /* 总超时，无响应 */
    }

    /* 从环形缓冲区读取响应数据 */
    avail = ring_buffer_available(rb);
    if (avail > sizeof(resp_buf)) {
        avail = sizeof(resp_buf);
    }

    total = ring_buffer_read(rb, resp_buf, avail);
    if (total == 0) {
        return -1;
    }

    /* 解析 Content-Length 并返回 */
    return (int)stropt_parse_content_length((const char *)resp_buf, (size_t)total);
}





