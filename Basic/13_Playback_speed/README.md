# GStreamer 基础教程 13：播放速度与方向（Playback speed)

本文是对 `basic-tutorial-13.c` 的中文学习笔记，演示如何通过发送 `seek` 与 `step` 事件实现倍速播放、减速播放、倒放以及单步前进等功能。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/playback-speed.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 `playbin` 构建播放器，加载远程媒体。
- 通过 `gst_event_new_seek()` 改变播放速率（支持负速率倒放）。
- 通过 `gst_event_new_step()` 在暂停时进行逐帧前进（单步）。
- 在键盘输入回调中动态切换播放/暂停、增减速率、改变播放方向、单步和退出。
- 认识 `GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE` 的含义与取舍。

---

## 代码结构概览（basic-tutorial-13.c）

- 结构体 `CustomData`：
  - `pipeline`: 顶层 `playbin` 元素。
  - `video_sink`: 视频渲染器（用于向下游发送 seek/step 事件）。
  - `loop`: GLib 主循环。
  - `playing`: 当前是否在播放（Play/Pause）。
  - `rate`: 当前播放速率（`gdouble`，可为负数表示倒放）。

- 发送速率变更事件 `send_seek_event(CustomData* data)`：
  1. 使用 `gst_element_query_position(..., GST_FORMAT_TIME, &position)` 获取当前播放位置（纳秒）。
  2. 构造 `seek` 事件：
     - 正速率（`rate > 0`）：`start = position`，`stop = end`（直到流结尾）。
     - 负速率（`rate <= 0`）：`start = 0`，`stop = position`（从当前位置向前回退至开头）。
     - 统一使用：`GST_FORMAT_TIME`，`GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE`。
  3. 若 `video_sink` 为空，通过 `g_object_get(pipeline, "video-sink", &video_sink, NULL)` 获取。
  4. 通过 `gst_element_send_event(video_sink, seek_event)` 发送事件，并打印当前速率。

- 键盘输入回调 `handle_keyboard(...)`：
  - `P`：在 `PLAYING` 与 `PAUSED` 间切换。
  - `S`/`s`：分别将速率 ×2 或 ÷2，然后调用 `send_seek_event()` 应用新速率。
  - `D`：速率取反，实现正放/倒放切换，然后调用 `send_seek_event()`。
  - `N`：获取或复用 `video_sink`，调用 `gst_event_new_step(GST_FORMAT_BUFFERS, 1, ABS(rate), TRUE, FALSE)` 单步前进一帧（在 `PAUSE` 状态下效果更好）。
  - `Q`：退出主循环。

- `tutorial_main(...)` 入口：
  - 初始化 GStreamer 和 `CustomData`。
  - 打印快捷键说明。
  - 使用 `gst_parse_launch("playbin uri=...", NULL)` 创建 `pipeline`，加载 WebM 示例媒体。
  - 使用 `g_io_add_watch` 监听标准输入，将键盘事件交给 `handle_keyboard`。
  - `gst_element_set_state(pipeline, GST_STATE_PLAYING)` 启动播放，默认 `rate=1.0`。
  - 运行 `GMainLoop`，退出后清理资源。

---

## 播放速率与方向详解

- `gst_event_new_seek(rate, format, flags, start_type, start, stop_type, stop)`：
  - `rate`：播放速率。`1.0` 表示正常速度，`2.0` 表示 2 倍速，负值表示倒放。
  - `format`：本例使用 `GST_FORMAT_TIME`（纳秒）。
  - `flags`：
    - `GST_SEEK_FLAG_FLUSH`：刷新各元素的内部缓冲，丢弃旧数据，尽快跳转到新的播放段。
    - `GST_SEEK_FLAG_ACCURATE`：尽可能精确地寻址到指定时间戳，但可能牺牲性能；若不强调精度可去掉，或改用 `KEY_UNIT` 以对齐关键帧加速跳转。
  - 区间设置策略：
    - 正向播放：从“当前位置”到“流末尾”。
    - 反向播放：从“开头”到“当前位置”。

- 事件发送位置：
  - 教程将 `seek/step` 事件发送到 `video_sink`。`step` 是典型的 sink-only 事件。
  - 实战中也常见使用 `gst_element_seek(pipeline, ...)` 直接对整条管道寻址（尤其要同时兼顾音频）。
  - 是否生效取决于解复用器、解码器与渲染器对“trick mode（快进/倒放/单步）”的支持。

- 单步（Step）
  - 使用 `gst_event_new_step(GST_FORMAT_BUFFERS, 1, ABS(rate), TRUE, FALSE)`，即按“帧（buffer）”为单位推进 1 帧。
  - 建议处于 `PAUSED` 时使用，视觉更可控；`PLAYING` 状态下也可触发但观感上不明显。

---

## 工作流程

1. 创建 `playbin`，设置媒体 URI。
2. 切到 `PLAYING`，初始化 `rate = 1.0`。
3. 监听键盘输入：
   - 调整 `rate` 或切换方向后，发送 `seek` 事件立即生效。
   - 在暂停下发送 `step` 事件，完成逐帧。
4. 主循环响应用户操作，直到退出。

---

## 键盘操作速查
- P：播放/暂停 切换
- S：加速（速率 ×2）
- s：减速（速率 ÷2）
- D：正放/倒放 切换（速率取反）
- N：单步前进一帧（推荐在暂停时使用）
- Q：退出程序

---

## 典型输出（示例片段）
```
USAGE: Choose one of the following options, then press enter:
 'P' to toggle between PAUSE and PLAY
 'S' to increase playback speed, 's' to decrease playback speed
 'D' to toggle playback direction
 'N' to move to next frame (in the current direction, better in PAUSE)
 'Q' to quit
Setting state to PLAYING
Current rate: 2
Stepping one frame
...
```

---

## 常见问题与排错
- 倒放/倍速无效：
  - 相关元素可能不支持 trick mode（特别是某些容器或编解码器）。尝试更换媒体格式（如 H.264 in MP4/WebM 等）或更换 sink。
  - 尝试将 `seek` 直接发给 `pipeline`（使用 `gst_element_seek`），以确保全管道协同寻址。
- 定位不精确或速度慢：
  - 去掉 `GST_SEEK_FLAG_ACCURATE` 或改用 `GST_SEEK_FLAG_KEY_UNIT`，以关键帧为单位快速跳转。
- 单步看不到效果：
  - 在暂停时执行 `N`，并确认 `video_sink` 支持 step 事件。
- `Unable to retrieve current position.`：
  - 媒体未开始播放或不支持时间格式查询；稍后重试或检查管道状态/媒体类型。
- 跨平台注意：
  - Windows 与 Unix 的 `GIOChannel` 创建方式不同（代码里已区分）。确保在可交互的控制台运行。

---

## 关键 API 速查
- `gst_event_new_seek()`：创建速率/定位事件。
- `gst_element_send_event()`：向指定元素发送事件（本例用于 sink）。
- `gst_event_new_step()`：创建逐帧事件。
- `gst_element_query_position()`：查询当前播放位置。
- `gst_element_set_state()`：切换 `PLAYING/PAUSED/NULL`。
- `g_io_add_watch()`：监听标准输入，实现键盘控制。

---

## 小结
通过 `seek` 与 `step` 事件可以在 GStreamer 中实现倍速、减速、倒放与逐帧等播放控制。是否生效取决于媒体格式与管道元素对 trick mode 的支持。示例程序提供了一个简洁、实用的交互框架，可作为开发自定义播放器控制的基础范式。
