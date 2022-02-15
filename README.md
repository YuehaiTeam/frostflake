# cocogoat-control
基于HTTP协议、带有授权机制的键鼠控制工具。

**所有操作都需要先经用户授权同意。**

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