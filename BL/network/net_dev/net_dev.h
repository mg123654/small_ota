#ifndef _NET_DEV_H_
#define _NET_DEV_H_

#include <stdint.h>
#include "../net_drv_port/net_drv_port.h"

#define RING_BUFFER_SIZE 1024


#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    MSTA_DISCONNECTED = 0,
    MSTA_CONNECTING,
    MSTA_CONNECTED,
    MSTA_ERROR

} net_dev_msta_t;

typedef enum {
    SSTA_IDLE = 0,
    SSTA_BUSY,
    SSTA_ERROR,
    SSTA_TIMEOUT
} net_dev_ssta_t;

typedef struct {
    net_dev_msta_t msta;
    net_dev_ssta_t ssta;
    uint8_t recv_buf[1024];
    uint8_t send_buf[512];
    
}net_dev_t;



/* =============== driver registration (call before net_dev_open) =============== */

int net_dev_init(net_dev_t * net_dev);

/* =============== network device abstract API =============== */

/**
 * Open network device and connect to target server.
 * Extracts host from url, initializes hardware, connects to module.
 *
 * @param url   full firmware URL (e.g. http://server/firmware.bin)
 * @return 0 = success, -1 = error
 */
int net_dev_open(const char *url);




#ifdef __cplusplus
}
#endif

#endif /* _NET_DEV_H_ */
