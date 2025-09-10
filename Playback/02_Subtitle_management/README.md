# GStreamer 播放教程 2：字幕管理（Subtitle Management）

本文是对“Subtitle management”教程的中文学习笔记，结合仓库中的 playback-tutorial-2.c 源码，说明：
- 如何用 playbin 一站式播放并管理音视频与字幕；
- 如何启用字幕、列出可用的字幕流、在运行时切换字幕；
- 如何加载外挂字幕（suburi）与设置字幕字体（subtitle-font-desc）。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/playback/subtitle-management.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 playbin（高层播放元素）简化播放：只需设置 uri 即可自动组装解复用/解码/渲染管线。
- 通过 playbin 的 flags 属性开启音频/视频/字幕输出：
  - GST_PLAY_FLAG_VIDEO（1<<0）
  - GST_PLAY_FLAG_AUDIO（1<<1）
  - GST_PLAY_FLAG_TEXT（1<<2）
- 枚举并查看流信息：通过 n-video / n-audio / n-text 获取内嵌流数量；通过 get-*-tags 读取编解码器、语言等标签。
- 运行时切换字幕：设置属性 current-text 为目标字幕流索引。
- 外挂字幕：通过 suburi 指定外部 .srt 等字幕文件；通过 subtitle-font-desc 指定渲染字体；必要时通过 subtitle-encoding 指定文件编码。
- 基本 Bus 消息处理与主循环：在进入 PLAYING 状态后分析流信息，在 EOS/ERROR 时退出。

---

## 代码结构概览（playback-tutorial-2.c）
- CustomData：保存 playbin、当前流数量（n_video/n_audio/n_text）、当前选择（current_video/current_audio/current_text）、GMainLoop。
- analyze_streams()：
  - 读取 n-video/n-audio/n-text；
  - 通过 get-video-tags/get-audio-tags/get-text-tags 打印标签（编解码器、语言、码率等）；
  - 读取 current-* 并提示如何切换字幕。
- handle_message()（Bus 回调）：
  - ERROR/EOS：打印并退出主循环；
  - STATE_CHANGED→PLAYING：调用 analyze_streams()。
- handle_keyboard()：
  - 从 stdin 读取数字并设置 current-text，实现运行时切换字幕。

---

## 工作流程
1) 初始化与创建元素
- gst_init；
- playbin = gst_element_factory_make("playbin", "playbin");

2) 设置媒体与字幕
- g_object_set(playbin, "uri", "https://…/sintel_trailer-480p.ogv");
- 可选外挂字幕：g_object_set(playbin, "suburi", "https://…/sintel_trailer_gr.srt");
- 可选字体：g_object_set(playbin, "subtitle-font-desc", "Sans, 18");

3) 启用视频/音频/字幕三类输出
- g_object_get(playbin, "flags", &flags);
- flags |= (GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_TEXT);
- g_object_set(playbin, "flags", flags);

4) 设置 Bus 监听与键盘输入
- gst_bus_add_watch(bus, handle_message, &data);
- g_io_add_watch(stdin, handle_keyboard, &data);

5) 切换到 PLAYING 并进入主循环
- gst_element_set_state(playbin, GST_STATE_PLAYING);
- g_main_loop_run(loop);

6) 运行时交互
- 在终端输入字幕流索引 n，设置 g_object_set(playbin, "current-text", n)。

---

## 典型输出（示例片段）
```
3 video stream(s), 1 audio stream(s), 2 text stream(s)

video stream 0:
  codec: Theora
video stream 1:
  codec: VP8
video stream 2:
  codec: H.264

audio stream 0:
  codec: Vorbis
  language: eng
  bitrate: 128000

subtitle stream 0:
  language: eng
subtitle stream 1:
  language: gr

Currently playing video stream 0, audio stream 0 and subtitle stream 1
Type any number and hit ENTER to select a different subtitle stream
```
实际输出依据媒体不同而变化；若某些流没有标签，会打印 "no tags found"。

---

## 典型使用（概念示意）
- 仅播放：
  - g_object_set(playbin, "uri", <媒体URL或file://路径>);
- 加载外挂字幕并渲染：
  - g_object_set(playbin, "suburi", <字幕URL或file://路径>);
  - g_object_set(playbin, "subtitle-font-desc", "Sans, 18");
- 启用三类输出：
  - 读取 flags，按位或上 VIDEO/AUDIO/TEXT，再写回 "flags"。

playbin 会在内部自动解码、将字幕合成到视频上（通常由内部 textoverlay 链路完成），并将音视频分别输出到自动的 sink。

---

## 关键属性与信号（playbin）
- 基本属性
  - uri：媒体地址（支持 file://、http(s):// 等）
  - suburi：外挂字幕地址（同样支持本地/网络路径）
  - flags：控制是否输出视频/音频/字幕（按位掩码）
  - n-video / n-audio / n-text：内嵌视频/音频/字幕流数量
  - current-video / current-audio / current-text：当前选择的流索引
  - subtitle-font-desc：字幕字体描述（Pango 语法，如 "Sans, 18" 或 "Sans Bold 18"）
  - 可选：subtitle-encoding（当外挂字幕不是 UTF-8 时可显式设置编码，如 "utf-8"、"gb18030"、"cp936" 等）

- 标签获取信号（通过 g_signal_emit_by_name 调用）
  - "get-video-tags"(index, &GstTagList*)
  - "get-audio-tags"(index, &GstTagList*)
  - "get-text-tags"(index, &GstTagList*)
  - 常见 Tag：
    - GST_TAG_VIDEO_CODEC / GST_TAG_AUDIO_CODEC
    - GST_TAG_LANGUAGE_CODE（语言）
    - GST_TAG_BITRATE 等

---

## 扩展与自定义
1) 动态开关字幕
- 运行时关闭字幕：
```c
int flags; g_object_get(playbin, "flags", &flags, NULL);
flags &= ~GST_PLAY_FLAG_TEXT; // 清掉 TEXT 位
g_object_set(playbin, "flags", flags, NULL);
```
- 重新开启时再置位 TEXT 位。

2) 指定外挂字幕编码
- 某些 SRT/ASS 文件不是 UTF-8，可设置：
```c
g_object_set(playbin, "subtitle-encoding", "gb18030", NULL);
```

3) 依据语言自动选择字幕
- 在 analyze_streams() 里遍历 0..n_text-1，读取 GST_TAG_LANGUAGE_CODE；若匹配用户偏好（如 "zh"、"en"），调用：
```c
g_object_set(playbin, "current-text", index, NULL);
```

4) 本地路径与跨平台
- Linux/macOS：file:///home/user/video.mp4；
- Windows：file:///C:/path/to/video.mp4（注意使用 / 分隔且包含盘符前的 /）。

5) 字体效果与样式
- Pango 语法示例："Sans 18"、"Sans Bold 20"、"Noto Sans CJK SC 18"；
- 字体需已安装到系统，缺失会回退到默认字体。

6) 无内嵌字幕的情况
- n-text 可能为 0。此时依赖 suburi 提供外挂字幕；
- playbin 仅支持一个 suburi，如需多字幕文件/切换自定义渲染，考虑自建管线（例如 textoverlay + subtitleparse + filesrc）或更高阶的应用逻辑。

---

## 流枚举与切换（要点）
- g_object_get(playbin, "n-text", &n_text) 获取字幕流数量；
- 通过 get-text-tags(index, &tags) 获取该字幕流的标签（如语言码）；
- 切换当前字幕：g_object_set(playbin, "current-text", index)；
- 多数流信息在进入 PLAYING 状态后才稳定，示例即在 STATE_CHANGED→PLAYING 时调用 analyze_streams()。

---

## 常见问题与排错
- 看不到字幕：
  - 确认 flags 包含 GST_PLAY_FLAG_TEXT；
  - 确认 n-text > 0（内嵌）或 suburi 指向可访问的字幕文件（外挂）；
  - 如出现乱码，设置 "subtitle-encoding"；
  - 检查总线 ERROR 消息，可能缺少解析/渲染相关插件。
- 切换字幕无效：
  - 确认索引在 [0, n_text) 范围内；
  - 等待进入 PLAYING 再切换；
  - 某些媒体没有内嵌字幕，需要使用 suburi。
- 字体与样式不生效：
  - 确认使用了 "subtitle-font-desc"；
  - 不同平台需要相应字体已安装。
- 网络资源不可达：
  - 测试 URL 的可访问性，或改用本地 file:// 路径。

---

## 关键 API 速查
- g_object_set / g_object_get（属性：uri, suburi, flags, n-*, current-*, subtitle-font-desc, subtitle-encoding）
- g_signal_emit_by_name(playbin, "get-video-tags"/"get-audio-tags"/"get-text-tags", index, &tags)
- GstTagList：gst_tag_list_get_string / gst_tag_list_get_uint / gst_tag_list_free
- 总线与主循环：gst_bus_add_watch, handle_message(ERROR/EOS/STATE_CHANGED), g_main_loop_run

---

## 小结
- 使用 playbin 可以极大简化“带字幕”的播放场景；
- 通过 flags 启用字幕，通过 n-text/current-text 管理并切换字幕流；
- 借助 suburi 可轻松加载外挂字幕，并用 subtitle-font-desc/subtitle-encoding 控制显示与编码；
- 结合 Bus 消息与标签系统，可做更完善的 UI（如列出语言并让用户选择字幕）。
