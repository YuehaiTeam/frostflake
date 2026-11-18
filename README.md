# cocogoat-control
基于HTTP协议、带有授权机制的键鼠控制工具。

**所有操作都需要先经用户授权同意。**

### Windows 安装

首次运行发行版时，可浏览或输入本地磁盘上的上级文件夹。选择 `H:\GameTools` 会安装到 `H:\GameTools\cocogoat-control`，选择 `H:\` 会安装到 `H:\cocogoat-control`。路径末尾已有 `cocogoat-control` 时直接使用该目录。上级文件夹可以已有其他软件，首次安装使用新建或空的应用子文件夹。路径支持中文和空格。点击取消即退出安装。

再次运行新版发行包会沿用 Windows 卸载项中保存的安装目录。默认目录沿用原有的管理员计划任务；自定义目录下启动插件时由 Windows 请求管理员确认。卸载时会清理主程序和有安装记录的组件，目录里的其他文件会保留。

安装器使用[椰羊网页端的页头标志](https://github.com/YuehaiTeam/cocogoat/blob/main/src/components/Icons/cocogoat.vue)和[浅色主题](https://github.com/YuehaiTeam/cocogoat/blob/main/src/style/theme.scss)。图像授权见 [installer-logo.LICENSE](installer-logo.LICENSE)。

### 暴露的函数：
```
mouse_event
keybd_event
SetCursorPos
SendMessage
```
### 其他功能：
 - 获取窗口列表
 - 获取特定窗口位置
 - 获取显示器dpi信息

### 使用的开源库
 - [Win32GUI](https://github.com/Onelio/Win32GUI)
 - [json11](https://github.com/dropbox/json11)
 - [httplib](https://github.com/yhirose/cpp-httplib)
