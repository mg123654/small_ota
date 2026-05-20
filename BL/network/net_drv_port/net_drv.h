/**
 * Low-level driver operations.
 * Implement these for your UART/SPI/I2C hardware.
 * All operations are synchronous (blocking) besides net_read_msg();
 * 
 */

#ifndef _NET_DRV_H_
#define _NET_DRV_H_

#include <stdint.h>
#include <stddef.h>
#include "lib.h"
#ifdef __cplusplus
extern "C" {
#endif


/* ring buffer capacity (tune per platform) */
#ifndef NET_DRV_RING_BUF_SIZE
#define NET_DRV_RING_BUF_SIZE   1024
#endif

/* driver ops — implement these callbacks */
typedef struct {
    /**
     * Initialize hardware (UART/SPI/I2C), configure external network module.
     * @return 0 = success, -1 = error
     */
    int  (*init)(void);

    /**
     * Deinitialize hardware, put module to sleep.
     */
    void (*deinit)(void);

    /**
     * Send raw bytes to network module (blocking, all bytes sent).
     * @param data   bytes to send
     * @param len    number of bytes
     * @return actual bytes sent, -1 = error
     */
    int  (*send)(const uint8_t *data,size_t len);


}net_drv_ops_t;

/*transport msg to ring buffer,this should be call in interrupt or as a task*/
int net_read_msg(ring_buffer_t* rb);


#ifdef __cplusplus
}
#endif

#endif /* _NET_DRV_H_ */
