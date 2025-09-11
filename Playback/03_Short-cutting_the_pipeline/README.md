# GStreamer 回放教程 3：Short-cutting the pipeline（使用 playbin 快速播放）

本文是对 playback-tutorial-3.c 的中文学习笔记，演示如何用高层封装元素 `playbin` 以最少代码完成媒体播放，并在播放即将结束时通过 `about-to-finish` 信号实现无缝切换（gapless）。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/playback/short-cutting-the-pipeline.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 `playbin` 代替手动拼装 `source → demux → decode → convert → sink` 的复杂管线：
  - 只需设置 `uri` 属性即可播放本地/网络媒体。
  - 可替换默认的音/视频/字幕 sink（如 `audio-sink`、`video-sink`、`text-sink`）。
  - 可通过 `flags` 快速启用/禁用音频、视频、字幕、可视化等。
- 通过信号 `about-to-finish` 在媒体结束前设置下一个 `uri`，实现无缝衔接（gapless）。
- 处理 Bus 消息（ERROR、EOS、STATE_CHANGED、TAG 等），维持主循环或轮询以保持应用活跃。
- 重要属性：`uri`、`volume`、`mute`、`current-audio`、`current-text`、`suburi`（外挂字幕）与 `flags`。

---

## 代码结构概览（playback-tutorial-3.c）
- 创建 `playbin`：`gst_element_factory_make("playbin", "player")`。
- 设置媒体 `uri`（本地文件需使用 `file://` 前缀；Windows 举例：`file:///C:/path/to/file.mp3`）。
- 可选：设置自定义输出 sink（`audio-sink`、`video-sink`、`text-sink`）或调整 `flags`（开关音视频/字幕/可视化）。
- 连接 `about-to-finish` 回调：在媒体缓冲将尽时设置下一首 `uri`，以无缝衔接。
- 切换到 `GST_STATE_PLAYING`，并通过 `GstBus` 处理消息：
  - `ERROR`：解析错误（`gst_message_parse_error`）并退出；
  - `EOS`：若未切换下一首则在流尾退出；
  - `STATE_CHANGED`：打印 `playbin` 的状态演进（NULL/READY/PAUSED/PLAYING）；
  - `TAG`：读取媒体标签（标题、艺术家、专辑等）。
- 退出前：将元素设为 `GST_STATE_NULL` 并释放 `bus` 与 `playbin` 引用。

---

## 工作流程（单媒体与播放列表）
1. `gst_init()` 初始化；创建 `playbin` 并设置首个 `uri`。
2. 可选：配置 `flags` 与自定义 `audio-sink`/`video-sink`/`text-sink`。
3. 连接 `about-to-finish`：在回调中设置下一首 `uri`（播放列表可从队列中取下一条）。
4. `gst_element_set_state(playbin, GST_STATE_PLAYING)` 启动播放。
5. 处理 `GstBus` 消息：`ERROR`、`EOS`、`STATE_CHANGED`、`TAG`。
6. 结束：将元素置为 `NULL`，释放资源并退出。

---

## 典型输出（示例片段）
```
State changed: NULL -> READY
State changed: READY -> PAUSED
State changed: PAUSED -> PLAYING
About to finish, switch to next: https://.../next_song.ogg
Tags: title=Sintel Trailer, artist=..., album=...
EOS
```
实际输出会根据媒体、平台与插件不同而变化。

---

## 常见问题与排错
- URI 格式：本地路径要使用 `file://`。Windows 路径需转换为 file URI（例如 `file:///C:/path/to/file.mp3`）。
- 缺少插件（Missing plugin）：Bus/日志若提示缺少解码器/解析器，请安装 `gstreamer1.0-plugins-{good,bad,ugly}`、`gstreamer1.0-libav` 等发行版包。
- `about-to-finish` 不触发：
  - 确保处于 `PLAYING` 且确实接近尾部；
  - 某些网络流或直播源不会触发；
  - 切换 `uri` 要在回调内立即进行，且不要在回调里做耗时操作。
- 切换过晚导致停顿：尽量在第一次 `about-to-finish` 回调里就设置下一个 `uri`；复杂播放列表可预先维护队列并快速取用。

---

## 扩展与自定义

### 1) gapless 播放（about-to-finish）
在媒体结束前触发回调，立即切换下一 `uri` 以无缝衔接。
```c
static void on_about_to_finish(GstElement *playbin, gpointer user_data) {
  const char *next_uri = (const char *)user_data; // 示例：仅演示单个“下一首”
  g_print("About to finish, switch to next: %s\n", next_uri);
  g_object_set(playbin, "uri", next_uri, NULL);
}
```
提示：不要等到 `EOS` 再切换，否则会产生可感知停顿。

### 2) playbin 属性与 flags
常用属性：`uri`、`suburi`（或 `subtitle-uri`，以 `gst-inspect-1.0 playbin` 为准）、`volume`、`mute`、`current-audio`、`current-text`、`audio-sink`、`video-sink`、`text-sink`。
通过 `flags` 启用/禁用功能（音频/视频/字幕/可视化等）：
```c
unsigned int flags = 0;
g_object_get(playbin, "flags", &flags, NULL);
flags &= ~GST_PLAY_FLAG_VIDEO;  // 关闭视频，仅播音频
flags |=  GST_PLAY_FLAG_AUDIO;  // 开启音频
flags |=  GST_PLAY_FLAG_TEXT;   // 需要字幕可打开
g_object_set(playbin, "flags", flags, NULL);
```

### 3) 自定义 sink 与嵌入式渲染
- 视频：`autovideosink`、`glimagesink`、`ximagesink`、`d3d11videosink`、`waylandsink`。
- 音频：`autoaudiosink`、`pulsesink`、`alsasink`、`osxaudio`、`directsoundsink`/`wasapisink`。
可将 `video-sink` 嵌入到 GUI 窗口（不同 sink 方式不同，请参考其文档）。仅音频时关闭视频 flag 可节省资源。

### 4) 标签获取与外挂字幕/多音轨
- 在 Bus 的 `TAG` 消息中解析 `GstTagList` 获取元数据：
```c
case GST_MESSAGE_TAG: {
  GstTagList *tags = NULL;
  gst_message_parse_tag(msg, &tags);
  if (tags) {
    gchar *s = gst_tag_list_to_string(tags);
    g_print("Tags: %s\n", s);
    g_free(s);
    gst_tag_list_unref(tags);
  }
  break;
}
```
- 外挂字幕：启动或切换时设置 `suburi`（或 `subtitle-uri`）。
- 多音轨/字幕选择：设置 `current-audio`、`current-text` 索引。

---

## 关键 API 速查
- 元素与属性：
  - `playbin`（`uri`、`suburi`/`subtitle-uri`、`audio-sink`、`video-sink`、`text-sink`、`volume`、`mute`、`flags`、`current-audio`、`current-text`）
- 信号：
  - `about-to-finish`（无缝切歌/切片）；
  - `source-setup`（获取并配置 `urisourcebin`，可调缓冲策略如 `queue2` 属性）；
  - `video-tags`、`audio-tags`、`text-tags`（部分版本）。
- Bus 消息：
  - `ERROR`、`EOS`、`STATE_CHANGED`、`WARNING`、`TAG`。
- 标签操作：
  - `GstTagList`，`gst_message_parse_tag()`，`gst_tag_list_to_string()`/`gst_tag_list_foreach()`。
- 常用 sink：
  - 音频：`autoaudiosink`、`pulsesink`、`alsasink`、`osxaudio`、`directsoundsink`/`wasapisink`；
  - 视频：`autovideosink`、`glimagesink`、`ximagesink`、`d3d11videosink`、`waylandsink`。

---

## 小结
`playbin` 将“搭管线”的复杂性隐藏在内部，使你可以在几行代码内完成媒体播放；结合 `about-to-finish` 信号即可实现 gapless 播放。通过 `flags` 与自定义 sink，你可以在保留高层易用性的同时，获得足够的可定制性，适用于播放器原型、工具类程序或嵌入式多媒体应用。
