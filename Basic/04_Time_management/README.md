# GStreamer 基础教程 4：时间管理（Time management）

本文是对 “Time management”（C 版）教程的中文学习笔记，聚焦如何在应用中管理“时间”相关的能力：查询播放位置与总时长、判断是否可寻址（seekable）、执行精准或按关键帧的定位（seek），以及通过总线消息与定时器维护稳定的时间显示。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/basic/time-management.html?gi-language=c

---

## 目标与要点（知识点）
- 查询当前位置与媒体总时长：
  - gst_element_query_position(..., GST_FORMAT_TIME, &pos)
  - gst_element_query_duration(..., GST_FORMAT_TIME, &dur)
- 判断媒体是否支持 seek：
  - gst_element_query_seeking(..., GST_FORMAT_TIME, &seekable, &start, &end)
- 执行定位（seek）：
  - 简化 API：gst_element_seek_simple(pipeline, GST_FORMAT_TIME, flags, new_pos_ns)
  - 完整 API：gst_element_seek(rate, format, flags, start_type, start, stop_type, stop)
- 监听关键消息并更新 UI/状态：
  - DURATION_CHANGED：需要重新查询总时长
  - STATE_CHANGED：可据此启停位置刷新定时器
  - ERROR/EOS：出错与播放结束处理
- 正确处理未知时长和尚未就绪的状态（GST_CLOCK_TIME_NONE），注意使用纳秒时间基与工具宏（如 GST_SECOND、GST_TIME_ARGS）。

---

## 代码结构概览（basic-tutorial-4 思路）

- 元素与管线
  - 常用 playbin 简化播放（支持本地文件或网络 URI）
  - 也可以自建 decodebin + sinks，本教程重点在“时间管理”，与具体解复用解码链无强绑定

- 数据结构（示例）
  - CustomData:
    - GstElement* pipeline
    - GstClockTime duration（初始为 GST_CLOCK_TIME_NONE）
    - gboolean seek_enabled / seek_done（可选，用于 UI 控制）
    - GMainLoop* loop

- 定时器（每 500ms 或 1s）
  - 通过 g_timeout_add_seconds 定时查询 position 和（必要时）duration
  - 打印/更新 UI：Position XX:XX / Duration XX:XX
  - 若 duration 为 GST_CLOCK_TIME_NONE，先尝试 query_duration；若仍为 NONE，暂不显示

- 总线回调
  - ERROR：打印错误并退出
  - EOS：播放到结尾，退出
  - DURATION_CHANGED：将缓存的 duration 置回 NONE，下次定时器回调中重新查询
  - STATE_CHANGED（来自 pipeline 本身）：可在从 PAUSED 切至 PLAYING 时启用位置刷新

- 键盘控制（可选，教程常用）
  - P：播放/暂停
  - S/s：前进/后退固定秒数（例如 ±10s）
  - Q：退出
  - 执行 seek 前先检查 seekable

---

## 工作流程

1. 创建并初始化 pipeline（例如使用 playbin 设置 uri）。
2. 将 pipeline 切至 PLAYING。
3. 注册 Bus 监听 ERROR/EOS/DURATION_CHANGED/STATE_CHANGED。
4. 启动定时器周期性查询 position/duration 并输出进度。
5. 用户交互触发 seek：
   - 先用 gst_element_query_seeking 判断是否可寻址（seekable）
   - 计算新位置 new_pos = clamp(current + delta, start, end)
   - 使用 gst_element_seek_simple 执行定位，flags 常用：
     - GST_SEEK_FLAG_FLUSH：丢弃旧数据尽快切到新段
     - GST_SEEK_FLAG_KEY_UNIT：按关键帧对齐，速度快但不够精确
     - 如需高精度可用 GST_SEEK_FLAG_ACCURATE（性能可能稍差）
6. 结束时清理：将 pipeline 置为 NULL 并 unref。

---

## 典型代码片段（伪代码）

- 定时打印位置与时长
```
static gboolean print_position(gpointer user_data) {
  CustomData* data = (CustomData*)user_data;
  gint64 pos = GST_CLOCK_TIME_NONE, dur = data->duration;

  if (!GST_IS_ELEMENT(data->pipeline)) return G_SOURCE_REMOVE;

  // 查询位置
  if (gst_element_query_position(data->pipeline, GST_FORMAT_TIME, &pos)) {
    // 查询时长（若未知或被标记为需要刷新）
    if (dur == GST_CLOCK_TIME_NONE) {
      if (gst_element_query_duration(data->pipeline, GST_FORMAT_TIME, &dur)) {
        data->duration = dur;
      }
    }
    g_print("Position %" GST_TIME_FORMAT " / ",
            GST_TIME_ARGS(pos));
    if (dur != GST_CLOCK_TIME_NONE)
      g_print("%" GST_TIME_FORMAT "\n", GST_TIME_ARGS(dur));
    else
      g_print("unknown\n");
  }

  return G_SOURCE_CONTINUE; // 继续定时回调
}
```

- 判断是否可寻址并前进 10 秒
```
static void seek_forward_10s(CustomData* data) {
  gboolean seekable = FALSE;
  gint64 start = 0, end = 0, pos = 0;

  if (!gst_element_query_seeking(data->pipeline, GST_FORMAT_TIME,
                                 &seekable, &start, &end) || !seekable) {
    g_print("Stream is NOT seekable.\n");
    return;
  }

  if (!gst_element_query_position(data->pipeline, GST_FORMAT_TIME, &pos)) {
    g_print("Cannot get current position.\n");
    return;
  }

  gint64 new_pos = pos + 10 * GST_SECOND;
  if (end > 0 && new_pos > end) new_pos = end;

  if (!gst_element_seek_simple(data->pipeline, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, new_pos)) {
    g_print("Seek failed.\n");
  }
}
```

- 处理 DURATION_CHANGED
```
static void on_message(GstBus* bus, GstMessage* msg, gpointer user_data) {
  CustomData* data = (CustomData*)user_data;
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_DURATION_CHANGED:
      data->duration = GST_CLOCK_TIME_NONE; // 下次刷新时重查
      break;
    case GST_MESSAGE_ERROR:
      // 打印后退出
      break;
    case GST_MESSAGE_EOS:
      // 结束退出
      break;
    default:
      break;
  }
}
```

---

## 常见问题与排错

- 无法获取时长或位置（返回 FALSE 或为 GST_CLOCK_TIME_NONE）
  - 媒体尚未解析完或元素未到合适状态；稍后重试
  - 对直播流或不支持时间的格式，duration 可能长期未知
- seek 无效或失败
  - 媒体不支持 seek（如直播）；先用 gst_element_query_seeking 判断
  - flags 选择不当：ACCURATE 更精确但慢，KEY_UNIT 依关键帧更快但非帧级精确
  - 某些容器/解码器不支持精准 seek 或支持有限
- 定时查询太频繁导致开销
  - 使用 g_timeout_add_seconds(1) 即可满足 UI 需求；无需 100ms 级别
- 打印时间格式
  - 使用 GST_TIME_ARGS(ts) 配合 GST_TIME_FORMAT，避免自行换算纳秒

---

## 关键 API 速查

- 查询
  - gst_element_query_position(element, GST_FORMAT_TIME, &pos_ns)
  - gst_element_query_duration(element, GST_FORMAT_TIME, &dur_ns)
  - gst_element_query_seeking(element, GST_FORMAT_TIME, &seekable, &start_ns, &end_ns)
- 定位
  - gst_element_seek_simple(element, GST_FORMAT_TIME, flags, new_pos_ns)
  - gst_element_seek(rate, format, flags, start_type, start, stop_type, stop)
- 常量与宏
  - GST_CLOCK_TIME_NONE、GST_SECOND
  - GST_TIME_FORMAT / GST_TIME_ARGS
  - GST_SEEK_FLAG_FLUSH、GST_SEEK_FLAG_KEY_UNIT、GST_SEEK_FLAG_ACCURATE
- 消息
  - GST_MESSAGE_DURATION_CHANGED、GST_MESSAGE_ERROR、GST_MESSAGE_EOS、GST_MESSAGE_STATE_CHANGED

---

## 小结

时间管理的核心在于三件事：可靠地“查询”（位置与时长）、判断“可寻址性”（seekable）、以及正确地“执行 seek”。结合总线消息（DURATION_CHANGED/ERROR/EOS）与周期性刷新（定时器），就能在不依赖 GUI 的前提下构建一个稳定、可交互的时间控制框架。进一步，你可以把此模式接入 GUI 或键盘控制，实现暂停/继续、前进/后退、跳转到指定时间等常用播放器功能。