<div align="center">
  <img src="assets/meltycat-icon.png" width="96" alt="MeltyCat">
  <h1>MeltyCat</h1>
  <p>MeltyCat 是一只软软地趴在当前窗口右上角的小猫。</p>
  <p><strong>简体中文</strong> · <a href="README.md">English</a></p>
  <p>
    <a href="../../releases/latest"><img src="https://img.shields.io/badge/%E4%B8%8B%E8%BD%BD-%E6%9C%80%E6%96%B0%E7%89%88%E6%9C%AC-d9783a?style=flat-square" alt="下载最新版本"></a>
    <img src="https://img.shields.io/badge/Windows-10%20%7C%2011-7b6fcf?style=flat-square" alt="Windows 10 | 11">
    <img src="https://img.shields.io/badge/macOS-11%2B-7b6fcf?style=flat-square" alt="macOS 11+">
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-8c8496?style=flat-square" alt="License MIT"></a>
  </p>
</div>

## 预览

<table>
  <tr>
    <td align="center"><img src="assets/preview-watch.gif" width="380" alt="橘猫趴在窗口右上角，盯着鼠标"><br>盯着 · 橘虎斑</td>
    <td align="center"><img src="assets/preview-hunt-level1.gif" width="380" alt="布偶猫注意到鼠标在晃"><br>捕猎 · 布偶猫</td>
  </tr>
  <tr>
    <td align="center"><img src="assets/preview-hunt-level3.gif" width="380" alt="奶牛猫认真起来了"><br>认真捕猎 · 奶牛猫</td>
    <td align="center"><img src="assets/preview-sleep.gif" width="380" alt="缅因猫在窗沿上打起瞌睡"><br>打盹 · 缅因猫</td>
  </tr>
</table>

## 它会做什么

- **趴着**：趴在你正在使用的窗口的右上角。你换窗口，它也跟着挪过去。
- **盯着**：鼠标去哪，它就看哪。
- **打盹**：没啥动静的话，它就睡了。
- **捕猎**：快速晃晃鼠标试试。晃得越久，它越认真。
- **摸摸**：可以用鼠标轻轻地摸摸它。

还有一些小细节，留给你自己发现啦。

自带九种皮肤，支持 15 种界面语言。

## 把你家的猫做成皮肤

1. 打开菜单，选**换皮肤…**，点**复制 AI 指南**。
2. 找一个能看图的 AI 助手，粘贴指南，再附上你家猫的照片。一张正脸、一张能看清花纹的侧身，效果最好。
3. 把 AI 给你的 `.meltycat` 文件拖进换皮肤窗口、点**选择文件…**，或者直接粘贴它给的文字。双击文件也能导入。Windows 上请先启动一次 MeltyCat，让它注册这个文件类型；如果这类文件默认由别的应用打开，可以用**打开方式**选择 `meltycat.exe`。
4. 不够像？截一张预览图发回给 AI，让它对照照片再改。

皮肤格式见 [skins/AI-GUIDE.md](skins/AI-GUIDE.md)。

## 下载与安装

到 [Releases 页面](../../releases/latest) 下载最新版本。每个版本都附有 `SHA256SUMS.txt`，可用来校验下载文件。

### Windows 10 / 11（x64）

1. 下载 `meltycat-windows-x64.zip`，解压到一个有写入权限的文件夹。设置会保存在程序旁边，所以不要放进 `Program Files`。
2. 双击 `meltycat.exe`，无需安装，也不需要额外的运行库。
3. 小猫会出现在当前窗口上，系统托盘里会出现猫图标。右键点击它可以换皮肤、切换语言或退出。如果看不到图标，请点开任务栏右下角 `^` 下的隐藏图标。
4. 如果 Windows SmartScreen 提示“Windows 已保护你的电脑”，点击**更多信息**，再点**仍要运行**。这个程序没有代码签名。

卸载时，先在托盘菜单里选择**退出 meltycat**，再删除文件夹。导入的皮肤保存在 `%APPDATA%\meltycat\skins`，也可以一并删除。

### macOS 11+（Apple 芯片和 Intel）

1. 下载 `meltycat-macos-universal.zip`，解压后把 `meltycat.app` 拖进**应用程序**文件夹。
2. 首次打开时，macOS 可能提示无法验证应用，因为它没有经过公证。macOS 15 及以上版本请打开**系统设置 > 隐私与安全性**，在“安全性”一栏点击**仍要打开**。更早的版本可以右键点击应用，选择**打开**。
3. 小猫会出现在当前窗口上，菜单位于顶部菜单栏，可以在那里换皮肤、切换语言或退出。应用不会在 Dock 里显示图标。

卸载时，先在菜单栏里退出，再把 `meltycat.app` 移到废纸篓。导入的皮肤保存在 `~/Library/Application Support/meltycat`，也可以一并删除。

## 从源码编译

**Windows（x64）：** 安装 MSVC C++ Build Tools 和 Windows SDK，运行 `build.cmd`，然后打开 `build\meltycat.exe`。

**macOS：** 安装 Xcode 或 Xcode 命令行工具，运行 `bash macos/build.sh`，然后打开 `macos/build/meltycat.app`。

## 开源许可证

MeltyCat 是开源软件，采用 [MIT 许可证](LICENSE)。欢迎使用、修改、分享，也欢迎把它变成你自己的猫。

用 MeltyCat 做了什么好玩的，或者想一起合作，欢迎在 [Issues](../../issues) 里告诉我们。

## 致谢

灵感来自 [KuroCat](https://github.com/chanke17/KuroCat) 和 [kurocat](https://github.com/qianyiai/kurocat)。
