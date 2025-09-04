# GStreamer 基础教程 11：调试工具（Debugging tools）

本文是对“Debugging tools”教程的中文学习笔记，聚焦实用的调试手段与工作流，帮助你在管线搭建、能力协商和运行时问题定位方面更高效。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/debugging-tools.html?gi-language=c

---

## 1) 快速入门：命令行与日志开关

常用的命令行工具与通用开关（与第 10 篇工具速览配合使用）：
- -v：在 gst-launch-1.0 中显示更详细的协商信息与属性。
- -m：打印 Bus 消息（状态变更、标签、警告、错误等）。
- -t：为日志消息打上时间戳。
- -q：安静模式（更少输出）。
- -e：在退出时发送 EOS，便于收尾。

示例：
- gst-launch-1.0 -v -m -t playbin uri=file:///path/to/file

---

## 2) GST_DEBUG：分类与级别

GStreamer 的调试日志由“分类(category) + 级别(level)”组成，通过环境变量 GST_DEBUG 或可执行文件通用选项 --gst-debug 控制。

- 级别（从低到高）：ERROR, WARNING, FIXME, INFO, DEBUG, LOG, TRACE（更高等级打印更细）。
- 分类：按模块/子系统划分，如 GST_PADS, GST_CAPS, playbin, decodebin, GST_PLUGIN_LOADING 等。

用法示例：
- 全局 info：
  - GST_DEBUG=*:4 gst-launch-1.0 playbin uri=...
- 指定分类更详细：
  - GST_DEBUG=playbin:5,GST_PADS:5,decodebin:5,*:3 gst-launch-1.0 playbin uri=...
- 通过命令行传参：
  - gst-launch-1.0 --gst-debug=GST_PADS:5,*:2 playbin uri=...
- 查看可用分类与帮助：
  - gst-launch-1.0 --gst-debug-help | | cat

输出到文件与配色：
- 写入文件：GST_DEBUG_FILE=/tmp/gst.log
- 关闭彩色：GST_DEBUG_NO_COLOR=1

---

## 3) 常见排错思路与“分类小抄”

- 链接失败 could not link / not-negotiated：
  - 提升 GST_PADS 和 GST_CAPS 日志：GST_DEBUG=GST_PADS:5,GST_CAPS:5,*:2
  - 在管线中补 videoconvert/audioconvert/audioresample 或添加格式过滤 caps。
- 自动插入/选择失败（playbin/decodebin 行为难以理解）：
  - GST_DEBUG=playbin:5,uridecodebin:5,decodebin:5,*:3
- 插件相关（无法找到元素或 Missing plugin）：
  - GST_DEBUG=GST_PLUGIN_LOADING:5,*:2
  - 检查 GST_PLUGIN_PATH 与已安装的插件包；用 gst-inspect-1.0 验证。
- 事件/时间戳/同步问题：
  - GST_DEBUG=GST_EVENT:5,GST_CLOCK:5,*:3
- 内存与引用计数追踪（更高级）：
  - 可尝试提高 LOG/TRACE 级别，或结合 Tracer（见附录）。

---

## 4) 导出 Pipeline 拓扑图（DOT）

调试协商或复杂拓扑时，导出图很有帮助。

方式 A：在你的 C 代码中使用宏（推荐用于应用程序调试）：
- 在合适的位置调用：
  - GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "my-pipeline");
  - GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "my-pipeline");
- 运行前设置输出目录：
  - export GST_DEBUG_DUMP_DOT_DIR=/tmp
- 生成的 .dot 文件用 Graphviz 渲染：
  - dot -Tpng /tmp/my-pipeline.dot -o /tmp/my-pipeline.png

方式 B：借助工具快速获取（适合粗略观察）：
- 许多示例程序/工具在关键状态变化时会触发 dot dump（或你可在示例源码里加上上述宏后再运行）。
- 同样设置 GST_DEBUG_DUMP_DOT_DIR 后运行你的程序或 gst-launch-1.0 管线；在目录中查找生成的 .dot 文件并用 Graphviz 查看。

小贴士：
- 如果没有任何 .dot 生成，多半是应用未调用相关宏；建议在源码中显式添加。
- 渲染时也可用 fdp/neato 等布局：fdp -Tpng x.dot -o x.png。

---

## 5) Bus 消息：错误定位与用户反馈

无论是示例程序还是你的应用，都应监听 Bus。

典型流程：
- 关注错误（ERROR）、警告（WARNING）、流结束（EOS）、状态变更（STATE_CHANGED）。
- 一旦错误，获取 GError 与 debug 字符串，打印出来或写入日志文件。

最小 C 代码片段：
```c
GstBus *bus = gst_element_get_bus(pipeline);
for (;;) {
  GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED);
  if (!msg) continue;

  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
      GError *err = NULL; gchar *dbg = NULL;
      gst_message_parse_error(msg, &err, &dbg);
      g_printerr("ERROR: %s\nDEBUG: %s\n", err->message, dbg ? dbg : "-");
      g_error_free(err); g_free(dbg);
      break;
    }
    case GST_MESSAGE_EOS:
      g_print("EOS reached\n");
      break;
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
        GstState old_s, new_s, pending;
        gst_message_parse_state_changed(msg, &old_s, &new_s, &pending);
        g_print("Pipeline state: %s -> %s\n",
                gst_element_state_get_name(old_s),
                gst_element_state_get_name(new_s));
      }
      break;
    default: break;
  }
  gst_message_unref(msg);
}
gst_object_unref(bus);
```

---

## 6) 常用环境变量与工具组合

- GST_DEBUG：控制日志分类与级别（见上）。
- GST_DEBUG_FILE：将调试日志写入文件。
- GST_DEBUG_NO_COLOR=1：关闭彩色输出，便于保存与 grep。
- GST_DEBUG_DUMP_DOT_DIR：设置 DOT 文件输出目录。
- GST_PLUGIN_PATH：附加插件搜索路径（排查“找不到元素/解码器”时常用）。
- gst-inspect-1.0 element：查看元素属性、Pad 模板与 Caps。
- gst-typefind-1.0 file：探测文件类型。
- gst-discoverer-1.0 uri：在不播放的情况下收集媒体信息。

---

## 7) 调试工作流范式

1) 先用 gst-play-1.0 或 playbin 验证媒体/源是否基本可播。
2) 失败时：
   - 升级日志：GST_DEBUG=*:4 或针对模块：GST_PADS:5,GST_CAPS:5。
   - 用 -v 观察协商；必要时添加 videoconvert/audioconvert/audioresample。
3) 复杂拓扑：
   - 在代码里加 DOT 宏导出图，用 Graphviz 看看数据路径与队列阻塞。
4) 插件/格式问题：
   - gst-inspect-1.0 检查元素可用性与支持的 caps；根据提示补插件包。
5) 稳定后：
   - 将关键日志写入文件，便于回溯；保留一份 DOT 图和最小复现场景。

---

## 8) 进阶：Tracer（性能与泄漏）

GStreamer 提供 Tracer 框架，可对时延、内存泄漏、丢帧等进行观测。简要提示：
- 启用方式（示例）：
  - GST_TRACERS="latency;leaks" GST_DEBUG="GST_TRACER:7,*:3" your-app
- 常见类型：latency（端到端/元素级时延）、leaks（引用计数泄漏）、stats（吞吐/负载）。
- 输出会进入常规调试日志；配合 GST_DEBUG_FILE 更易分析。

注：Tracer 功能与版本相关，按需查阅文档与系统是否包含对应插件。

---

## 9) 常见问题速查

- could not link/ not-negotiated：检查 caps；插入 *convert/*resample；提高 GST_PADS/GST_CAPS 日志。
- Missing plugin：用 gst-inspect-1.0 搜；安装 good/bad/ugly/libav 等对应包；检查 GST_PLUGIN_PATH。
- 音画不同步/卡顿：快速排查可先对 sink 设 sync=false；再检查时钟/时间戳日志（GST_CLOCK）。
- 网络流断续：提升 uridecodebin/rtpjitterbuffer 等分类日志；关注重传/丢包指标。

---

## 小结

- 用 GST_DEBUG 精准控制日志粒度，配合 -v/-m/-t 快速观察运行态。
- 在代码中加入 DOT 宏，导出拓扑图直观定位问题。
- 结合 gst-inspect/typefind/discoverer 校验格式、能力与插件。
- 需要更深的性能洞察时，考虑启用 Tracer。

掌握这些调试工具与套路，能显著提升定位与解决问题的效率。
