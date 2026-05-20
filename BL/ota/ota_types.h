#ifndef _OTA_TYPES_H_
#define _OTA_TYPES_H_

#include <stdint.h>



/* =============== OTA constants =============== */
#define OTA_SLICE_DATA_SIZE        512        /* data bytes per slice */
#define OTA_SLICE_CRC_LEN          2          /* CRC16 = 2 bytes */
#define OTA_SLICE_TOTAL_SIZE       (OTA_SLICE_DATA_SIZE + OTA_SLICE_CRC_LEN)  /* 514 */

#define CONFIG_OTA_URL_MAX         256
#define OTA_ENABLE_MAGIC           0xA5A5

/* =============== default URL (override in port layer) =============== */
#ifndef CONFIG_DEFAULT_OTA_URL
#define CONFIG_DEFAULT_OTA_URL      "http://example.com/firmware.bin"
#endif

/* =============== config struct (stored in config partition) =============== */
typedef struct {
    uint32_t total_size;           /* total image size = Content-Length = slices * 514 */
    uint32_t current_offset;       /* bytes already written to app partition (data only, no CRC) */
    uint16_t enable;               /* 0xA5A5 = upgrade enabled, 0 = idle */
    uint8_t  reserved[2];          /* 4-byte alignment */
    char     url[CONFIG_OTA_URL_MAX];
} ota_config_t;



#endif /* _OTA_TYPES_H_ */
