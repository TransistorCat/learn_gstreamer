# GStreamer 基础教程 8：短路管线（Short-cutting the pipeline）

本文是对 `basic-tutorial-8.c` 的中文学习笔记，演示如何用 `appsrc` 生成音频数据，通过 `tee` 分成多路，其中一路正常播放、一路做可视化、另一路用 `appsink` 将数据“短路/截流”回到应用层处理。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/short-cutting-the-pipeline.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 `appsrc` 从应用侧推送 16-bit、44100Hz、单声道 PCM 音频数据。
- 使用 `tee` 将同一数据源分三路：
  - 音频播放：`audioconvert ! audioresample ! autoaudiosink`
  - 可视化：`audioconvert ! wavescope ! videoconvert ! autovideosink`
  - 截流到应用：`appsink` 拉取样本（打印“*”）
- 正确设置 Buffer 时间戳与时长，确保下游同步：
  - `GST_BUFFER_TIMESTAMP` 与 `GST_BUFFER_DURATION`
  - `appsrc` 需设置 `"format"=GST_FORMAT_TIME`
- 了解 `tee` 的 Request pads 使用方式（申请、链接、释放），以及在每个分支放置 `queue` 的必要性。
- 管道错误处理：监听 `GstBus` 上的 `message::error` 并退出主循环。

---

## 代码结构概览（basic-tutorial-8.c）

- 结构体 `CustomData`
  - 管线与元素：
    - 源与分流：`appsrc`、`tee`
    - 音频分支：`queue`、`audioconvert`、`audioresample`、`autoaudiosink`
    - 可视化分支：`queue`、`audioconvert`、`wavescope`、`videoconvert`、`autovideosink`
    - 应用分支：`queue`、`appsink`
  - 波形状态参数与计数：`num_samples`、`a,b,c,d`
  - `sourceid`（用于控制 `g_idle_add` 注册的推送回调）、`GMainLoop* main_loop`

- 数据推送 `push_data(CustomData* data)`
  - 每次分配 `CHUNK_SIZE = 1024` 字节的 `GstBuffer`
  - 写入 16-bit PCM（单声道），生成简单的“迷幻”波形
  - 设置时间戳与时长：
    - `timestamp = scale(num_samples_total, GST_SECOND, SAMPLE_RATE)`
    - `duration = scale(num_samples_in_buffer, GST_SECOND, SAMPLE_RATE)`
  - `g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret)`

- 数据流控信号
  - `start_feed(appsrc, size, data)`：接到 `need-data` 时，用 `g_idle_add` 将 `push_data` 挂到主循环空闲回调中开始推送
  - `stop_feed(appsrc, data)`：接到 `enough-data` 时，`g_source_remove(sourceid)` 停止推送

- 应用侧截流 `new_sample(appsink, data)`
  - `pull-sample` 拿到 `GstSample`，示例里仅打印一个“*”表征收到数据，然后 `gst_sample_unref(sample)`

- 主流程 `main`
  - 初始化与创建元素，配置 `wavescope` 属性：`shader=0, style=0`
  - 用 `GstAudioInfo` 生成 `caps`，并设置给 `appsrc` 与 `appsink`
  - 连接 appsrc 的 `need-data`/`enough-data`，appsink 的 `new-sample`
  - `gst_bin_add_many` 加入管线，能自动链接的用 `gst_element_link_many` 连接
  - `tee` 使用 Request pads 手动链接到各分支的 `queue`：
    - `gst_element_request_pad_simple(tee, "src_%u")`
    - 取得各 `queue` 的静态 `sink` pad 并 `gst_pad_link`
  - 监听 `bus` 的 `message::error`，出错时打印并退出主循环
  - 播放、运行主循环、退出后释放 `tee` 的 Request pads、置 `NULL`、释放引用

---

## 工作流程

1. 初始化 GStreamer，创建所有元素与空管线。
2. 配置 `appsrc` 与 `appsink` 的 `caps`，连接数据流控与样本回调。
3. 将元素加入管线并链接可自动链接的部分。
4. 为 `tee` 申请三个 Request pads，分别与三条分支的 `queue` 的 `sink` pad 手动链接。
5. 设置管线到 `PLAYING`；主循环运行。
6. 当 `appsrc` 需要数据时，开始空闲回调驱动的周期性 `push-buffer`。
7. 三路同时接收相同的音频数据：
   - 音频分支播放声音
   - 可视化分支渲染 `wavescope`
   - 应用分支 `appsink` 收到样本并打印“*”
8. 错误发生时退出主循环，释放所有资源与 `tee` 的 Request pads。

---

## 典型输出（示例片段）

```
Obtained request pad src_0 for audio branch.
Obtained request pad src_1 for video branch.
Obtained request pad src_2 for app branch.
Start feeding
******************...（appsink 每收到一个样本打印一个“*”）
...
Stop feeding
```

实际输出会根据平台、插件可用性与运行时状态略有不同。

---

## 常见问题与排错

- Elements could not be created / plugin 不存在
  - 确认安装了相应的 GStreamer 插件（如 `autoaudiosink`、`autovideosink`、`wavescope` 属于常见的 good/bad 插件包）。
- 无法看到可视化窗口
  - 检查是否有 GUI 环境、视频输出插件是否可用；`autovideosink` 在不同平台选择不同后端。
- 播放卡顿或阻塞
  - 确保 `tee` 的每个分支后都有一个 `queue`，避免分支相互阻塞。
- 音视频不同步或抖动
  - 检查 `appsrc` 是否设置 `"format"=GST_FORMAT_TIME`，以及 Buffer 的时间戳/时长是否正确。
- 退出时泄漏
  - 记得释放 `tee` 的 Request pads：`gst_element_release_request_pad()`，并 `gst_object_unref()`。

---

## 关键 API 速查

- appsrc
  - `g_object_set(appsrc, "caps", caps, "format", GST_FORMAT_TIME, NULL)`
  - `g_signal_connect(appsrc, "need-data", start_feed, ...)`
  - `g_signal_connect(appsrc, "enough-data", stop_feed, ...)`
  - `g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret)`
- appsink
  - `g_object_set(appsink, "emit-signals", TRUE, "caps", caps, NULL)`
  - `g_signal_connect(appsink, "new-sample", new_sample, ...)`
  - `g_signal_emit_by_name(appsink, "pull-sample", &sample)`
- tee Request pads
  - `gst_element_request_pad_simple(tee, "src_%u")`
  - `gst_element_release_request_pad(tee, pad)`
- 时间与音频格式
  - `gst_util_uint64_scale(samples, GST_SECOND, SAMPLE_RATE)`
  - `gst_audio_info_set_format()` / `gst_audio_info_to_caps()`
- 链接与消息
  - `gst_element_link_many()` / `gst_pad_link()`
  - `gst_bus_add_signal_watch()` + `message::error`

---

## 小结

本教程展示了如何用 `appsrc` 生成音频数据，并通过 `tee` 同时驱动“播放、可视化、应用截流”三条分支。要点在于：
- 为 `appsrc` 提供正确的 caps 与纳秒时间基的时间戳
- 为 `tee` 的每个分支放置 `queue`
- 正确申请与释放 `tee` 的 Request pads

这是一种非常通用的模式：在保持常规播放/处理的同时，把同一份数据“短路”回应用层做统计、分析、录制或网络发送等扩展处理。