# GStreamer 基础教程 3：动态管线（Dynamic Pipelines）

本文是对 Basic Tutorial 3（Dynamic pipelines）的中文学习笔记，聚焦“运行期链接”的核心机制：当上游元素在播放过程中动态地产生新的 pads（Sometimes pads）时，如何捕捉并与下游链路完成连接。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/dynamic-pipelines.html?gi-language=c

---

## 目标与要点（知识点）
- 理解 GStreamer 中 Pad 的动态性：某些元素（如 decodebin、demuxer）在运行时才会创建 src pad。
- 监听并处理 "pad-added" 信号，在回调里判断新 pad 的媒体类型（caps），再完成与下游元素的链接。
- 区分静态（Always）pad 与动态（Sometimes）pad 的链接方式：
  - Always：元素创建时就存在，可在构建管线时用 `gst_element_link_many()` 静态链接。
  - Sometimes：运行中出现，必须在回调中通过 `gst_pad_link()` 手动链接。
- 基本的 Bus 消息处理（ERROR/EOS），以及主循环的使用。

---

## 典型管线（概念示意）

以本地文件为例，使用 filesrc + decodebin 自动解码，动态地把解码后的音频接到转换/重采样/音频输出链上：

- filesrc location=... ! decodebin name=dec
  - dec. —(当检测到音频流且 pad 出现时)→ audioconvert ! audioresample ! autoaudiosink

关键点：
- filesrc 和 decodebin 的链接可以在构建时静态完成（两者的 sink/src 均为 Always）。
- decodebin 的 src pad 是 Sometimes，只有在识别到某条媒体流后才会“长出来”。
- 新 pad 出现时通过 "pad-added" 回调判断其类型（如 `audio/x-raw`），然后把该 pad 链接到 `audioconvert` 的 sink。

---

## 代码结构概览

- 元素创建
  - pipeline、filesrc、decodebin
  - audioconvert、audioresample、autoaudiosink
- 静态链接
  - filesrc → decodebin（Always）
  - audioconvert → audioresample → autoaudiosink（Always）
- 动态链接
  - 连接 decodebin 的 "pad-added" 信号：当新 pad 出现时，检查 caps，若为 `audio/x-raw` 且未链接，则把该 pad 与 `audioconvert` 的 sink pad 链接。
- Bus 处理
  - 监听 ERROR/EOS，必要时退出主循环并清理资源。
- 主循环
  - 设置 pipeline 为 PLAYING，进入 `g_main_loop_run()` 等待消息和回调。

---

## pad-added 回调的典型实现（C 伪代码）

```c
static void pad_added_handler (GstElement *src, GstPad *new_pad, gpointer user_data) {
  CustomData *data = (CustomData *)user_data;
  GstPad *sinkpad = gst_element_get_static_pad (data->audioconvert, "sink");

  if (gst_pad_is_linked (sinkpad)) {
    g_print ("Sink pad already linked, ignoring\n");
    goto exit;
  }

  // 尝试获取新 pad 的 caps，用于判断媒体类型
  GstCaps *caps = gst_pad_get_current_caps (new_pad);
  if (!caps) caps = gst_pad_query_caps (new_pad, NULL);
  GstStructure *s = gst_caps_get_structure (caps, 0);
  const gchar *name = gst_structure_get_name (s); // e.g. "audio/x-raw"

  if (g_str_has_prefix (name, "audio/x-raw")) {
    if (gst_pad_link (new_pad, sinkpad) == GST_PAD_LINK_OK) {
      g_print ("Linked new pad to audioconvert sink\n");
    } else {
      g_printerr ("Failed to link new pad to audioconvert sink\n");
    }
  } else {
    g_print ("New pad type '%s' ignored\n", name);
  }

  gst_caps_unref (caps);
exit:
  gst_object_unref (sinkpad);
}
```

说明：
- `gst_pad_get_current_caps()` 若返回 NULL，可退而求其次使用 `gst_pad_query_caps()`。
- 仅当 caps 指示为 `audio/x-raw`（原始音频）时才与音频分支相连。其它类型暂不处理。
- 若要同时处理视频，可在回调中再判断 `video/x-raw`，并为视频分支准备对应的下游链（通常搭配 `videoconvert ! autovideosink`）。

---

## 主函数流程要点

1. `gst_init(&argc, &argv)`
2. 创建元素：pipeline、filesrc、decodebin、audioconvert、audioresample、autoaudiosink
3. 将元素加入管线：`gst_bin_add_many()`
4. 静态链接：
   - `gst_element_link(filesrc, decodebin)`
   - `gst_element_link_many(audioconvert, audioresample, audiosink, NULL)`
5. 连接回调：`g_signal_connect(decodebin, "pad-added", G_CALLBACK(pad_added_handler), &data)`
6. 设置 filesrc 的 `location` 属性
7. 设置状态为 PLAYING，进入主循环
8. 处理 Bus 消息（ERROR/EOS），退出时清理资源

---

## 与“多线程与 Pad 可用性（教程 7）”的关系

- 本教程（3）专注于“动态 pad 出现时如何在回调中完成链接”的最小套路，一般只处理单一路径（例如音频）。
- 教程 7 在此基础上进一步：
  - 同时处理音频与视频两条分支；
  - 在每条分支紧跟一个 `queue`，让分支在不同线程中并发处理，避免互相阻塞；
  - 更系统地阐述 Always/Sometimes/Request 三类 pad 的差异。

---

## 常见问题与排错

- 直接 `gst_element_link(decodebin, audioconvert)` 失败
  - 因为 decodebin 的 src 是 Sometimes pad，构建时不存在，必须在 "pad-added" 回调中用 `gst_pad_link()`。
- 回调没有触发
  - 确认上游是否正在播放且确实包含对应媒体流；检查 Bus 是否有 ERROR（缺插件/解码器）。
- `GST_PAD_LINK_REFUSED`
  - 检查 caps 是否为原始类型（`audio/x-raw` 或 `video/x-raw`）。如果是 demuxer 的 pad 还带编码格式，需要额外解码元素或改用 decodebin/uridecodebin。
- 缺少插件（Missing plugins）
  - 按错误提示安装相应的 GStreamer 插件包（`gstreamer1.0-plugins-{good,bad,ugly}`、`libav` 等）。

---

## 可选拓展

- 使用 `uridecodebin` 直接处理 `file://` 或 `http(s)://` 等 URI，避免 filesrc+typefind 的组合；其动态 pad 处理方式与 decodebin 相同。
- 同时支持音频/视频：在回调里根据 caps 将 `audio/x-raw` 链到音频分支，将 `video/x-raw` 链到视频分支。
- 输出结构化信息：在回调中记录检测到的流类型、语言、采样率/分辨率等，为上层 UI/日志服务。

---

## 关键 API 速查

- 回调与动态 pad
  - `g_signal_connect(element, "pad-added", pad_added_handler, user_data)`
  - `gst_pad_get_current_caps()` / `gst_pad_query_caps()`
  - `gst_caps_get_structure()` / `gst_structure_get_name()`
  - `gst_element_get_static_pad(element, "sink")`、`gst_pad_is_linked()`、`gst_pad_link()`
- 静态链接
  - `gst_element_link()`、`gst_element_link_many()`
- 运行与消息
  - `gst_element_set_state()`、`gst_bus_add_signal_watch()`、`message::error` / `::eos`

---

## 小结

“动态管线”的核心是：并非所有 pad 在构建时都存在。对 decodebin、demuxer 等元素，必须在运行中监听 `pad-added`，读取 caps 决定媒体类型，并与恰当的下游链路在回调中完成链接。这是后续构建复杂播放、录制与转码拓扑的关键基础。
