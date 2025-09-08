# GStreamer 基础教程 1：Hello World（playbin 与最简播放）

本文是对 Basic Tutorial 1（Hello world）的中文学习笔记，演示如何用一段极简 C 代码，借助高度集成的 `playbin` 元素播放一段在线媒体。通过本例可以快速跑通 GStreamer 的初始化、构建管线、设置状态、处理总线消息以及资源清理的基本流程。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/hello-world.html?gi-language=c

---

## 目标与要点
- 使用 `gst_parse_launch()` 与 `playbin` 构建最小可用的播放管线。
- 掌握基础生命周期：`gst_init()` → 构建/配置 → `set_state(PLAYING)` → 等待 `ERROR/EOS` → 清理。
- 了解 Bus（总线）消息的基本处理方式。
- 熟悉 URI 播放（http/https）与 `gst-launch-1.0` 的等价命令。

---

## 示例代码概览（basic-tutorial-1.c）

核心思路：用 `gst_parse_launch()` 一行描述管线，创建 `playbin` 并设置播放 URI，然后播放直至出错或到达流末尾（EOS）。

关键片段：
- 初始化：`gst_init(&argc, &argv)`
- 构建管线：
  - `pipeline = gst_parse_launch("playbin uri=<...>", NULL);`
  - 等价命令行：`gst-launch-1.0 playbin uri=<...>`
- 开始播放：`gst_element_set_state(pipeline, GST_STATE_PLAYING)`
- 等待消息：
  - 通过 `bus = gst_element_get_bus(pipeline)` 获取 Bus
  - `gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS)` 阻塞等待 ERROR 或 EOS
- 结束与清理：
  - `gst_message_unref(msg); gst_object_unref(bus);`
  - `gst_element_set_state(pipeline, GST_STATE_NULL);`
  - `gst_object_unref(pipeline);`
- macOS 包装：示例在 `main()` 中对 macOS 使用 `gst_macos_main()` 包装，这样更好地集成 Cocoa 事件循环。

---

## 什么是 playbin？

`playbin` 是一个“播放器一体机”元素：
- 内部自动选择并构建所需的解复用器、解码器、转换器以及音视频输出 sink。
- 你只需提供 `uri`（或 `current-uri`）属性，它会自动完成大多数播放构建工作。
- 适合“快速能播”的场景，复杂/可控的场景可改用 `uridecodebin` 或手工搭建管线。

常用属性（仅列举与本例相关）：
- `uri`：媒体地址（可为 file:// 或 http(s):// 等）
- `audio-sink` / `video-sink`：自定义音/视频输出元素（不指定则自动选择 `autoaudiosink`/`autovideosink`）

---

## 总线（Bus）与消息（Message）

GStreamer 通过 Bus 把元素的消息（如错误、EOS、状态变化）送达应用线程：
- `GST_MESSAGE_ERROR`：播放发生错误，使用 `gst_message_parse_error()` 可获取详细错误与调试信息。
- `GST_MESSAGE_EOS`：流结束（End Of Stream）。
- 更多消息类型（在后续教程会用到）：`STATE_CHANGED`、`WARNING`、`INFO`、`ASYNC_DONE` 等。

示例里使用同步阻塞的方式等待 ERROR / EOS；在较复杂应用中，可改用 `gst_bus_add_signal_watch()` + 回调的异步模式。

---

## 与 gst-launch-1.0 的对应关系

示例代码：
- `gst_parse_launch("playbin uri=https://.../sintel_trailer-480p.webm", NULL);`

等价命令：
- `gst-launch-1.0 playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm`

`gst_parse_launch()` 提供与命令行相同的语法糖，便于快速拼装与实验管线。

---

## 常见问题与排错

- 无法创建元素 / 无法播放：
  - 检查是否安装了基本插件集（good/bad/ugly、libav 等，按平台打包不同）。
  - 使用 `gst-inspect-1.0 playbin` 确认插件存在。
- 网络/证书问题（播放 https 资源失败）：
  - 确认系统信任的 CA 证书链；必要时尝试本地文件 `file:///path/to/media` 以排除网络问题。
- 无画面或无声音：
  - 尝试设置环境变量 `GST_DEBUG=3` 或更高，查看关键日志。
  - 在需要时指定 `audio-sink`/`video-sink` 为具体元素（例如 `pulsesink`、`alsasink`、`ximagesink`、`waylandsink` 等）。
- 程序退出后窗口残留或闪烁（某些平台）：
  - 确保在退出前把 pipeline 置为 `GST_STATE_NULL` 并释放所有引用。

---

## 进一步的练习

- 改为播放本地文件：
  - `playbin uri=file:///absolute/path/to/media.mp4`
- 打印更详细的错误信息：
  - 收到 `GST_MESSAGE_ERROR` 后使用 `gst_message_parse_error(msg, &err, &debug)`。
- 指定自定义 sink：
  - `g_object_set(playbin, "video-sink", gst_element_factory_make("xvimagesink", NULL), NULL);`
- 过渡到更可控的管线：
  - 使用 `uridecodebin` + `queue` + `audioconvert/videoconvert` + `auto*` sink，参见本仓库 Basic/03 与 Basic/07。

---

## 关键 API 速查

- 初始化与生命周期：
  - `gst_init()`、`gst_element_set_state()`、`gst_object_unref()`
- 构建：
  - `gst_parse_launch()`（快速构建）
  - `gst_pipeline_new()/gst_element_factory_make()`（手动构建，后续教程使用）
- Bus：
  - `gst_element_get_bus()`、`gst_bus_timed_pop_filtered()`
- 属性：
  - `g_object_set(element, "uri", <uri>, NULL)`

---

## 小结

Hello World 示例展示了使用 `playbin` 快速播放媒体的最短路径：初始化 → 构建 → 播放 → 等待消息 → 清理。它为后续教程铺垫了 Bus 消息处理、状态机与资源管理等基础知识。随着需求变复杂，你会逐步从 `playbin` 过渡到更细粒度的元素组合与动态管线。
