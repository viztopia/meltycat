# meltycat

[English](README.md)

meltycat 是一只软软地瘫在当前窗口右上角的桌面猫。它会盯着你的鼠标看，闲下来就打盹，有八种皮肤可以换。

| 平台 | 状态 |
| --- | --- |
| Windows 10 / 11（x64） | 已发布 |
| macOS 11+（Apple 芯片和 Intel） | 开发中 |

## 预览

<!-- 把截图和 GIF 放进 assets/ 文件夹，然后在这里引用，例如：
![待机](assets/idle.gif)
![捕猎](assets/hunt.gif)
-->

预览图稍后补充。

## 功能

- 快速来回晃动鼠标，小猫就会进入捕猎状态。一直晃下去，看看它能认真到什么程度。
- 用鼠标在小猫的头、身体或前爪上慢慢划过，就能摸摸它。
- 皮肤菜单按毛长分组。长毛组依次为布偶猫、缅因猫和西伯利亚猫，短毛组依次为橘虎斑、黑猫、狸花猫、奶牛猫和海胆（Uni）。
- 支持 15 种界面语言，皮肤和语言选择会自动保存。

## 下载与安装

到 [Releases 页面](../../releases/latest) 下载最新版本。每个版本都附有 `SHA256SUMS.txt`，可用来校验下载文件。

### Windows

1. 下载 `meltycat-windows-x64.zip`，解压到一个有写入权限的文件夹。设置会保存在程序旁边，所以不要放进 `Program Files`。
2. 双击 `meltycat.exe`，无需安装，也不需要额外的运行库。
3. 小猫会出现在当前窗口上，系统托盘里会出现猫图标。如果看不到，请点开任务栏右下角 `^` 下的隐藏图标。
4. 如果 Windows SmartScreen 提示“Windows 已保护你的电脑”，点击**更多信息**，再点**仍要运行**。这个程序没有代码签名。

卸载时，先在托盘菜单里选择**退出 meltycat**，再删除文件夹即可。

### macOS

macOS 版本还在开发中，下面的步骤以后可能会调整。

1. 下载 `meltycat-macos-universal.zip`，解压后把 `meltycat.app` 拖进**应用程序**文件夹。
2. 首次打开时，macOS 可能提示无法验证应用，因为它没有经过公证。macOS 15 及以上版本请打开**系统设置 > 隐私与安全性**，在“安全性”一栏点击**仍要打开**。更早的版本可以右键点击应用，选择**打开**。
3. 小猫会出现在当前窗口上，菜单位于顶部菜单栏，应用不会在 Dock 里显示图标。

卸载时，先在菜单栏里退出，再把 `meltycat.app` 移到废纸篓。

## 使用方法

- **捕猎：** 快速来回晃动鼠标。
- **摸摸：** 在小猫的头、身体或前爪上慢慢移动鼠标。
- **菜单：** 右键点击托盘图标（Windows）或点击菜单栏图标（macOS），可以换皮肤、切换语言或退出。

## 从源码编译

**Windows（x64）：** 安装 MSVC C++ Build Tools 和 Windows SDK，运行 `build.cmd`，然后打开 `build\meltycat.exe`。

**macOS：** 安装 Xcode 或 Xcode 命令行工具，运行 `bash macos/build.sh`，然后打开 `macos/build/meltycat.app`。

## 许可证

可免费用于个人、学习和其他非商业用途，欢迎修改和分享，但不得用于商业用途。本项目采用 [PolyForm Noncommercial License 1.0.0](LICENSE)，完整条款见该文件。

## 致谢

灵感来自 [KuroCat](https://github.com/chanke17/KuroCat) 和 [kurocat](https://github.com/qianyiai/kurocat)。
