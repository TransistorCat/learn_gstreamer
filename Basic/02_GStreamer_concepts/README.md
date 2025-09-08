# GStreamer 基础教程 2：核心概念（GStreamer concepts）

本文是对 Basic Tutorial 2 的中文学习笔记，聚焦 GStreamer 的核心概念：元素（Element）、Pad、能力（Caps）与协商、容器（Bin/Pipeline）、状态机、消息与事件等。阅读本篇将帮助你理解 sample 代码为何如此组织，以及后续教程（动态管线、多线程、格式/能力等）的知识基础。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/concepts.html?gi-language=c

---

## 目标与要点
- 理解数据如何在 GStreamer 中“从上游到下游”流动（Buffers），由 Elements 通过 Pads 链接而成的管线（Pipeline）。
- 熟悉三大“通信机制”：
  - 消息（Messages，在总线上 Bus 传递，供应用层消费，如错误、EOS、状态变化）；
  - 事件（Events，沿数据流向上传/下行，用于控制流，如 SEEK、EOS、FLUSH、SEGMENT 等）；
  - 查询（Queries，沿数据流查询能力/时长/位置等）。
- 了解 Pad 的类型与可用性（Always/Sometimes/Request），以及 Caps 协商的重要性（是否能成功链接）。
- 掌握 Pipeline/Element 的生命周期与状态机：NULL → READY → PAUSED → PLAYING。

---

## 概念速览

- Element（元素）：功能模块，如源（source）、过滤器（filter/convert）、接收器（sink）。
- Pad：元素的“接口”，分为 src（输出）与 sink（输入）。Elements 通过各自的 Pads 链接形成数据流。
- Caps（Capabilities，能力）：描述媒体类型与参数的“类型系统”，例如 `audio/x-raw, format=F32LE, rate=48000, channels=2`。是否能链接，取决于上下游 pad 的 caps 是否兼容（通过协商）。
- Bin / Pipeline：
  - Bin 是元素的容器（也是 Element 的子类），可以把复杂拓扑封装为“一个元素”。
  - Pipeline 是顶层 Bin，提供时钟、状态机和总线（Bus）。
- Factories / Plugins：元素由插件提供，通过工厂函数 `gst_element_factory_make()` 创建实例。
- Buffer：承载媒体数据的最小单位（帧/包），沿着 src→sink 方向被推动（push 模式居多）。
- Event / Query / Message：
  - Event（事件）在数据流中传播（上行或下行），改变或通告流状态（如 SEEK、新分段）。
  - Query（查询）用于从下游获取信息（如时长、可 seek 能力）。
  - Message（消息）送达 Pipeline 的 Bus，由应用线程读取（如 ERROR/EOS）。
- Clock & Synchronization：Pipeline 统一时钟，sinks 按时间戳渲染（音视频同步的基础）。

---

## 示例代码（basic-tutorial-2.c）对应的拓扑

示例构建了一条简单的播放链：
- videotestsrc（生成测试视频）→ vertigotv（视频特效）→ autovideosink（自动选择视频输出）

核心代码片段说明：
- 创建元素：
  - `gst_element_factory_make("videotestsrc", "source")`
  - `gst_element_factory_make("vertigotv", "filter")`
  - `gst_element_factory_make("autovideosink", "sink")`
- 创建 Pipeline：`gst_pipeline_new("test-pipeline")`
- 将元素加入 Pipeline：`gst_bin_add_many(pipeline, source, filter, sink, NULL)`
- 链接：
  - `gst_element_link(source, filter)`
  - `gst_element_link(filter, sink)`
  - 若中间需要格式转换，通常插入 `videoconvert` 或 `audioconvert` 等元素。
- 设置属性：`g_object_set(source, "pattern", 1, NULL)`
- 设置状态并等待：
  - `gst_element_set_state(pipeline, GST_STATE_PLAYING)`
  - 通过 `gst_bus_timed_pop_filtered()` 阻塞等待 `ERROR` 或 `EOS` 消息。

---

## 元素与 Pad：链接与可用性

- Pad 方向：src（上游输出）→ sink（下游输入）。链接发生在 `src_pad` 与 `sink_pad` 之间。
- 可用性（Availability）：
  - Always：元素创建时就存在（可在构建管线时直接链接）。
  - Sometimes：运行时才出现（如 demuxer/decodebin 的动态 src pad）。需监听 `"pad-added"` 信号，回调中手动链接（见教程 3）。
  - Request：按需申请（如 `tee` 的 `src_%u`），使用后需释放。
- 静态链接 API：`gst_element_link()` / `gst_element_link_many()`，适用于 Always pads。
- 细粒度链接：也可以直接用 `gst_pad_link(srcpad, sinkpad)`。

---

## 能力（Caps）与协商

- Caps 描述数据格式：媒体主类型 + 键值对参数，例如：
  - `video/x-raw, format=I420, width=1280, height=720, framerate=30/1`
  - `audio/x-raw, format=S16LE, rate=48000, channels=2`
- 链接是否成功，取决于上下游 pad 的 caps 是否有交集（协商成功）。
- 当上下游不兼容时，插入合适的“转换/解析”元素（如 `audioconvert`, `audioresample`, `videoconvert`, `capsfilter`）。
- 过滤式链接：`gst_element_link_filtered(up, down, caps)` 可强制以特定 caps 建立链接。
- Caps 固定与否：
  - 固定（fixed）caps：参数确定，可获得明确的“编解码器描述”（参考教程 9）。
  - 非固定（unfixed）caps：参数待定，协商后才能确定具体格式。

---

## Bin 与 Pipeline、Ghost Pad

- Bin：把若干元素打包成一个“逻辑元素”，便于复用和管理。
- Pipeline：继承 Bin，附带 Bus 与时钟，是应用层交互的顶层对象。
- Ghost Pad：Bin 对外暴露的“代理 Pad”，把 Bin 内部某个 pad 暴露到 Bin 外部，使 Bin 可以与其它元素进行链接。
- 常见自动插件：
  - `decodebin/uridecodebin`：自动解复用/解码，动态输出音/视频 pad（Sometimes）。
  - `playbin`：高度集成的“播放器元素”，自动构建内部管线。

---

## 消息（Message）与总线（Bus）

- Pipeline 提供 Bus，元素把状态/错误/EOS等消息发送到 Bus。
- 应用通过：
  - 轮询：`gst_bus_timed_pop_filtered()`、`gst_bus_pop()`；
  - 或异步：`gst_bus_add_signal_watch()` + 信号回调；
  来获取消息并作出处理。
- 常见消息类型：
  - `GST_MESSAGE_ERROR`：错误（请 `gst_message_parse_error` 获取详细信息）。
  - `GST_MESSAGE_EOS`：流结束。
  - `GST_MESSAGE_STATE_CHANGED`：元素或管线状态变化（可用来调试状态迁移）。
  - 其他：`WARNING`、`INFO`、`STREAM_START`、`ASYNC_DONE` 等。

---

## 事件与查询（在流内传播）

- 事件（Event）：沿着数据路径传播，影响或通告流状态。
  - 例：`GST_EVENT_SEEK`（寻址）、`GST_EVENT_EOS`、`GST_EVENT_FLUSH_START/STOP`、`GST_EVENT_SEGMENT`、`GST_EVENT_CAPS`。
- 查询（Query）：从下游获取信息。
  - 例：`DURATION`（时长）、`POSITION`（当前位置）、`SEEKING`（是否可寻址）、`LATENCY`、`CONVERSION`。
- 与消息区别：消息是“异步通知给应用层”；事件/查询是在数据流内传递，通常由元素处理。

---

## 状态机与异步状态变更

- 四种状态：`NULL` → `READY` → `PAUSED` → `PLAYING`。
- 典型流程：创建后是 NULL；准备资源进入 READY；预滚动到 PAUSED；开始流动为 PLAYING。
- `gst_element_set_state()` 返回值：
  - `GST_STATE_CHANGE_SUCCESS`：立即完成；
  - `GST_STATE_CHANGE_ASYNC`：异步进行（常见于到 PAUSED/PLAYING 的转换）；
  - `GST_STATE_CHANGE_FAILURE`：失败。
- 异步完成时会有 `ASYNC_DONE`/`STATE_CHANGED` 消息。应用可结合 Bus 观察到最终状态。

---

## 时钟、时间与同步（简述）

- Pipeline 选择统一时钟（系统时钟、音频时钟或自定义）。
- 每个 Buffer 带时间戳（PTS/DTS）；Sink 按“运行时间”（running-time）进行调度与渲染。
- `base-time` 确定运行时间起点；暂停/恢复会调整基准，保证 A/V 同步。

---

## 常见问题与排错

- Not all elements could be created：
  - 检查插件是否安装（`gst-inspect-1.0 <element>`）。
- Elements could not be linked：
  - 检查 caps 兼容性，或加入 `*convert/*resample`，必要时用 `capsfilter`/`link_filtered`。
- 运行时 ERROR 或无输出：
  - 打印 Bus 上的 `ERROR`，查看 `debug_info`；
  - 提升日志：设置环境变量 `GST_DEBUG=3`（或更高）。
- 动态 pad 未链接：
  - 使用 `pad-added` 回调（见教程 3 与教程 7）。

---

## 关键 API 速查

- 元素/管线：
  - `gst_element_factory_make()`、`gst_pipeline_new()`、`gst_bin_add()`/`_many()`
  - `gst_element_link()`/`_many()`、`gst_element_link_filtered()`
  - `g_object_set()/get()`
- 状态与运行：
  - `gst_element_set_state()`、`gst_element_get_state()`
- Bus 与消息：
  - `gst_element_get_bus()`、`gst_bus_timed_pop_filtered()`、`gst_bus_add_signal_watch()`
  - `gst_message_parse_error()`、`GST_MESSAGE_STATE_CHANGED`、`GST_MESSAGE_EOS`
- Pads 与 Caps：
  - `gst_element_get_static_pad()`、`gst_pad_link()`
  - `gst_caps_new_simple()`、`gst_caps_from_string()`、`gst_caps_is_fixed()`

---

## 小结

- 元素通过 Pads 链接形成 Pipeline，媒体数据以 Buffer 形式流动。
- Caps 决定是否能“说得上话”，转换/重采样等元素帮助完成协商。
- 消息/事件/查询各司其职：应用读 Bus 上的消息，管线内部用事件与查询进行控制与信息获取。
- 掌握状态机与 Bus 处理，是稳定控制管线生命周期的关键。

延伸阅读：
- 动态管线与 `pad-added`：Basic Tutorial 3（本仓库：Basic/03_Dynamic_pipelines）
- 媒体格式与 Pad 能力：Basic Tutorial 6（Basic/06_Media_formats_and_Pad_Capabilities）
- 多线程与 Pad 可用性：Basic Tutorial 7（Basic/07_Multithreading_and_Pad_Availability）
- 媒体信息探测（GstDiscoverer）：Basic Tutorial 9（Basic/09_Media_information_gathering）

