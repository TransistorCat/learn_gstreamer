# GStreamer 基础教程 12：网络流播放 (Streaming)

本文是对 `basic-tutorial-12.c` 的中文学习笔记，演示了如何使用 `playbin` 播放网络流，并处理流媒体播放中常见的缓冲（buffering）和时钟丢失（clock lost）等问题。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/streaming.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 `playbin` 播放一个网络 URI。
- 监听 GstBus 上的消息，对流媒体状态进行响应。
- **处理缓冲消息** (`GST_MESSAGE_BUFFERING`)：在数据缓冲完成前将管道置于 `PAUSED` 状态，并向用户显示进度。
- **处理时钟丢失消息** (`GST_MESSAGE_CLOCK_LOST`)：当网络时钟不稳定时，重置时钟以恢复同步。
- **识别直播流**：通过 `gst_element_set_state()` 的返回值 `GST_STATE_CHANGE_NO_PREROLL` 判断流是否为直播（live），直播流没有预缓冲（pre-roll）过程。

---

## 代码结构概览（basic-tutorial-12.c）

- **`CustomData` 结构体**：除了保存 `pipeline` 和 `loop` 指针外，新增了一个布尔型成员 `is_live`，用于标记当前播放的是否为直播流。

- **`cb_message()` 回调函数**：这是本教程的核心。通过一个 `switch` 语句处理来自 GstBus 的不同消息：
  - `GST_MESSAGE_ERROR`: 打印错误并退出。
  - `GST_MESSAGE_EOS`: 流结束，退出。
  - `GST_MESSAGE_BUFFERING`: 
    - 如果是直播流 (`is_live` 为 `TRUE`)，则忽略此消息。
    - 否则，解析出缓冲百分比并打印。
    - 如果缓冲未满 100%，将管道状态设为 `PAUSED`，暂停播放。
    - 如果缓冲已满 100%，将管道状态设为 `PLAYING`，开始或恢复播放。
  - `GST_MESSAGE_CLOCK_LOST`: 接收端时钟与发送端失去同步。通过将管道短暂地切换到 `PAUSED` 再切回 `PLAYING` 来请求一个新的时钟。

- **`main()` 函数**：
  - `gst_init()` 初始化 GStreamer。
  - 使用 `gst_parse_launch()` 创建一个 `playbin` 元素，并指定一个网络 URI。
  - 调用 `gst_element_set_state(pipeline, GST_STATE_PLAYING)` 尝试开始播放。
  - **关键**：检查 `set_state` 的返回值。如果等于 `GST_STATE_CHANGE_NO_PREROLL`，说明这是一个没有预缓冲阶段的直播流，此时设置 `data.is_live = TRUE`。
  - 为 GstBus 添加信号监听，并将 `cb_message` 连接到 `message` 信号。
  - 启动 `GMainLoop`，等待事件和消息。

---

## 工作流程

1.  **创建管道**：使用 `gst_parse_launch` 创建 `playbin` 并加载一个网络媒体 URI。
2.  **启动与状态检查**：尝试将管道置于 `PLAYING` 状态。在这一步，通过检查返回值判断流的类型（点播 vs 直播）。
3.  **进入主循环**：程序开始等待总线消息。
4.  **处理缓冲（点播流）**：
    - `playbin` 开始下载数据，并发出 `GST_MESSAGE_BUFFERING` 消息。
    - `cb_message` 捕获该消息，打印百分比。
    - 在缓冲到 100% 之前，程序强制将 `playbin` 置于 `PAUSED` 状态，避免因数据不足而播放卡顿。
    - 缓冲完成后，程序将 `playbin` 置于 `PLAYING` 状态，视频开始流畅播放。
5.  **处理时钟丢失**：如果网络抖动等原因导致时钟不同步，GStreamer 会发出 `GST_MESSAGE_CLOCK_LOST` 消息。回调函数通过重置状态来恢复时钟同步。
6.  **播放结束**：收到 `EOS` 或 `ERROR` 消息后，程序清理资源并退出主循环。

---

## 典型输出（示例片段）

对于一个点播网络视频，你可能会看到类似下面的输出：

```
Buffering (  3%)
Buffering ( 12%)
Buffering ( 25%)
...
Buffering ( 98%)
Buffering (100%)
(视频开始播放)

... (播放结束) ...

Got EOS
```

--- 

## 常见问题与排错
- **无法播放/连接超时**：
  - 确认 URI 地址是否有效，以及你的网络连接是否正常，有无防火墙限制。
- **持续缓冲/缓冲速度慢**：
  - 这是网络带宽不足或服务器响应慢的典型表现。代码中的缓冲机制就是为了应对这种情况，但如果网络状况极差，仍可能无法流畅播放。
- **收到 `CLOCK_LOST` 消息**：
  - 这通常与不稳定的网络（高延迟、抖动）或发送端时钟问题有关。本教程中的状态重置是一种有效的恢复策略。

---

## 关键 API 速查
- **`gst_element_set_state()`**
  - 返回 `GstStateChangeReturn` 枚举值。`GST_STATE_CHANGE_NO_PREROLL` 是判断直播流的关键。
- **`GstMessage` 类型**
  - `GST_MESSAGE_BUFFERING`: 流正在缓冲数据。
  - `GST_MESSAGE_CLOCK_LOST`: 需要一个新的时钟来与数据流同步。
- **`gst_message_parse_buffering()`**
  - 从 `GST_MESSAGE_BUFFERING` 消息中解析出缓冲进度的百分比（0-100）。

---

## 小结

`playbin` 极大地简化了媒体播放，但对于网络流，一个健壮的播放器必须处理好缓冲和时钟同步等动态事件。通过监听 GstBus 上的特定消息，应用程序可以精细地控制播放行为，从而在不稳定的网络环境下提供更佳的用户体验。本教程展示了处理这些核心问题的标准模式。
