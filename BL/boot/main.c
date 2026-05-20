#include "bl.h"
/*整个流程采用同步设计，全部都为阻塞操作，使用单线程模式*/

void main(void)
{
    /*
    
    平台硬件初始化 
    
    */

    fal_init();




    //读取配置分区参数

    //参数检查，是否已有app分区，如果没有直接进行进入ota模式。



    //如果有app分区，判断ota使能参数，使能则进入ota模式，否则跳转到app。

    //ota模式，需要进行网络回调注册，设备初始化，













    const struct fal_partition *app_part;
    ota_config_t cfg;

    /* 1. initialize FAL */
    if (fal_init() < 0) {
        goto jump_to_app;
    }

    /* 2. find partitions */
    app_part = fal_partition_find("app");
    if (app_part == NULL) {
        goto jump_to_app;
    }

    /* 3. load OTA config */
    if (ota_config_load(&cfg) < 0) {
        goto jump_to_app;
    }

    /* 4. URL redirect: if invalid, write default */
    if (!ota_config_url_is_valid(&cfg)) {
        memset(cfg.url, 0, sizeof(cfg.url));
        strncpy(cfg.url, CONFIG_DEFAULT_OTA_URL, CONFIG_OTA_URL_MAX - 1);
        ota_config_save(&cfg);
    }

    /* 5. check upgrade enable */
    if (cfg.enable == OTA_ENABLE_MAGIC) {
        if (cfg.total_size > 0 && cfg.total_size < OTA_SLICE_TOTAL_SIZE) {
            goto jump_to_app;
        }

        if (cfg.total_size > 0) {
            if (net_dev_open(cfg.url) == 0) {
                ota_core_run(app_part, &cfg);
                net_dev_close();
            }
        }
    }

jump_to_app:
    bl_jump_to_app(app_part->offset);

    while (1) {}
}
