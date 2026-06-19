# pc_host 使用说明

`pc_host/` 是本项目的上位机程序，基于 **PyQt5 + pyserial** 实现，用于通过串口和 S800 / TM4C1294 板通信，并提供图形化控制界面。

## 1. 功能概览

当前上位机包含这些功能：

- 串口扫描、连接、断开
- 上电后等待设备 ready，再自动做初始状态同步
- 时间 / 日期 / 闹钟设置
- `DISPLAY`、`FORMAT`、`MODE` 等控制
- LED、蜂鸣器、虚拟按键控制
- 右侧数字孪生镜像面板
- 底部收发日志区
- 原始命令发送
- NTP 对时
- 自动昼夜模式
- 天气获取
- 简单数据图表导出

## 2. 依赖安装

依赖文件在：

- `pc_host/requirements.txt`

建议在 `pc_host/` 目录下创建本地虚拟环境：

```bash
cd pc_host
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

如果你已经有现成环境，也可以直接安装依赖后运行。

## 3. 启动方式

请在**项目根目录**启动，而不是在 `pc_host/` 目录里直接运行 `main.py`。

```bash
cd ..
python3 -m pc_host.main
```

如果你已经在项目根目录：

```bash
python3 -m pc_host.main
```

## 4. 基本使用流程

### 4.1 连接开发板

1. 将开发板连接到电脑串口
2. 启动上位机
3. 点击左侧 **刷新**，更新串口列表
4. 选择正确串口
5. 点击 **连接**

### 4.2 等待 ready

连接后，上位机不会立刻发初始化查询，而是先等待设备 ready。

当前逻辑是：

- 连接后先等待约 **10 秒**
- 然后自动发送 `*PING`
- 收到 `*PONG` 后进入 ready 状态
- 再自动执行初始 `GET` 同步

所以连接成功后，请先等状态栏进入 **READY**，再进行正式操作。

### 4.3 状态同步

当前状态同步方式分两层：

- **首次同步**：通过 `GET:DISPLAY / FORMAT / DATE / TIME / ALARM`
- **运行期更新**：通过串口应答、事件和本地 shadow 状态

注意：有些状态当前不是完全通过 `GET` 获取，而是靠最近一次命令或事件更新。

## 5. 界面说明

## 5.1 顶部状态栏

显示：

- 当前串口
- READY 状态
- RTT / 延迟
- `FORMAT`
- `MODE`
- `ALARM`

## 5.2 左侧控制区

左侧主要包含：

- 串口管理
- 时间 / 日期 / 闹钟设置
- 显示控制
- LED / 蜂鸣器控制
- 虚拟按键
- 协议演示
- 扩展功能
- 原始命令发送

## 5.3 右侧数字孪生区

右侧显示：

- 8 位显示内容
- LED 状态
- 最近按键事件
- 当前模式

## 5.4 底部日志区

底部日志区会记录：

- 发送命令
- 正常应答
- 事件上报
- 错误信息

可以用来排查通信问题。

## 6. 使用注意事项

### 6.1 命令节流

上位机内部做了统一命令调度，**相邻命令最小间隔为 500ms**。

因此：

- 不需要连续狂点按钮
- 如果点击后没有立刻发送下一条，是正常现象

### 6.2 ready 前禁止正式命令

如果设备还没 ready，上位机会阻止需要 ready 的命令发送，并在日志区提示。

### 6.3 网络功能需要联网

这些功能依赖网络：

- NTP 对时
- 天气获取

如果网络不可用，日志区会显示失败信息。

### 6.4 图表与历史文件

当前实现会生成这些文件：

- 历史 CSV：`pc_host/pc_host_history.csv`
- 图表图片：`pc_host_history.png`

说明：

- `pc_host_history.csv` 保存在 `pc_host/` 目录下
- `pc_host_history.png` 保存在**当前启动目录**
- 如果你是在项目根目录启动，上面的 PNG 会出现在项目根目录

## 7. 常用功能示例

### 7.1 设置时间

在左侧输入框填写类似：

```text
HOUR MINUTE SECOND 12 34 56
```

然后点击 **SET TIME**。

### 7.2 设置日期

输入类似：

```text
YEAR MONTH DATE 2026 06 18
```

然后点击 **SET DATE**。

### 7.3 设置闹钟

输入类似：

```text
HOUR MINUTE SECOND 07 30 00
```

然后点击 **SET ALARM**。

关闭闹钟可点击：

- **ALARM OFF**

### 7.4 控制显示和格式

可直接点击：

- `DISPLAY ON`
- `DISPLAY OFF`
- `FORMAT LEFT`
- `FORMAT RIGHT`

### 7.5 虚拟按键

左侧虚拟按键区支持：

- `FUNC`
- `SHIFT`
- `ADD`
- `SAVE`
- `DISP`
- `SPEED`
- `FORMAT`
- `EXT`
- `USER1`
- `USER2`

点击后会发送对应的 `*SET:KEY <NAME>`。

### 7.6 原始命令发送

如果需要直接验证协议，可以在调试区输入原始命令，例如：

```text
*PING
```

然后点击发送。

## 8. 自测命令

如果想快速确认当前 `pc_host` 相关测试都能跑：

```bash
QT_QPA_PLATFORM=offscreen python3 -m unittest discover -s pc_host/tests -v
```

## 9. 说明

这个上位机实现是按课程作业目标完成的，重点是：

- 功能可演示
- 串口链路清楚
- 界面便于验收
- 代码结构尽量简单

它不是生产级工具，因此一些实现会更偏向“够用、清楚、方便展示”。
