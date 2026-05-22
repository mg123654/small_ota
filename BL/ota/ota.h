#ifndef __OTA_H__
#define __OTA_H__

#include "net_dev.h"
#include "stdint.h"

#define NUM_SLICE_BYTE 514


typedef ota_file_download_opt_t{
    int (*get_total_size)(const char*url);

    int (*get_slice)();
}

/* 获取固件包总字节数（通过 HTTP Range: bytes=0-0 请求，解析 Content-Length） */
int ota_get_total_size(const char *url);

#endif


