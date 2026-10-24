# USB小电视固件

本目录是 `USB小电视（USBminiTV）` 的 CH32V305FBP6 固件工程。USB 设备层使用
WCH 官方 `CH372Device_Double_Buffering` 例程中的 USBHS 驱动，并在其上实现
Vendor 显示接口和 UAC1 自适应扬声器。

## 功能

- USB 产品字符串：`USBminiTV`
- VID/PID：`1209:2110`
- Interface 0：Vendor 显示接口，不包含 HID
- EP1 Bulk OUT：显示数据，HS 512 字节包，硬件双缓冲
- EP2 Bulk IN：保留原显示接口的 IN 端点
- 2048 字节显示块聚合和双应用缓冲
- 240x240 / 480x480 / 960x960 分辨率查询
- EDID、像素格式、驱动握手、着色器和常量缓冲区厂商请求
- ST7789 SPI2 + DMA1 Channel 5 显示输出
- UAC1 扬声器接口：16-bit、双声道输入
- 采样率：44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz
- EP3 Isochronous OUT：800 字节，自适应同步，无反馈 IN 端点
- 音频在固件内混合为单声道，通过 PA5/DAC2 + TIM7 + DMA2 Channel 4 输出
- 4096 采样环形缓冲；以约 10 ms 待播放音频所对应的采样数为目标值，
  PI 控制器根据实际待播放采样数与目标值的偏差微调 TIM7 周期，调节范围为
  标称采样率的 ±5000 ppm

## 音频硬件与调节原理

模拟音频从 `PA5`（DAC2）输出。DAC 输出需要外接低通滤波与功放后再驱动
扬声器，不能直接驱动低阻扬声器。USB 主机看到的是立体声设备，左右声道在
MCU 内按当前 UAC 音量设置混合成单声道。`PC8` 延续原硬件设计，配置为推挽
输出并保持低电平，用于解除板载音频通道的静音/关断状态。

主机按照选定的标称采样率持续向 EP3 发送 PCM 数据。固件实时计算环形缓冲区中
尚未播放的有效采样数：当待播放采样数高于目标值时，略微加快 DAC 播放；低于
目标值时，略微减慢。这样可以补偿 USB 主机和 MCU 采样时钟之间的微小偏差，
又无需发送异步反馈数据。

## 开机画面

`User/USBminiTV_boot.png` 是为本项目制作的 240×240 开机图，包含 Mini 电视机、
USB 插头和 `USBminiTV` 标识。图片会转换为 RGB565 无损 RLE 数据，开机时直接
从 Flash 流式解码到 LCD，不使用整帧 RAM。原始 RGB565 数据需要 115,200 字节，
16 色优化后的 `User/boot_logo.rle565` 为 16,251 字节。

LCD 初始化期间背光保持关闭；LOGO 写入完成后，背光在 1 秒内由暗到亮，LOGO
总共显示 2 秒。随后固件关闭背光并将显存清为黑色，再启动 USB 枚举。收到第一帧
显示数据时恢复 100% 背光；USB 未配置、总线挂起、总线复位或主机发送 `0x80`
关屏命令时，固件都会黑屏并关闭背光。

更换 BMP 后可用工作区 Python（需 Pillow）重新生成资源：

```powershell
python tools/convert_boot_logo.py User/USBminiTV_boot.png User/boot_logo.rle565
```

运行期间还会以 TIM6 的 10 kHz 自由运行计数器为独立时基扫描 USBHS
`FRAME_NO`。若设备已经配置但帧号连续 20 ms 不变化，也会作为硬件挂起/复位判断
之外的冗余掉线条件执行黑屏关背光。

## 构建

用 MounRiver Studio 导入本目录的 `.project`，选择 `obj` 配置后构建。
目标芯片和链接脚本沿用原工程的 `CH32V305FBP6` 配置。当前代码的
`HSE_VALUE` 为 8 MHz，与原理图 X1 一致。系统 PLL 使用 HSE×18 得到 144 MHz，
USBHS 参考时钟使用 HSE÷2。

最近一次 Clean + Build 结果：`text = 57,844 B`、`data = 952 B`、
`bss = 17,812 B`，满足 128 KiB Flash / 32 KiB SRAM 限制。生成文件位于
`obj/CH32V305FBP6_WCH_USB_Display.hex` 和 `.elf`；`obj/` 是构建产物，不纳入开源仓库。

USB 身份与版本集中在 `User/project_config.h`。更改 VID/PID 后，配套 Android
应用或 Windows 驱动中的设备匹配信息也必须同步更新；这些主机端程序不包含在本
仓库中。

## 目录结构

- `User/`：USB 显示、USB Audio、LCD 驱动、开机图和项目配置
- `Core/`、`Peripheral/`、`Startup/`、`Debug/`：WCH CH32V30x BSP
- `Ld/`：CH32V305FBP6 链接脚本
- `tools/`：开机图 RGB565/RLE 转换工具
- `.project`、`.cproject`、`.settings/`：MounRiver Studio 工程配置
- `CH32V305FBP6_WCH_USB_Display.wvproj`：WCH-Link 工程配置

## 许可

本项目原创的 MCU 固件代码、文档和开机图转换工具采用
[GNU GPL v3.0](LICENSE)（GPL-3.0-only）开源。WCH BSP、WCH USBHS 驱动和
`incbin` 保留各自原有许可；具体边界见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
