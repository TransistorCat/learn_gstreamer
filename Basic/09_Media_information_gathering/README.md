# GStreamer 基础教程 9：媒体信息收集（GstDiscoverer）

本文是对 basic-tutorial-9.c 的中文学习笔记，演示如何使用 `GstDiscoverer` 在“不播放”的前提下解析媒体（本地文件或网络 URI），获取媒体的总体信息（时长、是否可 seek）、每路流的编解码信息、以及标签（metadata）。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/media-information-gathering.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 `GstDiscoverer` 对媒体 URI 进行探测，可同步或异步。
- 获取：
  - 总体信息：时长、是否可寻址（seekable）、整体标签。
  - 拓扑结构：容器 ➜ 音/视频/字幕等子流。
  - 每条流的能力（caps）与标签；若 caps 固定，可得到人类可读的“编解码器描述”。
- 异步模式通过 GLib 主循环和信号回调实现，适合批量处理多个 URI。

---

## 代码结构概览（basic-tutorial-9.c）

- 自定义数据结构 `CustomData`：保存 `GstDiscoverer*` 与 `GMainLoop*`。
- `print_tag_foreach()`：遍历并打印标签（name: value），对非字符串类型统一序列化输出。
- `print_stream_info(info, depth)`：
  - 取 `caps = gst_discoverer_stream_info_get_caps(info)`；
  - 若 caps 固定（`gst_caps_is_fixed`），用 `gst_pb_utils_get_codec_description(caps)` 得到友好描述；否则输出 caps 字符串；
  - 打印该流的类型昵称（audio/video/subtitle/container 等）与标签。
- `print_topology(info, depth)`：递归打印流拓扑：
  - 若 `get_next(info)` 有下游流，继续向下打印；
  - 否则若是容器（`GstDiscovererContainerInfo`），枚举其子流列表并逐个打印。
- `on_discovered_cb()`（信号 `"discovered"`）：
  - 根据 `GstDiscovererResult` 打印状态（OK、URI_INVALID、ERROR、TIMEOUT、BUSY、MISSING_PLUGINS 等）。
  - OK 时输出：时长、整体标签、是否可 seek、并打印顶层流拓扑信息。
- `on_finished_cb()`（信号 `"finished"`）：所有 URI 探测完成时退出主循环。
- `main()`：
  - 解析 URI（默认使用官方示例 webm 资源）。
  - `gst_init()` 初始化；创建 `GstDiscoverer`（超时 5 秒）；连接回调信号；
  - `gst_discoverer_start()` 启动；`gst_discoverer_discover_uri_async()` 提交异步任务；
  - 进入 `g_main_loop_run()` 等待回调；结束后停止并释放资源。

---

## 工作流程（异步模式）
1. 创建 `GstDiscoverer` 并 `start()`。
2. 调用 `gst_discoverer_discover_uri_async()` 提交一个或多个 URI。
3. 主循环等待：每个 URI 完成时触发 `"discovered"` 回调；全部完成后触发 `"finished"` 回调。
4. 在回调中从 `GstDiscovererInfo` 读取：
   - `get_duration()`、`get_seekable()`、整体 `get_tags()`；
   - 顶层 `get_stream_info()`，并通过 `print_topology` 递归遍历所有子流；
   - 每条流读取 `caps`、`tags`，必要时取得 codec 描述。

---

## 典型输出（示例片段）
```
Discovered 'https://.../sintel_trailer-480p.webm'

Duration: 0:00:52.345678901
Tags:
  title: Sintel Trailer
Seekable: yes

Stream information:
  container: Matroska/WebM
    video: VP8
    audio: Vorbis
```
实际输出会根据媒体不同而变化，标签也可能包含艺术家、语言、比特率等信息。

---

## 常见问题与排错
- Missing plugins：
  - 日志若出现 `Missing plugins: ...`，说明系统缺少解码器/解析器。根据提示安装相应插件包（如 `gstreamer1.0-libav`、`gstreamer1.0-plugins-{good,bad,ugly}` 等）。
- Timeout（超时）：
  - 创建 `GstDiscoverer` 时的超时可调（示例为 5 秒）。网络慢或资源体积大时可适当增大。
- URI 无效：
  - 确保带有正确协议（file://、http(s):// 等）。Windows 本地路径需要转换为 file URI。

---

## 扩展与自定义

### 1) 获取更细粒度的音/视频参数
可在 `print_stream_info()` 中对 `info` 进行类型判断，并读取更具体的参数：
```c
if (GST_IS_DISCOVERER_AUDIO_INFO (info)) {
  GstDiscovererAudioInfo *ai = GST_DISCOVERER_AUDIO_INFO (info);
  g_print ("%*sAudio details: channels=%u rate=%u language=%s\n", 2*depth, " ",
           gst_discoverer_audio_info_get_channels (ai),
           gst_discoverer_audio_info_get_sample_rate (ai),
           gst_discoverer_audio_info_get_language (ai) ?: "unknown");
}

if (GST_IS_DISCOVERER_VIDEO_INFO (info)) {
  GstDiscovererVideoInfo *vi = GST_DISCOVERER_VIDEO_INFO (info);
  gint w = gst_discoverer_video_info_get_width (vi);
  gint h = gst_discoverer_video_info_get_height (vi);
  gint fn = gst_discoverer_video_info_get_framerate_num (vi);
  gint fd = gst_discoverer_video_info_get_framerate_denom (vi);
  g_print ("%*sVideo details: %dx%d, fps=%g\n", 2*depth, " ", w, h,
           fd ? (double) fn / fd : 0.0);
}
```

### 2) 同步探测（简单但阻塞）
如果不需要异步与主循环，可以改用同步 API：
```c
GError *err = NULL;
GstDiscoverer *disc = gst_discoverer_new (5 * GST_SECOND, &err);
GstDiscovererInfo *info = gst_discoverer_discover_uri (disc, uri, &err);
if (!info) { /* 处理错误 */ }
/* 使用 info 打印/提取信息 */
g_clear_error (&err);
g_object_unref (disc);
```

### 3) 批量 URI
- 在异步模式下可多次调用 `gst_discoverer_discover_uri_async(disc, uriX)`。
- 全部提交后等待 `"finished"` 信号统一退出。

### 4) 输出为结构化数据
- 可将发现结果组织为 JSON（如 cJSON/GLib JSON），便于与其它系统集成（媒体库、索引服务等）。

---

## 关键 API 速查
- 创建与控制：
  - `gst_discoverer_new()`, `gst_discoverer_start()`, `gst_discoverer_stop()`
  - `gst_discoverer_discover_uri()`（同步）
  - `gst_discoverer_discover_uri_async()`（异步）
- 回调信号：`"discovered"`, `"finished"`
- 结果对象：`GstDiscovererInfo`
  - `gst_discoverer_info_get_result()`, `gst_discoverer_info_get_uri()`
  - `gst_discoverer_info_get_duration()`, `gst_discoverer_info_get_seekable()`
  - `gst_discoverer_info_get_tags()`, `gst_discoverer_info_get_stream_info()`
- 流信息：`GstDiscovererStreamInfo` 及子类
  - `gst_discoverer_stream_info_get_caps()`, `gst_discoverer_stream_info_get_tags()`
  - `gst_discoverer_stream_info_get_next()`
  - 容器：`GstDiscovererContainerInfo` ➜ `gst_discoverer_container_info_get_streams()`
  - 音频：`GstDiscovererAudioInfo`（声道、采样率、语言等）
  - 视频：`GstDiscovererVideoInfo`（分辨率、帧率等）
- 标签操作：`GstTagList`, `gst_tag_list_foreach()`
- 编解码描述：`gst_pb_utils_get_codec_description(caps)`

---

## 小结
`GstDiscoverer` 提供了轻量、结构化的媒体分析能力，适合用于：
- 媒体库扫描与索引
- 媒体信息面板/属性页
- 批量验证、自动化处理的前置探测

在此基础上，你可以按需扩展输出格式、增加更详尽的参数打印，或集成到你的应用中作为媒体“探针”。
