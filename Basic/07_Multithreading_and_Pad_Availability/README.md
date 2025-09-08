# GStreamer 基础教程 7：多线程与 Pad 可用性（Multithreading and Pad Availability）

本文是对“Multithreading and Pad Availability”教程的中文学习笔记，讲解：
- 如何处理“动态出现的”Pads（Sometimes pads），用 pad-added 回调在运行期完成链接；
- 为什么在多路（音频/视频）分支上使用 queue 以获得真正的并行处理（多线程）；
- Always / Sometimes / Request 三类 Pad 的区别与使用注意事项。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/multithreading-and-pad-availability.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 decodebin/uridecodebin 解复用/解码一个媒体 URI，并在 pad 动态出现时完成与下游分支的链接。
- 在音频分支和视频分支各自放置 queue，启用独立的下游处理线程，避免分支之间互相阻塞。
- 了解 Pad 的三种可用性：
  - Always：元素创建时就存在
  - Sometimes：在运行时（探测出流后）才出现（典型如 decodebin/demuxer 的 src pad）
  - Request：按需申请（典型如 tee 的 src_%u）
- 正确的动态链接模式：在 pad-added 回调中，通过查看新 Pad 的 caps 决定其属于音频还是视频，再与对应分支的 sink pad 手动链接。
- 基本的 Bus 消息处理（ERROR/EOS）。

---

## 典型管线拓扑（概念示意）

使用 uridecodebin（或 decodebin）自动识别并解码媒体，输出音/视频两路：

- uridecodebin uri=... name=dec
  - dec. ! queue ! audioconvert ! audioresample ! autoaudiosink
  - dec. ! queue ! videoconvert ! autovideosink

关键点：
- uridecodebin/decodebin 的 src pad 是 Sometimes（动态）出现，不能在构建时用 gst_element_link_many 直接静态链接，必须等到 pad-added 信号触发再进行 gst_pad_link。
- 每条分支紧跟一个 queue，让音频和视频可以在不同线程中并行处理，避免一个 sink 的阻塞影响另一条分支。

---

## 代码结构与核心回调（概览）

- CustomData 结构体
  - pipeline：顶层管道
  - source：uridecodebin 或 decodebin
  - audio_queue, audioconvert, audioresample, audio_sink
  - video_queue, videoconvert, video_sink
  - 一些布尔标记（如已链接音频/视频）与 GMainLoop 指针

- pad-added 回调
  - 原型：static void pad_added_handler(GstElement* src, GstPad* new_pad, CustomData* data)
  - 步骤：
    1) 取得 new_pad 的当前 caps（gst_pad_get_current_caps 或 gst_pad_query_caps）
    2) gst_caps_get_structure -> gst_structure_get_name，判断是 "audio/x-raw" 还是 "video/x-raw"（或其它）
    3) 根据类型选择对应分支的 queue 的 sink pad（gst_element_get_static_pad(queue, "sink")）
    4) 若该分支尚未链接（!GST_PAD_IS_LINKED(sinkpad)），执行 gst_pad_link(new_pad, sinkpad)
    5) 释放临时引用（caps、sinkpad）

- Bus 回调（或信号）
  - 处理 ERROR（打印详情并退出主循环）、EOS（流结束，退出）、其他消息可按需处理

- main 流程
  - gst_init
  - 创建 pipeline、uridecodebin（或 decodebin）、音频链和视频链元素
  - 将元素加入管线（gst_bin_add_many）
  - 给 source 连接 "pad-added" 信号回调
  - 其余能静态链接的元素（如 queue->convert->sink）用 gst_element_link_many 预先链接好
  - 设置 pipeline 为 PLAYING，进入 g_main_loop_run
  - 收到 EOS/ERROR 后清理资源

---

## 为什么需要 queue（多线程意义）

- 没有 queue：音频和视频分支会共享相同的上游“推流线程”，一条分支（例如视频渲染阻塞）会拖累另一条分支（音频卡顿），导致整体吞吐下降甚至死锁。
- 有了 queue：每个 queue 会创建一个新线程处理其下游链路，分支之间相对解耦。音频与视频可各自独立缓冲与同步。
- 队列大小控制：
  - 可通过 max-size-buffers / max-size-bytes / max-size-time 控制队列长度
  - leaky 属性可在队列满时丢尾或丢头，防止无限积压

---

## Pad 可用性三种类型

- Always（总是存在）
  - 元素创建时就有，如 audioconvert 的 sink/src pad
  - 适合用 gst_element_link 或 gst_element_link_many 静态链接

- Sometimes（有时存在）
  - 运行时才出现，如 decodebin/uridecodebin/demuxer 的 src pad，只有识别到某条音频/视频流后才创建
  - 必须监听 "pad-added"（有时还可用 "pad-removed"）信号，在回调里完成 gst_pad_link

- Request（按需申请）
  - 需要时通过 gst_element_request_pad_simple() 申请，典型如 tee 的 src_%u
  - 使用完记得 gst_element_release_request_pad() 归还

本教程聚焦 Sometimes pad 的处理（pad-added 动态链接），并强调在多路分支上用 queue 获得并发处理能力。

---

## 工作流程

1. 创建 pipeline 和 uridecodebin（或 decodebin），设置 uri（若用 decodebin，可用 filesrc + decodebin + typefind 等组合）
2. 创建音频链：queue -> audioconvert -> audioresample -> autoaudiosink，并静态链接
3. 创建视频链：queue -> videoconvert -> autovideosink，并静态链接
4. 将所有元素加入 pipeline
5. g_signal_connect(source, "pad-added", pad_added_handler, &data)
6. set_state(PLAYING)，进入主循环
7. 解码器发现音频/视频流时发出 pad-added，回调中根据 caps 将 new_pad 链到对应分支的 queue sink pad
8. 播放直至 EOS 或 ERROR，退出主循环并清理资源

---

## 典型 pad-added 回调逻辑（伪代码）

```
pad_added_handler(src, new_pad, data):
  caps = gst_pad_get_current_caps(new_pad)
  s = gst_caps_get_structure(caps, 0)
  name = gst_structure_get_name(s)   // "audio/x-raw" 或 "video/x-raw" 等

  if starts_with(name, "audio/x-raw") and !audio_already_linked:
      sinkpad = gst_element_get_static_pad(data->audio_queue, "sink")
      if gst_pad_is_linked(sinkpad) == FALSE:
         if gst_pad_link(new_pad, sinkpad) == GST_PAD_LINK_OK:
            audio_already_linked = TRUE
      gst_object_unref(sinkpad)

  else if starts_with(name, "video/x-raw") and !video_already_linked:
      sinkpad = gst_element_get_static_pad(data->video_queue, "sink")
      if gst_pad_is_linked(sinkpad) == FALSE:
         if gst_pad_link(new_pad, sinkpad) == GST_PAD_LINK_OK:
            video_already_linked = TRUE
      gst_object_unref(sinkpad)

  gst_caps_unref(caps)
```

注意：
- 某些媒体可能只有音频或只有视频，回调会根据实际出现的 pad 决定链接。
- 使用 uridecodebin 通常能输出 raw 媒体类型（audio/x-raw, video/x-raw）；若用 demuxer + decodebin，pad-added 的类型可能是编码格式，需要额外解码元素。

---

## 常见问题与排错

- 直接 gst_element_link(decodebin, queue) 失败
  - 因为 decodebin 的 src 是 Sometimes pad，不存在静态 pad，必须用 pad-added 回调里 gst_pad_link
- 声音正常、画面卡顿（或反之）
  - 检查是否在每个分支紧跟一个 queue；没有 queue 时分支会互相阻塞
  - 检查 sink 的同步（sync）与队列大小参数
- 链接失败（GST_PAD_LINK_REFUSED）
  - 检查 caps 是否匹配（是否 raw）；确认是否使用了 uridecodebin 或在 decodebin 后加入了适当的解码器
- 未出现 pad-added
  - 检查媒体是否有效、是否包含对应流；有的容器只含音频或视频
  - 确认总线无错误（打印 ERROR 消息以定位插件或解码器缺失）
- 运行时报缺少插件
  - 安装相应的 GStreamer 插件包（good/bad/ugly）以及对应的编解码器支持

---

## 关键 API 速查

- 动态 pad 处理
  - g_signal_connect(element, "pad-added", pad_added_handler, user_data)
  - gst_pad_get_current_caps(new_pad) / gst_pad_query_caps
  - gst_caps_get_structure / gst_structure_get_name
  - gst_element_get_static_pad(queue, "sink")
  - gst_pad_is_linked / gst_pad_link

- 多线程队列
  - queue 元素属性：max-size-buffers / max-size-bytes / max-size-time / leaky

- 其它常用
  - gst_element_link_many（用于 Always pad 的静态链接）
  - gst_element_set_state / gst_bus_add_signal_watch / message::error, ::eos

---

## 小结

- 处理“动态出现”的 Sometimes pad 是 GStreamer 动态管线的核心技能。对 decodebin/uridecodebin 等元素，必须在 pad-added 回调中完成与下游的手动链接。
- 在多路分支（音频/视频）上加入 queue，能够把两个分支放入不同的线程中独立运行，避免互相阻塞，是构建稳定、流畅播放管线的关键。
- 牢记三类 pad 的差异与使用方式，结合 bus 消息处理与合理的队列参数设置，才能搭建可靠的多媒体处理流程。