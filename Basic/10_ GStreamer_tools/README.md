# GStreamer 基础教程 10：GStreamer 工具速览

本文是对“GStreamer tools”教程的中文学习笔记，涵盖最常用的命令行工具与实践示例，帮助你在不写代码的情况下快速验证想法、调试管线、探测媒体能力。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/gstreamer-tools.html?gi-language=c

---

## 总览：有哪些常用工具？
- gst-launch-1.0：用命令行快速搭建和运行 Pipeline。
- gst-inspect-1.0：查看插件/元素的说明、属性、Pad 模板和 Caps。
- gst-typefind-1.0：对文件做类型探测（MIME/Caps）。
- gst-discoverer-1.0：在不播放的前提下收集媒体信息（容器、时长、流、标签等）。
- gst-device-monitor-1.0：枚举系统可用的音视频设备。
- gst-play-1.0：简单播放器（基于 playbin）。

这些工具共享 GStreamer 的通用命令行/环境变量（如 --gst-debug、GST_DEBUG、GST_PLUGIN_PATH 等），便于调试与排错。

---

## 1) gst-launch-1.0：搭建与运行 Pipeline

语法要点：
- 基础：element1 [prop=value] ! element2 ! element3 ...
- 设置属性：element prop1=val1 prop2=val2
- 指定 Caps（能力/格式过滤）：... ! video/x-raw,format=I420,width=640,height=480 ! ...
- 命名并复用 Pad（tee 等）：tee name=t ! queue ! sink1 t. ! queue ! sink2

常用参数：
- -v：Verbose，打印元素的协商信息、属性等。
- -m：打印 Bus 上的消息（错误、状态、标签等）。
- -t：打印时间戳。
- -q：安静模式（少量输出）。
- -e：在退出时强制发送 EOS。

入门示例：
- 视频测试：
  - gst-launch-1.0 -v videotestsrc pattern=smpte ! videoconvert ! autovideosink
- 音频测试：
  - gst-launch-1.0 -v audiotestsrc freq=440 ! audioconvert ! audioresample ! autoaudiosink
- 指定 Caps：
  - gst-launch-1.0 videotestsrc ! video/x-raw,format=I420,width=320,height=240 ! autovideosink
  - gst-launch-1.0 audiotestsrc ! audio/x-raw,rate=8000,channels=1 ! autoaudiosink
- 播放本地或网络媒体（playbin 最简单）：
  - gst-launch-1.0 playbin uri=file:///home/user/clip.mp4
  - gst-launch-1.0 playbin uri=https://example.com/video.mp4
- 使用解码器组合（演示，复杂媒体推荐 playbin）：
  - gst-launch-1.0 filesrc location=clip.mp4 ! decodebin ! videoconvert ! autovideosink
- 一源多路（tee）：
  - GST_DEBUG_DUMP_DOT_DIR=. gst-launch-1.0 audiotestsrc ! tee name=t ! queue ! autoaudiosink t. ! queue ! fakesink silent=false

小贴士：
- Windows 本地路径请转换为 file URI（例如 file:///C:/path/to/file.mp4）。
- caps 过滤是协商的重要手段；不过若目标元素不支持该 caps，链接会失败。
- decodebin 可能产生多路 pad（音视频各一），在复杂拓扑里常需 queue/自动选择 sink；对“通用播放”场景建议直接用 playbin。

---

## 2) gst-inspect-1.0：查询元素与插件信息

用途：
- 列出所有元素/插件、查看某个元素的属性、Pad 模板、支持的 caps、信号等。

示例：
- 查看某元素：
  - gst-inspect-1.0 videoconvert
- 查看某解码器的支持格式：
  - gst-inspect-1.0 avdec_h264
- 快速查找名字中包含“x264”的元素：
  - gst-inspect-1.0 | grep x264

输出解读要点：
- Pad Templates：显示静态/请求 Pad 及其 Caps（协商范围）。
- Properties：可设置的属性、类型和默认值。
- Rank：元素“优先级”（影响自动选择，如 playbin 的自动挑选）。

---

## 3) gst-typefind-1.0：文件类型探测

示例：
- gst-typefind-1.0 movie.unknown

输出通常包含 MIME 类型与对应的 Caps，可辅助选择合适的解析器/解复用器。

---

## 4) gst-discoverer-1.0：媒体信息收集

示例：
- gst-discoverer-1.0 file:///home/user/clip.mp4
- gst-discoverer-1.0 https://example.com/stream.webm

可获得：
- 总体：时长、是否可寻址（seekable）、整体标签（metadata）。
- 结构：容器类型、音/视频/字幕等子流。
- 每路流的编解码器描述、分辨率/帧率/声道/采样率、语言等。

如缺少插件会提示 Missing plugins，根据提示安装（good/bad/ugly/libav 等）。

---

## 5) gst-device-monitor-1.0：设备枚举

示例：
- 列出视频设备：gst-device-monitor-1.0 Video
- 列出音频设备：gst-device-monitor-1.0 Audio

结合平台特定源：
- Linux 摄像头：v4l2src device=/dev/video0
- macOS 摄像头：avfvideosrc
- Windows 摄像头：ksvideosrc

示例管线：
- gst-launch-1.0 v4l2src device=/dev/video0 ! videoconvert ! autovideosink

---

## 6) gst-play-1.0：简单播放器

示例：
- gst-play-1.0 file:///home/user/clip.mp4
- gst-play-1.0 https://example.com/stream.mkv

支持基本的播放、暂停、seek，适合快速验证媒体可播放性。可通过参数指定特定的音视频 sink（--videosink/--audiosink）。

---

## 7) 调试与排错：通用技巧

- 日志级别（环境变量）：
  - GST_DEBUG=*:3 gst-launch-1.0 ...        # 全局 info 级别
  - GST_DEBUG=playbin:5,GST_PADS:5 ...       # 指定分类更详细
- 打印 Pipeline DOT 图：
  - 运行前设置：export GST_DEBUG_DUMP_DOT_DIR=/tmp
  - 执行后在该目录生成 dot 文件（如 0.00.00.*.dot），用 Graphviz 渲染：dot -Tpng pipeline.dot -o pipeline.png
- 插件路径问题：
  - 自定义路径：export GST_PLUGIN_PATH=/your/plugin/dir
- gst-launch-1.0 常用开关：-v（协商详情）、-m（Bus 消息）、-t（时间戳）。

常见错误指引：
- 链接失败（could not link）：检查上下游 caps 是否可协商，或补 videoconvert/audioconvert/audioresample。
- Missing plugin：根据 gst-inspect-1.0/gst-discoverer-1.0 提示安装相应插件包。
- 时钟与同步问题（画面卡顿/音画不同步）：尝试在接收端设置 sync=false（如 autovideosink sync=false）以排除时钟影响做快速验证。

---

## 8) 小抄：常见管线片段

- 将测试视频缩放与转换后显示：
  - gst-launch-1.0 videotestsrc ! video/x-raw,format=I420,width=640,height=360 ! videoconvert ! autovideosink
- 播放摄像头：
  - gst-launch-1.0 v4l2src device=/dev/video0 ! videoconvert ! autovideosink
- 播放文件（自动一切）：
  - gst-launch-1.0 playbin uri=file:///path/to/file
- 简单音频 tone：
  - gst-launch-1.0 audiotestsrc wave=sine freq=1000 ! audioconvert ! audioresample ! autoaudiosink

---

## 小结
GStreamer 提供了一套强大的命令行工具：
- 用 gst-launch-1.0 快速搭建与验证管线；
- 用 gst-inspect-1.0 查元素能力与属性；
- 用 gst-typefind-1.0 与 gst-discoverer-1.0 了解媒体文件；
- 用 gst-device-monitor-1.0 确认设备；
- 用 gst-play-1.0 快速播放验证；
- 配合 GST_DEBUG 与 DOT 图进行深入调试。

掌握这些工具，可以极大提升你在开发与排障时的效率。
