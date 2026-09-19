# ESP32-S3 N16R8 + ST7789 多物体弹跳动画

使用 VS Code 的 PlatformIO 插件和 Arduino 框架。屏幕为 **240×240 SPI ST7789**。
上电后，黑色背景上同时显示 **8 个不同大小、不同速度的彩色方块**，带白色描边，
碰到动画区域边缘反弹。方块可以重叠穿过，暂不模拟物体间碰撞。
顶部信息栏显示 **实时 FPS 和方块数量**，方块不会进入信息栏。
帧率上限约 60 FPS；显示值是每 0.5 秒统计一次的实际完成帧率，包含绘制、SPI 传输和等待时间。
速度使用像素/秒计算，正常帧率变化不会改变运动速度；单次更新超过 50ms 时限制运动步长，避免暂停后跳跃。

## 接线

| ST7789 | ESP32-S3 | 功能 |
| --- | --- | --- |
| VCC | 3.3V | 电源 |
| GND | GND | 共地 |
| SCL / SCK | GPIO12 | SPI 时钟 |
| SDA / MOSI | GPIO11 | SPI 数据，非 I2C |
| RES / RST | GPIO8 | 屏幕复位 |
| DC | GPIO9 | 数据/命令 |
| CS | GPIO10 | 片选 |
| BLK | GPIO7 | 背光控制，默认高电平点亮 |

这里按 BLK 为模块的逻辑背光控制输入使用；如果它直接连接背光 LED，
应按模块规格通过驱动电路供电，不能让 GPIO 承担背光供电电流。

## 编译与烧录

1. 在 VS Code 中安装 **PlatformIO IDE** 插件。
2. 用 **Open Folder** 打开本目录（包含 `platformio.ini`）。
3. 等待 PlatformIO 安装声明的依赖，点击底部 **Build（✓）** 编译。
4. 用 USB 数据线连接开发板，点击 **Upload（→）** 烧录。
5. 烧录后自动播放动画；可打开 **Serial Monitor** 查看初始化信息，波特率为 `115200`。

PlatformIO 终端中也可执行：

```sh
pio run
pio run -t upload
pio device monitor
```

`platformio.ini` 以 `esp32-s3-devkitc-1` 为基础，覆盖为 **16MB QIO Flash + 8MB OPI PSRAM**，
使用 `default_16MB.csv` 分区。固定 Espressif32 平台版本为 `6.12.0`，对应 Arduino-ESP32 `2.0.17`。
RGB565 整帧缓冲区占 115200 字节（约 112.5 KiB），通过 GFX 分配，未强制放入 PSRAM；
PSRAM 配置仍按 N16R8 启用。分配失败时串口会提示错误。

默认串口日志走 ESP32-S3 **原生 USB CDC**，建议连接开发板的原生 USB 接口。
如果通过 CH340/CP210x 等 USB 转串口接口查看日志，将 `ARDUINO_USB_CDC_ON_BOOT=1`
改为 `ARDUINO_USB_CDC_ON_BOOT=0`，重新编译烧录。程序不会等待电脑打开串口。

有多个串口时，在 `platformio.ini` 中设置 `upload_port` / `monitor_port`。
首次烧录若无法连接，可按住 **BOOT**，按一下 **RESET/EN**，再松开 **BOOT** 后重试；
手动进入下载模式后，烧录结束如未自动运行，按一下 **RESET/EN**。

## 修改参数

在 `include/display_config.h` 中集中修改：

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `kWidth` / `kHeight` | 240 / 240 | 屏幕原始宽高 |
| `kRotation` | 0 | 0、1、2、3 分别旋转 0°、90°、180°、270° |
| `kInvertColors` | true | 如颜色呈负片效果，可尝试 false |
| `kBacklightOn` | HIGH | 背光低电平有效的模块改为 LOW |
| `kSpiFrequency` | 20000000 | SPI 20MHz；出现花屏时可降到 10000000 |
| `kBoxCount` | 8 | 方块数量，1～32；越多则绘制开销越大 |
| `kMinBoxSize` / `kMaxBoxSize` | 12 / 36 | 方块边长范围，像素 |
| `kSpeedScale` | 1.0 | 速度倍率，例如 0.5 为半速、2.0 为两倍速 |
| `kHudHeight` | 24 | 顶部信息栏高度，至少 20 像素 |
| `kFrameIntervalUs` | 16667 | 最小帧间隔，微秒；用于限制最高帧率 |
| `kFpsUpdateIntervalUs` | 500000 | 实测 FPS 更新间隔，微秒 |
| `kMaxPhysicsStepUs` | 50000 | 单次运动更新的最大时间跨度，微秒 |

实现使用 Adafruit ST7789 驱动和 GFX 的 RGB565 内存画布。
每帧先在内存中合成所有方块，再向屏幕传输各方块新旧位置的包围矩形。
所有刷新区域都来自同一幅完整画面，因此重叠方块不会互相擦除、留下黑洞或拖影。
信息栏只在读数更新时传输，全屏传输只在启动时执行一次。
这能减少 SPI 传输量与擦除闪烁，但不使用屏幕 TE 同步，不能保证完全消除扫描撕裂。

运动与矩形计算位于 `include/animation.h`，不依赖 Arduino；可用本机 C++ 编译器验证：

```sh
g++ -std=c++11 -Wall -Wextra -Iinclude test/animation_test.cpp -o .pio/animation_test.exe
.pio/animation_test.exe
```

## 排查

- **完全不亮**：检查 3.3V、GND、BLK 接线和背光有效电平。
- **背光亮但没有图像**：检查 CS/DC/RST/SCK/MOSI 是否与接线表一致，尤其不要互换 SCK 与 MOSI。
- **花屏或显示不稳定**：缩短杜邦线并降低 SPI 频率。
- **画面偏移/方向不对**：确认模块为 240×240 ST7789，并尝试调整 `kRotation`。
- **没有串口日志**：确认使用的 USB 接口与 CDC 配置匹配，必要时打开串口后按 RESET。

## 参考

- [PlatformIO ESP32-S3-DevKitC-1 文档](https://docs.platformio.org/en/latest/boards/espressif32/esp32-s3-devkitc-1.html)
- [Adafruit ST7789 官方示例](https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/examples/graphicstest_st7789/graphicstest_st7789.ino)
