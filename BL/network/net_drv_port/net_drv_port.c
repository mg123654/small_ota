#include "net_drv_port.h"


/*throw msg to ring buffer,this fuc should be call in a interrupt*/
int net_read_msg(ring_buffer_t* rb){

    /*
    int data;
    int len;
    len=get_msg_from_server(data);
    ring_buffer_write(rb,data,len);
    */
}

net_drv_ops_t net_drv_opt_m={
    
};
/*connect to server*/
int net_drv_init(void){

}

void net_drv_deinit(void){

}

int net_drv_send(const uint8_t *data,size_t len){
    
}
