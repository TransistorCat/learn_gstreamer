# Playback Tutorial 9: Digital audio pass-through (数字音频直通)

本教程介绍 GStreamer 中的数字音频直通机制。了解如何通过 S/PDIF 等数字接口直接传输编码音频数据，让外部音频系统进行解码，从而获得更高的音质。

**官方教程**: [Digital audio pass-through](https://gstreamer.freedesktop.org/documentation/tutorials/playback/digital-audio-pass-through.html?gi-language=c)

---

## 目标与要点（知识点）

- 理解数字音频直通的概念和优势
- 掌握 S/PDIF（IEC 60958）数字音频接口的工作原理
- 学习 GStreamer 音频接收器的数字音频支持机制
- 了解不同平台音频接收器的差异（PulseAudio、DirectSound、OSX Audio）
- 掌握数字音频格式的兼容性处理
- 理解 `playbin` 的自动直通检测和管道构建

---

## 代码结构概览

本教程主要是概念性讲解，重点关注系统配置和格式处理：

```c
// 自定义音频接收器配置（解决兼容性问题）
GstElement *create_digital_audio_sink(const gchar *supported_formats) {
    GstElement *bin = gst_bin_new("digital-audio-sink");
    GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);
    GstElement *audiosink = gst_element_factory_make("pulsesink", NULL);
    
    // 设置支持的数字音频格式
    GstCaps *caps = gst_caps_from_string(supported_formats);
    g_object_set(capsfilter, "caps", caps, NULL);
    
    gst_bin_add_many(GST_BIN(bin), capsfilter, audiosink, NULL);
    gst_element_link(capsfilter, audiosink);
    
    // 创建 ghost pad
    GstPad *sink_pad = gst_element_get_static_pad(capsfilter, "sink");
    gst_element_add_pad(bin, gst_ghost_pad_new("sink", sink_pad));
    
    gst_caps_unref(caps);
    gst_object_unref(sink_pad);
    return bin;
}
```

---

## 工作流程

### 直通 vs 非直通对比

**数字音频直通模式**:
```
源文件 → 解复用器 → 编码音频数据 → 音频接收器 → S/PDIF → 外部解码器
```
- 跳过软件解码，保持原始编码格式（AC-3、DTS 等）
- 由外部音频设备进行硬件解码
- 音质更高，CPU 负载低，支持多声道环绕声

**非直通模式（软件解码）**:
```
源文件 → 解复用器 → 音频解码器 → PCM 数据 → 音频接收器 → 模拟输出
```
- 在计算机上软件解码为 PCM 格式
- 兼容性好，支持音效处理和混音
- 适用于普通音频设备

### GStreamer 自动选择流程

1. **系统配置**: 在操作系统音频控制面板启用数字音频输出
2. **接收器检测**: GStreamer 音频接收器检测数字音频输出可用性
3. **能力协商**: 音频接收器更新输入能力，接受编码音频数据
4. **自动直通**: `playbin` 检测到可以直接连接编码数据到音频接收器
5. **管道构建**: 跳过音频解码器，构建直通管道
6. **数字传输**: 编码音频数据通过 S/PDIF 传输到外部解码器

---

## 典型输出（示例片段）

```bash
# 检查 PulseAudio 数字音频支持
$ pactl list sinks | grep -A 5 "digital"
Name: alsa_output.pci-0000_00_1b.0.iec958-stereo
Description: Built-in Audio Digital Stereo (IEC958)

# 查看音频接收器支持的格式
$ gst-inspect-1.0 pulsesink | grep -A 10 "Pad Templates"
SRC template: 'sink'
  Availability: Always
  Capabilities:
    audio/x-raw
    audio/mpeg
    audio/x-ac3
    audio/x-eac3
    audio/x-dts

# 测试数字音频直通
$ gst-launch-1.0 filesrc location=movie.mkv ! \
  matroskademux ! ac3parse ! pulsesink
```

---

## 常见问题与排错

### 数字音频输出不工作
```bash
# 检查系统数字音频配置
$ aplay -l  # 列出音频设备
$ cat /proc/asound/cards  # 查看声卡信息

# PulseAudio 配置检查
$ pactl list sinks short
$ pactl info
```

### 格式兼容性问题
- 外部解码器不支持所有 GStreamer 暴露的格式
- 需要用户干预选择正确的音频格式
- 避免使用 `autoaudiosink`（仅支持原始音频）

### 音质问题
- 确保 S/PDIF 连接正常（光纤或同轴）
- 检查外部解码器是否正确识别音频格式
- 验证系统音频设置中的采样率配置

---

## 扩展与自定义

### 格式限制的自定义接收器
```c
// 创建支持特定格式的音频接收器
GstElement *create_ac3_only_sink(void) {
    return create_digital_audio_sink(
        "audio/x-ac3; audio/x-eac3; audio/x-raw"
    );
}

// 在 playbin 中使用自定义接收器
void setup_digital_audio_playback(GstElement *playbin) {
    GstElement *custom_sink = create_ac3_only_sink();
    g_object_set(playbin, "audio-sink", custom_sink, NULL);
}
```

### 动态格式检测
```c
// 检测外部解码器支持的格式
gboolean probe_decoder_capabilities(GstElement *sink) {
    GstCaps *sink_caps = gst_element_get_static_pad(sink, "sink");
    GstCaps *current_caps = gst_pad_get_current_caps(sink_caps);
    
    if (current_caps) {
        gchar *caps_str = gst_caps_to_string(current_caps);
        g_print("Supported formats: %s\n", caps_str);
        g_free(caps_str);
        gst_caps_unref(current_caps);
        return TRUE;
    }
    return FALSE;
}
```

### 平台特定配置
```c
// 根据平台选择合适的音频接收器
GstElement *create_platform_audio_sink(void) {
#ifdef __linux__
    return gst_element_factory_make("pulsesink", NULL);
#elif defined(_WIN32)
    return gst_element_factory_make("directsoundsink", NULL);
#elif defined(__APPLE__)
    return gst_element_factory_make("osxaudiosink", NULL);
#else
    return gst_element_factory_make("autoaudiosink", NULL);
#endif
}
```

### 音频格式转换回退
```c
// 当直通失败时的回退机制
void setup_audio_with_fallback(GstElement *playbin) {
    GstElement *bin = gst_bin_new("audio-sink-bin");
    GstElement *queue = gst_element_factory_make("queue", NULL);
    GstElement *convert = gst_element_factory_make("audioconvert", NULL);
    GstElement *resample = gst_element_factory_make("audioresample", NULL);
    GstElement *sink = gst_element_factory_make("pulsesink", NULL);
    
    gst_bin_add_many(GST_BIN(bin), queue, convert, resample, sink, NULL);
    gst_element_link_many(queue, convert, resample, sink, NULL);
    
    // 创建 ghost pad
    GstPad *sink_pad = gst_element_get_static_pad(queue, "sink");
    gst_element_add_pad(bin, gst_ghost_pad_new("sink", sink_pad));
    gst_object_unref(sink_pad);
    
    g_object_set(playbin, "audio-sink", bin, NULL);
}
```

---

## 关键 API 速查

### 音频接收器相关
```c
// 创建音频接收器
GstElement *gst_element_factory_make(const gchar *factoryname, 
                                     const gchar *name);

// 设置设备属性（ALSA）
g_object_set(G_OBJECT(sink), "device", "hw:0,1", NULL);

// 获取接收器能力
GstCaps *gst_pad_get_current_caps(GstPad *pad);
```

### 能力过滤
```c
// 创建能力过滤器
GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);

// 设置过滤能力
GstCaps *caps = gst_caps_from_string("audio/x-ac3; audio/x-eac3");
g_object_set(capsfilter, "caps", caps, NULL);
```

### 数字音频格式
```c
// 常见数字音频 MIME 类型
"audio/mpeg"        // MPEG Audio (MP3, AAC)
"audio/x-ac3"       // Dolby Digital AC-3
"audio/x-eac3"      // Enhanced AC-3 (E-AC-3)
"audio/x-dts"       // DTS
"audio/x-raw"       // PCM (未压缩)
```

---

## 小结

数字音频直通是高端音频系统的重要特性，允许音频数据以数字形式从计算机传输到外部解码器，避免了不必要的数字-模拟-数字转换，从而获得更高的音质。

关键要点：
- **自动化**: GStreamer 自动检测数字音频输出并启用直通模式
- **系统依赖**: 需要在操作系统级别启用数字音频输出
- **格式兼容**: 需要确保外部解码器支持传输的音频格式
- **平台差异**: 不同平台使用不同的音频接收器和配置方法

正确配置数字音频直通可以显著提升音频播放质量，特别是在高保真音频系统中效果明显。但需要注意格式兼容性问题，必要时使用自定义接收器进行格式限制。
