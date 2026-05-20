#include "net_dev.h"
#include <stdio.h>
#include <string.h>
#include "lib.h"
/*ring buffer*/

uint8_t net_ring_buffer[RING_BUFFER_SIZE];
ring_buffer_t net_rb;
ring_buffer_t *rb = &net_rb;

net_dev_t net_dev_m;
net_dev_t* net_dev = &net_dev_m;

net_drv_ops_t drv_ops;

static int net_opened = 0;


/* =============== driver registration =============== */

/**
 * Set the driver operations. Must be called before net_dev_open().
 * Typically called by the porting layer.
 */
int net_drv_register(const net_drv_ops_t *ops)
{
    if (ops == NULL) {
        return -1;
    }
    memcpy(&drv_ops, ops, sizeof(net_drv_ops_t));
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


/* read a single byte from ring buffer, fall back to driver recv with timeout */
static int ring_read_byte(uint32_t timeout_ms)
{
    if (!ring_buffer_is_empty(rb)) {
        uint8_t c;
        ring_buffer_read(rb, &c, 1);
        return (int)c;
    }
    if (drv_ops.recv_byte) {
        return drv_ops.recv_byte(timeout_ms);
    }
    return -1;
}


/* =============== public API =============== */


int net_dev_init(net_dev_t*net_dev)
{    
    ring_buffer_init(rb,net_ring_buffer,RING_BUFFER_SIZE);
    memset(net_dev->recv_buf,0,sizeof(net_dev->recv_buf));
    memset(net_dev->send_buf,0,sizeof(net_dev->send_buf));
    net_dev->msta=MSTA_DISCONNECTED;
    net_dev->ssta=SSTA_IDLE;
}



int net_dev_open(const char *url)
{
    if (net_opened) {
        return -1;
    }

    /* init ring buffer */
    ring_buffer_clear(rb);

    /* initialize hardware */
    if (drv_ops.init && drv_ops.init() != 0) {
        return -1;
    }

    net_opened = 1;
    (void)url;
    return 0;
}



void net_dev_close(void)
{
    if (!net_opened) {
        return;
    }

    if (drv_ops.deinit) {
        drv_ops.deinit();
    }

    net_opened = 0;
}

int net_dev_http_get_range(uint32_t offset, uint16_t len, uint8_t *buf)
{
    char request[256];
    int req_len;
    int c;
    uint16_t i;

    if (!net_opened) {
        return -1;
    }

    /* flush ring buffer before new request */
    ring_buffer_clear(rb);

    /* build HTTP Range GET request */
    req_len = snprintf(request, sizeof(request),
        "GET / HTTP/1.1\r\n"
        "Range: bytes=%lu-%lu\r\n"
        "\r\n",
        (unsigned long)offset,
        (unsigned long)(offset + len - 1));
    if (req_len < 0 || req_len >= (int)sizeof(request)) {
        return -1;
    }

    /* send request */
    if (drv_ops.send == NULL) {
        return -1;
    }
    if (drv_ops.send((const uint8_t *)request, (size_t)req_len) < 0) {
        return -1;
    }

    /* skip HTTP response headers (\r\n\r\n) */
    if (skip_http_header(rb) != 0) {
        return -1;
    }

    /* read body: exactly len bytes */
    for (i = 0; i < len; i++) {
        c = ring_read_byte(10000);
        if (c < 0) {
            return (int)i;  /* partial read */
        }
        buf[i] = (uint8_t)c;
    }

    return len;
}

