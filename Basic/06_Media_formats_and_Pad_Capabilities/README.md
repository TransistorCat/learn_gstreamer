# GStreamer 基础教程 6：媒体格式与 Pad 能力（Media formats and Pad Capabilities）

本文是对“Media formats and Pad Capabilities”（C 版）的中文学习笔记，核心在于理解 GStreamer 的“能力协商”（caps negotiation）机制：元素的 Pad 支持哪些媒体类型与参数、如何用 capsfilter 约束媒体格式、如何在代码中查询/构造/过滤 caps，以及当协商失败时该如何排查。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/media-formats-and-pad-capabilities.html?gi-language=c

---

## 你将学到
- 什么是 GstCaps（能力）与媒体类型（media type）
- Pad Template 与 Pad 能力（caps）的关系；固定/未固定/范围/集合
- capsfilter 元素与 gst_element_link_filtered 的使用方法
- 在代码中创建与查询 caps：gst_caps_new_simple、gst_caps_from_string、gst_pad_query_caps、gst_pad_get_current_caps、gst_caps_fixate 等
- 能力协商失败的典型原因与排错思路

---

## 背景概念

- 媒体类型（media type）
  - 例如：video/x-raw、audio/x-raw、image/jpeg、audio/mpeg 等
- 能力（caps）
  - 在某个媒体类型的基础上，附带参数集合（GstStructure），如：
    - video/x-raw, format=I420, width=640, height=480, framerate=30/1
    - audio/x-raw, format=S16LE, rate=44100, channels=2, layout=interleaved
- Pad Template 与实际 Pad
  - 每个元素的 Pad Template 定义了它“可能支持”的 caps（可通过 gst-inspect 查看），实际运行时 Pad 会从模板中选出一个“具体的”caps。
- 能力协商（caps negotiation）
  - 上下游元素通过交互选择一个双方都支持的“交集”caps。协商成功后，数据才能顺利流通；否则将报 NOT_NEGOTIATED 错误。

---

## 固定与未固定的 caps

- 固定（fixed caps）：所有字段都已确定，如 width=640, height=480, format=I420。
- 未固定（unfixed caps）：包含列表或范围/任意值，表明“可以接受多种选择”，例如：
  - format={I420, NV12}
  - framerate=[1/1, 60/1]
  - width=[320, 1920]
- GStreamer 在协商过程中会不断“收敛”到一个固定 caps；必要时可调用 gst_caps_fixate() 将未固定 caps 固化为一个具体选择（遵循插件的偏好和默认策略）。

---

## 两种常见的格式约束方式

1) 使用 capsfilter 元素
- 在管线描述或代码里插入 capsfilter，并设置其 "caps" 属性
- 例子（gst-launch 风格）：
  - videotestsrc ! video/x-raw,format=I420,width=320,height=240,framerate=30/1 ! autovideosink
- 代码风格：
  - caps = gst_caps_from_string("video/x-raw,format=I420,width=320,height=240,framerate=30/1");
  - g_object_set(capsfilter, "caps", caps, NULL);

2) 使用 gst_element_link_filtered()
- 在链接两个元素时附带一个 caps 作为过滤条件，只让满足该 caps 的媒体通过：
  - caps = gst_caps_new_simple("audio/x-raw",
      "format", G_TYPE_STRING, "S16LE",
      "rate",   G_TYPE_INT,    44100,
      "channels", G_TYPE_INT,  2, NULL);
  - gst_element_link_filtered(src, sink, caps);

两者本质等价：一个是通过独立元素显式过滤，一个是在链接时临时指定过滤条件。

---

## 在代码中查询与打印 caps

典型场景与方法：

- 查询一个 Pad 当前运行中的 caps
  - caps = gst_pad_get_current_caps(pad);
  - 如果返回 NULL，说明当前未固定，可改用 gst_pad_query_caps(pad, NULL);
- 查询一个 Pad 在当前条件下“可能支持”的 caps
  - caps = gst_pad_query_caps(pad, NULL);
- 打印 caps
  - gchar* s = gst_caps_to_string(caps); g_print("%s\n", s); g_free(s);

可以通过这些方法在运行时“观察”协商情况，辅助排错。

---

## 示例片段（伪代码）

- 构造并使用 caps 过滤视频
```
GstElement *src = gst_element_factory_make("videotestsrc", NULL);
GstElement *conv = gst_element_factory_make("videoconvert", NULL);
GstElement *sink = gst_element_factory_make("autovideosink", NULL);
GstElement *pipeline = gst_pipeline_new(NULL);

gst_bin_add_many(GST_BIN(pipeline), src, conv, sink, NULL);

// 使用 link_filtered 指定原始视频格式与参数
GstCaps *caps = gst_caps_from_string("video/x-raw,format=I420,width=640,height=480,framerate=30/1");
gst_element_link_filtered(src, conv, caps);
gst_caps_unref(caps);

// conv 到 sink 可直接链接（都会是 raw 视频）
gst_element_link(conv, sink);

gst_element_set_state(pipeline, GST_STATE_PLAYING);
```

- 查询 Pad 支持的 caps
```
GstPad *pad = gst_element_get_static_pad(conv, "src");
GstCaps *possible = gst_pad_query_caps(pad, NULL);
gchar *s = gst_caps_to_string(possible);
g_print("videoconvert src possible caps: %s\n", s);
g_free(s);
gst_caps_unref(possible);
gst_object_unref(pad);
```

- 在 decodebin/uridecodebin 的 pad-added 回调里按类型过滤
```
static void pad_added(GstElement* src, GstPad* new_pad, CustomData* data) {
  GstCaps *caps = gst_pad_get_current_caps(new_pad);
  const GstStructure *s = gst_caps_get_structure(caps, 0);
  const gchar *name = gst_structure_get_name(s); // "audio/x-raw" or "video/x-raw" etc.

  if (g_str_has_prefix(name, "audio/x-raw")) {
    GstCaps *filter = gst_caps_from_string("audio/x-raw,format=S16LE,rate=44100,channels=2");
    GstPad *sinkpad = gst_element_get_static_pad(data->audio_queue, "sink");
    // 尝试 new_pad -> queue.sink 的链路并满足 filter
    // 方式一：用 ghost/capsfilter；方式二：对后续元素使用 link_filtered
    gst_pad_link(new_pad, sinkpad);
    gst_object_unref(sinkpad);
    gst_caps_unref(filter);
  }
  gst_caps_unref(caps);
}
```

注意：在动态 pad 场景中，实际常见做法是 new_pad 先接到 queue，再由 queue 下游链路（audioconvert/audioresample 等）通过 link_filtered 或 capsfilter 限定参数。

---

## raw 与编码格式的典型 caps 字段

- video/x-raw
  - format（I420、NV12、RGB、BGRx、YUY2...）
  - width、height
  - framerate（如 30/1、30000/1001）
  - colorimetry、chroma-site（可选）
- audio/x-raw
  - format（S16LE、F32LE、S24_32LE...）
  - rate（44100、48000...）
  - channels（1、2...）
  - layout（interleaved / non-interleaved）
- 编码格式（如 video/x-h264、audio/mpeg 等）
  - 通常包含 stream-format、alignment、codec-data 等字段，供下游解析/解码器使用

---

## 常见问题与排错

- 链接失败（gst_element_link 返回 FALSE 或 bus 报 NOT_NEGOTIATED）
  - 上下游不支持共同的 caps。插入转换元素（videoconvert、audioconvert、audioresample、videoscale）或使用 capsfilter 限制到双方都能接受的参数。
- 画面/声音无法输出或异常
  - 检查是否要求了设备/平台不支持的格式（如某些 sinks 不支持特定像素格式）
  - 通过 gst-inspect-1.0 <element> 查看 Pad Templates，确认对齐的 caps 列表
- 性能问题（分辨率/帧率过高）
  - 使用 capsfilter 将分辨率/帧率“下采样”，降低处理压力
- 协商不稳定或未固定
  - 在关键位置调用 gst_caps_fixate() 固化 caps；或明确在 capsfilter 中给出固定值（避免范围/集合）

---

## 调试建议

- gst-inspect-1.0 element
  - 查看元素的 Pad Templates 和支持的 caps
- GST_DEBUG=2~6
  - 提高日志级别，关注 negotiation、capsfilter、converter 等类别
- 输出运行中 caps
  - 在关键 Pad 上调用 gst_pad_get_current_caps 并打印 gst_caps_to_string
- 导出 DOT 图
  - 设置 GST_DEBUG_DUMP_DOT_DIR，查看协商后各链路的 caps

---

## 关键 API 速查

- 创建/解析 caps
  - gst_caps_new_simple
  - gst_caps_from_string
  - gst_caps_to_string
  - gst_caps_fixate
- 过滤与链接
  - capsfilter 元素 + g_object_set(capsfilter, "caps", caps, NULL)
  - gst_element_link_filtered
- 查询与判断
  - gst_pad_get_current_caps
  - gst_pad_query_caps
  - gst_caps_is_fixed / gst_caps_is_any / gst_caps_is_empty
  - gst_caps_intersect / gst_caps_can_intersect
- 常用转换元素
  - videoconvert、videoscale、audioconvert、audioresample

---

## 小结

GStreamer 的“能力（caps）”是元素之间理解与交换媒体数据的契约。掌握 caps 的构造、过滤与查询，学会在关键位置使用转换元素与 capsfilter，你就能：
- 精确地约束媒体参数（分辨率、像素格式、采样率、声道等）
- 读懂并排查协商失败（NOT_NEGOTIATED）问题
- 为不同平台/设备选择兼容的格式，构建更稳定、高效的多媒体管线

这为后续更复杂的动态管线、分支并行（queue）、短路（appsink/appsrc）与 trick mode（seek/step）等进阶主题打下坚实基础。