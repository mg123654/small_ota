#include "net_dev.h"
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




/* =============== public API =============== */


int net_dev_init(net_dev_t*net_dev)
{    
    ring_buffer_init(rb,net_ring_buffer,RING_BUFFER_SIZE);
    memset(net_dev->recv_buf,0,sizeof(net_dev->recv_buf));
    memset(net_dev->send_buf,0,sizeof(net_dev->send_buf));
    net_dev->msta=MSTA_DISCONNECTED;
    
}


/*connect to server,this */
int net_dev_open(const char *url)
{
    
    if (net_opened) {
        return -1;
    }


    /* initialize hardware */
    if (drv_ops.init()) {
        if (drv_ops.init() != 0) {
            return -1;
        }
    }
    
    net_opened = 1;

    (void)url;
    return 0;
}



