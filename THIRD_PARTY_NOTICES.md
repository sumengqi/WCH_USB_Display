# 第三方许可说明

仓库根目录的 `LICENSE`（GPL-3.0-only）只适用于本项目原创代码、文档与素材，
不会替代下列第三方文件中的版权与许可声明。

## WCH CH32V30x EVT / BSP

- 来源：南京沁恒微电子 CH32V30x EVT；公开上游可参考
  [openwch/ch32v307](https://github.com/openwch/ch32v307)。
- 范围：`Core/`、`Debug/`、`Peripheral/`、`Startup/`，以及 `User/` 中带有
  WCH 文件头的系统和中断文件。
- 许可：带 `SPDX-License-Identifier: Apache-2.0` 的文件按 Apache License 2.0
  分发，许可全文见 `LICENSES/Apache-2.0.txt`。

## WCH USBHS 设备驱动

- 来源：CH32V30x EVT 的 `USBHS/DEVICE/CH372Device_Double_Buffering` 例程。
- 范围：`Peripheral/inc/ch32v30x_usb.h`、`User/ch32v30x_usbhs_device.c` 和
  `User/ch32v30x_usbhs_device.h`；后两个文件已针对本项目的 Vendor 显示接口和
  UAC1 扬声器作了修改。
- 许可：以各文件原有的 WCH 文件头声明为准，其中要求软件及二进制用于南京沁恒
  微电子生产的微控制器。这些文件不按 GPL-3.0-only 重新授权。

## incbin

- 来源：[graphitemaster/incbin](https://github.com/graphitemaster/incbin)，作者
  Dale Weiler。
- 范围：`User/incbin.h`。
- 许可：The Unlicense / public-domain dedication，许可全文见
  `LICENSES/Unlicense.txt`。

