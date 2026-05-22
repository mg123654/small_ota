# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

小型嵌入式 OTA 升级框架。核心目标：体积小、可移植、适合资源受限 MCU 平台（支持 RTOS / bare metal）。

---

## Architecture

### 分区布局

| 分区 | 用途 | 说明 |
|------|------|------|
| bootloader | 启动 + OTA 下载 | 包含 OTA 核心逻辑，不包含网络栈 |
| app | 主应用固件 | 运行时被 OTA 覆盖写入 |
| config | OTA 配置持久化 | 存储 URL、下载进度、升级标志 |

### 启动流程

```
上电 → main() (BL/boot/main.c)
        ├─ fal_init()
        ├─ 读取 config 分区 ota_config_t
        ├─ URL 无效 → 写入 CONFIG_DEFAULT_OTA_URL
        ├─ enable != 0xA5A5 → 直接跳转 app（正常启动）
        └─ enable == 0xA5A5
             ├─ net_dev_open() 初始化外部网络模块
             ├─ total_slices = total_size / 514
             ├─ current_slice = current_offset / 512
             ├─ 从 current_slice 开始逐片下载:
             │    http_offset = current_slice * 514
             │    net_dev_http_get_range(url, http_offset, 514) → 阻塞收到 514 字节
             │    crc16(data[0..511]) == data[512..513] ?
             │    ├─ 失败 → 结束，不更新 offset（下次启动从此片重试）
             │    └─ 通过 → fal_partition_write(app, data, 512)
             │              current_offset += 512
             │              更新 config 分区 current_offset
             ├─ 全部完成 → enable = 0; total_size = 0; 写回 config
             └─ 跳转 app
```

关键设计决策：
- **同步阻塞模型**：收到完整切片后校验 → 写入 Flash → 再请求下一片。不涉及中断与 Flash 写操作的冲突。
- **不分片缓存**：每片校验通过后直接写入 Flash，不保存分片数据。
- **暂不支持断点续传**：断电重启后从 config 中记录的 current_offset 对应分片重新下载（已写入 Flash 的数据是幂等的）。

### 网络架构（三层）

```
┌─────────────────────────────────┐
│  OTA 核心逻辑                    │  不知道底层通信方式
│  - get_slice(offset, size)      │
│  - crc16 校验 + FAL 写 Flash     │
└──────────┬──────────────────────┘
           │
┌──────────▼──────────────────────┐
│  网络设备抽象层 (net_dev)         │  统一的 net_dev 接口
│  - net_dev_init()               │  使用 lib_ring_buffer 作为缓冲区
│  - net_dev_open()               │  HTTP 头解析用自建状态机
│  - net_dev_http_get_range()     │
│  - net_drv_register()           │
└──────────┬──────────────────────┘
           │
┌──────────▼──────────────────────┐
│  驱动适配层 (net_drv_port)        │  UART/SPI/I2C 具体实现
│  - net_drv_ops_t 回调注册        │  封装 AT 指令或自定义帧
│  - init / deinit / send         │  中断接收写入 ring buffer
│  - net_read_msg(rb)             │  从 ring buffer 读取网络数据
└──────────┬──────────────────────┘
           │
   ┌───────┴───────┐
   │ 外部联网模块    │  跑完整 TCP/HTTP 栈
   │ (WiFi/4G/ETH)  │  通过 UART/SPI/I2C 连接 MCU
   └───────────────┘
```

---

## Flash Abstraction Layer (FAL)

项目使用定制版 FAL（已去除 RT-Thread 强依赖），位于 `lib/lib_fal/`。

### 源文件清单

```
lib/lib_fal/
├── fal/
│   ├── inc/
│   │   ├── fal.h           # 主头文件（RT-Thread 声明用 #ifdef __RTTHREAD__ 保护）
│   │   └── fal_def.h       # 结构体定义 + 日志宏 + 内存分配宏
│   └── src/
│       ├── fal.c           # fal_init() + fal_init_check()
│       ├── fal_flash.c     # Flash 设备注册与查找
│       ├── fal_partition.c # 分区表管理 + fal_partition_read/write/erase
│       └── fal_rtt.c       # RT-Thread 适配（#ifdef RT_VER_NUM，bare metal 下不编译）
├── port/
│   ├── fal_cfg.h           # Flash 设备表 + 分区表
│   ├── fal_flash_port.c    # 移植模板（sysprintf + flash ops 空壳）
│   └── example/            # SWM320 等参考移植
└── FAL_USAGE.md            # FAL 详细使用说明
```

详细使用说明见 `lib/lib_fal/FAL_USAGE.md`。

---

## OTA Config 分区结构体

```c
#define CONFIG_OTA_URL_MAX    256
#define OTA_ENABLE_MAGIC      0xA5A5

typedef struct {
    uint32_t total_size;       // 打包后镜像总大小（= Content-Length = 片数 × 514）
    uint32_t current_offset;   // 已写入 App 分区的原始数据字节数（不含 CRC）
    uint16_t enable;           // 升级使能标志，0xA5A5 = 使能，0 = 无升级任务
    uint8_t  reserved[2];      // 对齐到 4 字节
    char     url[CONFIG_OTA_URL_MAX];
    uint8_t  padding[];        // 填充到 FAL 分区大小
} ota_config_t;
```

### 字段职责和写入者

| 字段 | 写入者 | 说明 |
|------|--------|------|
| url | App / Bootloader | App 在触发升级前写入；Bootloader 在首次初始化（读到无效值）时写入默认 URL |
| total_size | App | 触发升级时写入，0 = 无升级任务 |
| enable | App | 最后写入（设为 0xA5A5），保证前面字段写完才使能 |
| current_offset | Bootloader | 每成功写入一片 App 数据后更新 |

### URL 可重定向机制

- Bootloader 启动时读取 config 中的 url
- 如果有效（非全 0xFF 擦除态）→ 使用 config 中的 url
- 如果无效 → 写入 `CONFIG_DEFAULT_OTA_URL` 宏定义的默认值
- App 运行时可以重写 url，指向新服务器

```c
// 由移植层在编译时提供
#define CONFIG_DEFAULT_OTA_URL "http://example.com/firmware.bin"
```

---

## 固件镜像格式

### PC 打包工具输出格式

打包工具将原始固件二进制按 N=512 字节分片，每片末尾追加 2 字节 CRC16：

```
[slice_0: 512 bytes data][CRC16 2 bytes][slice_1: 512 bytes data][CRC16 2 bytes]...
```

- 每片请求：514 字节（512 数据 + 2 CRC）
- CRC 只校验本片的 512 字节数据
- **最后一个切片必须被填充到 514 字节，否则 Bootloader 拒绝升级**
- total_size = 打包后含 CRC 的总大小 = Content-Length = 片数 × 514

### 下载流程中的关键值

```
total_slices  = total_size / 514
current_slice = current_offset / 512
http_offset   = current_slice * 514
```

### 获取镜像总大小

通过 HTTP GET 请求 0 字节（Range: bytes=0-0），解析返回头中的 `Content-Length` 值。

---

## CRC 校验

- 算法：**CRC16-MODBUS**
- 多项式：0x8005
- 校验范围：每片 512 字节数据（不含末尾的 CRC 字节）
- 查表大小：512 字节 ROM
- 选 CRC16 而非 CRC32 的理由：切片独立校验，每片 512 字节下 CRC16 检出率足够，更快更省 ROM

---

## 分片大小

**N = 512 字节**

选择理由：
- 514 字节缓冲区对绝大多数 MCU 无 RAM 压力
- 512 与常见 Flash 最小写入单元对齐（256/512 字节），避免跨页处理
- HTTP Range 头部开销约 300 字节，N=512 时 CRC 开销占比仅 0.39%

---

## App 侧职责

App 不参与下载过程，仅负责触发升级：

1. 收到升级通知（途径由 App 自行决定，不纳入本框架）
2. 调用框架提供的 API 写入 config 分区字段（url、total_size）
3. 调用 `ota_config_set_enable()` 写 enable = 0xA5A5
4. 调用系统复位

框架提供以下 API 供 App 使用（声明在 `BL/ota/ota_config.h`）：

```c
// 设置 OTA 使能标志，App 调用后重启即可触发升级
int ota_config_set_enable(void);

// 重置整个 config（清除所有字段，含 enable）
int ota_config_reset(void);

// 写入升级 URL（自动处理 \0 截断）
int ota_config_set_url(const char *url);

// 写入镜像总大小
int ota_config_set_total_size(uint32_t size);
```

---

## 网络设备抽象层 API

```c
// 打开网络设备（初始化硬件 + 联网模块）
int net_dev_open(const char *url);

// 关闭网络设备
int net_dev_close(void);

// HTTP Range GET 请求，阻塞等待返回指定范围的数据
// offset: 请求起始偏移
// len: 请求长度
// buf: 输出缓冲区
// 返回: 实际收到字节数，<=0 为错误
int net_dev_http_get_range(uint32_t offset, uint16_t len, uint8_t *buf);
```

---

## 目录结构

```
small_ota/
├── BL/                           # Bootloader 源码（所有核心模块）
│   ├── framwork.h                # 空文件（占位）
│   ├── boot/
│   │   ├── bl.h                  # 引导入口聚合头文件
│   │   ├── bl_jump.h / .c        # ARM Cortex-M 跳转（支持 ARMCC/GCC/IAR）
│   │   ├── main.c                # 入口：fal_init → config → download → jump
│   │   └── readme.md
│   ├── network/
│   │   ├── net_dev/
│   │   │   ├── net_dev.h / .c    # 抽象层：阻塞 GET Range，HTTP 头解析，驱动注册
│   │   │   └── readme.md
│   │   └── net_drv_port/
│   │       ├── net_drv.h         # 驱动 ops 结构体 + TCP 辅助函数声明
│   │       └── net_drv.c         # TCP 辅助函数（connect/send/read stubs）
│   └── ota/
│       ├── ota_types.h           # 常量 + ota_config_t 结构体
│       ├── ota_config.h / .c     # config 分区读写 API（基于 FAL）
│       └── ota_core.h / .c       # OTA 下载循环（逐片 GET→CRC→写 Flash→更新进度）
├── lib/
│   ├── lib.h                     # 聚合头文件（include 所有 lib）
│   ├── lib_crc16/
│   │   ├── crc16.h / .c          # CRC16-MODBUS（多项式 0x8005）
│   ├── lib_fal/                  # FAL 库（Flash 抽象层 + 移植层）
│   ├── lib_ring_buffer/
│   │   ├── ring_buffer.h / .c    # 通用环形缓冲区
│   └── lib_stropt/               # 字符串解析（Content-Length 等）
└── CLAUDE.md
```

所有库文件统一放在 `lib/` 下，命名前缀 `lib_`。BL 模块放在 `BL/` 下按功能分组。

**注意：项目当前没有 Makefile / CMakeLists.txt 等构建配置文件。**

---

## 设计约束和边界条件

1. **全部 Flash 操作通过 FAL API**，不直接操作 Flash 地址
2. **Bootloader 不含网络协议栈**，网络能力完全外置到通信模块
3. **同步阻塞式下载**，保证 Flash 写入时不会有新数据进入
4. **CRC16 校验失败不重试当前片**，依赖断电续传来恢复
5. **镜像打包时最后一个切片必须填充到 514 字节**，PC 打包工具负责保证
6. **enable 标志独立于 total_size**，App 最后写 enable 保证写入原子性
7. **current_offset 双重职责**：HTTP 续传位置计算 + App 分区写入偏移

---

## 库说明

### lib.h — 聚合头文件

`lib/lib.h` 是统一的库入口，include 了所有子库：

```c
#include "./lib_crc16/crc16.h"
#include "./lib_fal/fal/inc/fal.h"
#include "./lib_ring_buffer/ring_buffer.h"
#include "./lib_stropt/stropt.h"
```

BL 模块通过 `#include "lib.h"` 获取所有库接口。注意：此 include 路径依赖构建系统的 `-I lib/` 配置。

### lib_ring_buffer — 通用环形缓冲区

`lib/lib_ring_buffer/ring_buffer.h` 提供独立于业务的环形缓冲区，由 `net_dev` 层使用。

API：
- `ring_buffer_init(rb, buf, size)` — 初始化
- `ring_buffer_clear(rb)` — 清空（仅重置指针，不清零数据）
- `ring_buffer_is_empty(rb)` — 判空
- `ring_buffer_available(rb)` — 可读字节数
- `ring_buffer_write(rb, data, len)` — 写入（处理回绕）
- `ring_buffer_read(rb, out_buf, max_len)` — 读取（处理回绕）

### lib_stropt — 字符串解析

`lib/lib_stropt/stropt.h` 提供 HTTP 头解析工具：
- `stropt_parse_content_length()` — 解析 Content-Length 值
- `stropt_find_body_start()` — 查找 `\r\n\r\n` 分隔位置
- `stropt_is_valid()` — 检查字符串是否非空且非 Flash 擦除态

注意：当前 `net_dev.c` 使用自己的 `skip_http_header()` 状态机解析 HTTP 头，`stropt` 尚未被调用。

### lib_crc16 — CRC16-MODBUS

`lib/lib_crc16/crc16.h` 提供 `crc16_modbus(data, len)` 函数，256 条目查表实现，512 字节 ROM。

---

## 当前开发状态

### 无构建系统

项目尚无 Makefile / CMakeLists.txt。所有文件通过 include 路径组合，由 IDE 或手动管理。

### 已知问题

以下问题是当前代码中存在的 bug，修改相关模块时需注意：

1. **net_dev.c 中 `drv_ops` 未声明**：`drv_ops` 在 `net_dev_open()`、`net_dev_close()`、`net_dev_http_get_range()` 和 `net_drv_register()` 中使用，但该翻译单元内未声明此全局变量（应添加 `static net_drv_ops_t drv_ops;`）。

2. **net_dev.c 中 `ring_flush()` 和 `ring_read_byte()` 未定义**：V1 代码中这两个函数在 net_dev.c 内为 static，但 V2 拆分到 `lib_ring_buffer` 后丢失。`ring_flush` 可替换为 `ring_buffer_clear(rb)`；`ring_read_byte` 需要新增实现（带超时的单字节读取，内部调用 `drv_ops.recv_byte`）。

3. **net_dev.c 中 `net_dev_open()` 重复 init**：`drv_ops.init()` 被调用了两次（L74-L78），应简化为单次调用。

4. **ring_buffer.h 函数名不匹配**：头文件声明 `get_ring_buffer_available()`，但 .c 文件实现的是 `ring_buffer_available()`。

5. **net_dev.h 中枚举值冲突**：`net_dev_msta_t` 和 `net_dev_ssta_t` 都定义了 `STATE_IDLE = 0`，会导致编译错误。

6. **net_drv.h 中 `extern ring_buffer_t rb`**：引用了 `net_dev.c` 中定义的 `rb`，需要确保构建时链接顺序正确。

7. **`BL/framwork.h` 为空文件**，无实际用途。
