# GStreamer 回放教程 7：Custom playbin sinks（自定义 playbin 接收器）

本文是对 playbook-tutorial-7.c 的中文学习笔记，演示如何在 GStreamer 中自定义 `playbin` 的音频和视频接收器，通过创建复合管道（sink-bin）实现音频均衡器等高级功能。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/playback/custom-playbin-sinks.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 `audio-sink` 和 `video-sink` 属性替换 `playbin` 默认接收器：
  - 允许应用程序控制最终的渲染/显示过程。
  - 保留 `playbin` 的媒体检索和解码能力。
- 创建复合接收器（sink-bin）：
  - 将多个元素封装在 `GstBin` 中作为单一元素使用。
  - 通过 `GstGhostPad` 连接内部元素与外部管道。
- Bin 容器概念：理解 `GstBin`、`GstPipeline` 和 `GstElement` 的层次关系。
- **Ghost Pad 机制详解**：
  - **核心作用**：作为 Bin 内部元素 pad 的"代理人"，让外部可以访问 Bin 内部的连接点
  - **解决问题**：Bin 本身没有 pad，外部无法直接连接到内部元素
  - **工作原理**：`playbin` → `ghost_pad` → `内部元素的真实pad`，实现数据透明转发
  - **形象比喻**：如同大楼的前台接待，外部访客通过前台访问内部房间
- 实际应用示例：构建包含均衡器的音频处理链。
- Ghost Pad 机制：实现 Bin 内外部元素的数据传递。

---

## 代码结构概览（playback-tutorial-7.c）
- 创建 `playbin` 管道：设置媒体 URI。
- 构建音频处理链：`equalizer-3bands` → `audioconvert` → `autoaudiosink`。
- 创建 Bin 容器：使用 `gst_bin_new()` 封装音频处理元素。
- 建立 Ghost Pad：连接 Bin 内部的均衡器输入端到外部接口。
- 配置均衡器参数：设置频段增益实现低音增强效果。
- 设置自定义接收器：通过 `audio-sink` 属性将 Bin 传递给 `playbin`。

---

## 工作流程（自定义接收器）
1. `gst_init()` 初始化；创建 `playbin` 并设置媒体 URI。
2. 创建音频处理元素：
   - `equalizer-3bands`：三频段均衡器
   - `audioconvert`：音频格式转换（确保兼容性）
   - `autoaudiosink`：自动音频输出
3. 创建 Bin 容器：`gst_bin_new()` 创建名为 "audio_sink_bin" 的容器。
4. 添加和链接元素：
   - `gst_bin_add_many()` 将元素添加到 Bin
   - `gst_element_link_many()` 按顺序链接元素
5. 创建 Ghost Pad：
   - `gst_element_get_static_pad()` 获取均衡器的输入端
   - `gst_ghost_pad_new()` 创建指向内部端口的 Ghost Pad
   - `gst_element_add_pad()` 将 Ghost Pad 添加到 Bin
6. 配置均衡器：设置 `band1` 和 `band2` 为 -24.0dB（低音增强）。
7. 设置自定义接收器：`g_object_set(pipeline, "audio-sink", bin, NULL)`。
8. 开始播放并等待结束。

---

## 典型输出（示例片段）
```
[播放开始，音频通过自定义均衡器处理]
[低音增强效果：高频衰减 -24dB，低频保持原始强度]
[音频输出到系统默认音频设备]
```
- 程序运行时没有特殊控制台输出
- 音频效果体现在播放的声音中：明显的低音增强
- 可以通过修改均衡器参数实时感受不同的音频效果

---

## 常见问题与排错
- 元素创建失败：确保安装了 `gst-plugins-good` 包，包含 `equalizer-3bands` 元素。
- 链接失败（caps 不兼容）：
  - 确保包含 `audioconvert` 元素进行格式转换。
  - 检查元素之间的能力协商是否成功。
- Ghost Pad 创建错误：
  - 确认获取的内部 Pad 有效且未被释放。
  - 检查 Ghost Pad 是否正确激活和添加到 Bin。
- 音频无输出：
  - 验证 `autoaudiosink` 能否找到可用的音频设备。
  - 尝试使用具体的音频接收器如 `alsasink` 或 `pulsesink`。
- 均衡器无效果：检查均衡器参数范围，通常为 -24.0 到 +12.0 dB。

---

## 扩展与自定义

### 1) 视频效果 Bin
创建包含视频特效的自定义视频接收器：
```c
static GstElement* create_video_sink_bin() {
  GstElement *bin, *effect, *convert, *sink;
  GstPad *pad, *ghost_pad;
  
  /* 创建视频特效元素 */
  effect = gst_element_factory_make("vertigotv", "effect");
  convert = gst_element_factory_make("videoconvert", "convert");
  sink = gst_element_factory_make("autovideosink", "video_sink");
  
  /* 创建并配置 Bin */
  bin = gst_bin_new("video_sink_bin");
  gst_bin_add_many(GST_BIN(bin), effect, convert, sink, NULL);
  gst_element_link_many(effect, convert, sink, NULL);
  
  /* 创建 Ghost Pad */
  pad = gst_element_get_static_pad(effect, "sink");
  ghost_pad = gst_ghost_pad_new("sink", pad);
  gst_pad_set_active(ghost_pad, TRUE);
  gst_element_add_pad(bin, ghost_pad);
  gst_object_unref(pad);
  
  return bin;
}
```

### 2) 多频段均衡器配置
实现更精细的音频均衡控制：
```c
static void configure_advanced_equalizer(GstElement *equalizer) {
  /* 10频段均衡器配置 */
  g_object_set(equalizer,
               "band0", -12.0,  /* 60Hz */
               "band1", -8.0,   /* 170Hz */
               "band2", -4.0,   /* 310Hz */
               "band3", 0.0,    /* 600Hz */
               "band4", 2.0,    /* 1kHz */
               "band5", 4.0,    /* 3kHz */
               "band6", 2.0,    /* 6kHz */
               "band7", -2.0,   /* 12kHz */
               "band8", -6.0,   /* 14kHz */
               "band9", -10.0,  /* 16kHz */
               NULL);
}
```

### 3) 动态效果切换
运行时切换不同的音频效果：
```c
typedef struct {
  GstElement *pipeline;
  GstElement *current_sink;
  GstElement *equalizer_bin;
  GstElement *reverb_bin;
} EffectSwitcher;

static void switch_audio_effect(EffectSwitcher *switcher, gint effect_type) {
  GstElement *new_sink = (effect_type == 0) ? 
                         switcher->equalizer_bin : switcher->reverb_bin;
  
  gst_element_set_state(switcher->pipeline, GST_STATE_PAUSED);
  g_object_set(switcher->pipeline, "audio-sink", new_sink, NULL);
  gst_element_set_state(switcher->pipeline, GST_STATE_PLAYING);
  
  switcher->current_sink = new_sink;
}
```

### 4) 音频分析和可视化
结合音频分析元素创建复合接收器：
```c
static GstElement* create_analyzer_sink_bin() {
  GstElement *bin, *tee, *queue1, *queue2, *analyzer, *sink;
  
  /* 创建分流和分析元素 */
  tee = gst_element_factory_make("tee", "tee");
  queue1 = gst_element_factory_make("queue", "queue1");
  queue2 = gst_element_factory_make("queue", "queue2");
  analyzer = gst_element_factory_make("spectrum", "analyzer");
  sink = gst_element_factory_make("autoaudiosink", "sink");
  
  /* 构建分析管道：输入 → tee → (分析分支, 播放分支) */
  bin = gst_bin_new("analyzer_sink_bin");
  gst_bin_add_many(GST_BIN(bin), tee, queue1, queue2, analyzer, sink, NULL);
  
  /* 链接播放分支 */
  gst_element_link_many(tee, queue1, sink, NULL);
  /* 链接分析分支 */
  gst_element_link_many(tee, queue2, analyzer, NULL);
  
  /* 创建 Ghost Pad */
  GstPad *pad = gst_element_get_static_pad(tee, "sink");
  GstPad *ghost_pad = gst_ghost_pad_new("sink", pad);
  gst_pad_set_active(ghost_pad, TRUE);
  gst_element_add_pad(bin, ghost_pad);
  gst_object_unref(pad);
  
  return bin;
}
```

---

## 关键 API 速查
- Bin 容器操作：
  - `gst_bin_new()`（创建新的 Bin 容器）；
  - `gst_bin_add_many()`（批量添加元素到 Bin）；
  - `GST_BIN()`（类型转换宏）。
- Ghost Pad 管理：
  - `gst_element_get_static_pad()`（获取元素的静态端口）；
  - `gst_ghost_pad_new()`（创建 Ghost Pad）；
  - `gst_pad_set_active()`（激活端口）；
  - `gst_element_add_pad()`（添加端口到元素）。
- playbin 属性：
  - `audio-sink`（音频接收器属性）；
  - `video-sink`（视频接收器属性）。
- 常用音频元素：
  - `equalizer-3bands`、`equalizer-10bands`（均衡器）；
  - `audioconvert`（音频格式转换）；
  - `autoaudiosink`、`alsasink`、`pulsesink`（音频输出）。
- 常用视频元素：
  - `videoconvert`（视频格式转换）；
  - `vertigotv`、`solarize`、`effectv`（视频特效）；
  - `autovideosink`、`xvimagesink`（视频输出）。

---

## 小结
自定义 `playbin` 接收器功能为 GStreamer 应用提供了在保持 `playbin` 便利性的同时实现高级音视频处理的能力。通过 `GstBin` 和 `GstGhostPad` 机制，开发者可以将复杂的处理链封装为单一组件，实现音频均衡、视频特效、信号分析等功能。这种设计模式在专业音视频应用中广泛使用，如音乐播放器的音效处理、视频播放器的画面增强、直播软件的实时特效等场景。掌握这一技术可以显著提升应用程序的音视频处理能力和用户体验。
