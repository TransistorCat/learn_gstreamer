# GStreamer 回放教程 5：Color Balance（颜色平衡）

本文是对 playback-tutorial-5.c 的中文学习笔记，演示如何在 GStreamer 中使用 `GstColorBalance` 接口实时调整视频的亮度、对比度、色调和饱和度等颜色平衡参数。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/playback/color-balance.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 `GstColorBalance` 接口访问颜色平衡功能：
  - 查询可用的颜色平衡通道（`GstColorBalanceChannel`）。
  - 获取每个通道的名称、最小值和最大值范围。
  - 实时读取和修改通道的当前值。
- `playbin` 自动处理颜色平衡接口：
  - 如果管道中的元素支持该接口，`playbin` 直接转发给应用程序。
  - 否则自动插入 `colorbalance` 元素提供功能。
- 键盘交互控制：通过按键实时调整亮度、对比度、色调、饱和度。
- 百分比显示：将原始数值转换为 0-100% 的用户友好格式。

---

## 代码结构概览（playback-tutorial-5.c）
- 创建 `playbin` 并设置媒体 `uri`（网络视频流）。
- 实现 `print_current_values()` 函数：遍历所有颜色平衡通道，显示当前百分比值。
- 实现 `update_color_channel()` 函数：根据通道名称查找并调整指定通道的数值。
- 键盘事件处理 `handle_keyboard()`：监听按键输入，映射到具体的颜色调整操作。
- 主循环：设置键盘监听，启动播放，进入 GLib 主循环等待用户交互。

---

## 工作流程（颜色平衡调整）
1. `gst_init()` 初始化；创建 `playbin` 并设置网络媒体 `uri`。
2. 设置键盘输入监听：`g_io_add_watch()` 注册键盘事件回调函数。
3. `gst_element_set_state(pipeline, GST_STATE_PLAYING)` 开始播放。
4. 显示初始颜色平衡值：调用 `print_current_values()` 输出所有通道的当前百分比。
5. 用户按键交互：
   - 'C'/'c'：增加/减少对比度（CONTRAST）
   - 'B'/'b'：增加/减少亮度（BRIGHTNESS）
   - 'H'/'h'：增加/减少色调（HUE）
   - 'S'/'s'：增加/减少饱和度（SATURATION）
   - 'Q'：退出程序
6. 每次调整后自动显示更新后的所有通道值。
7. 结束：将元素置为 `NULL`，释放资源。

---

## 典型输出（示例片段）
```
USAGE: Choose one of the following options, then press enter:
 'C' to increase contrast, 'c' to decrease contrast
 'B' to increase brightness, 'b' to decrease brightness
 'H' to increase hue, 'h' to decrease hue
 'S' to increase saturation, 's' to decrease saturation
 'Q' to quit

CONTRAST:  50% BRIGHTNESS:  50% HUE:  50% SATURATION:  50% 
[用户按 'C']
CONTRAST:  60% BRIGHTNESS:  50% HUE:  50% SATURATION:  50% 
[用户按 'b']
CONTRAST:  60% BRIGHTNESS:  40% HUE:  50% SATURATION:  50% 
```
- 初始状态所有通道通常为 50%（中间值）
- 大写字母增加数值，小写字母减少数值
- 每次调整步长为通道范围的 10%

---

## 常见问题与排错
- 某些通道不可用：不是所有视频源都支持全部四个颜色平衡通道，检查 `channels` 列表确认可用通道。
- 调整无明显效果：
  - 确保视频内容有足够的颜色信息（黑白视频对色调/饱和度调整不敏感）。
  - 某些硬件加速解码器可能不支持颜色调整。
- 键盘输入无响应：确保终端焦点在程序窗口，检查 `stdin` 重定向问题。
- 网络视频加载慢：使用本地视频文件测试，或检查网络连接状态。
- 数值超出范围：代码已包含边界检查，但自定义实现时需注意 `min_value` 和 `max_value` 限制。

---

## 扩展与自定义

### 1) 保存和恢复设置
实现配置文件功能保存用户偏好：
```c
static void save_color_settings(GstElement *pipeline, const gchar *filename) {
  FILE *file = fopen(filename, "w");
  const GList *channels = gst_color_balance_list_channels(GST_COLOR_BALANCE(pipeline));
  
  for (const GList *l = channels; l != NULL; l = l->next) {
    GstColorBalanceChannel *channel = (GstColorBalanceChannel *)l->data;
    gint value = gst_color_balance_get_value(GST_COLOR_BALANCE(pipeline), channel);
    fprintf(file, "%s=%d\n", channel->label, value);
  }
  fclose(file);
}
```

### 2) GUI 滑块控制
使用 GTK+ 创建图形界面替代键盘控制：
```c
static void on_scale_value_changed(GtkRange *range, gpointer user_data) {
  ColorChannelData *data = (ColorChannelData *)user_data;
  gint value = (gint)gtk_range_get_value(range);
  gst_color_balance_set_value(GST_COLOR_BALANCE(data->pipeline), 
                              data->channel, value);
}
```

### 3) 预设配置
定义常用的颜色预设（如"鲜艳"、"柔和"、"黑白"）：
```c
typedef struct {
  const gchar *name;
  gint contrast, brightness, hue, saturation;
} ColorPreset;

static ColorPreset presets[] = {
  {"Vivid", 70, 55, 50, 80},
  {"Soft", 40, 60, 50, 30},
  {"B&W", 60, 50, 50, 0},
};
```

### 4) 实时预览
结合视频截图功能提供调整前后对比：
```c
static void capture_frame_comparison(GstElement *pipeline) {
  /* 捕获当前帧 */
  GstSample *sample;
  g_signal_emit_by_name(pipeline, "convert-sample", 
                        gst_caps_from_string("image/jpeg"), &sample);
  /* 保存为对比图片 */
}
```

---

## 关键 API 速查
- 接口与查询：
  - `GST_COLOR_BALANCE(pipeline)`（转换为颜色平衡接口）；
  - `gst_color_balance_list_channels()`（获取可用通道列表）；
  - `gst_color_balance_get_value()`（读取通道当前值）；
  - `gst_color_balance_set_value()`（设置通道新值）。
- 通道结构：
  - `GstColorBalanceChannel`（通道信息结构体）；
  - `channel->label`（通道名称，如"CONTRAST"）；
  - `channel->min_value`、`channel->max_value`（数值范围）。
- 常见通道名：
  - "CONTRAST"（对比度）、"BRIGHTNESS"（亮度）；
  - "HUE"（色调）、"SATURATION"（饱和度）。
- 输入处理：
  - `g_io_add_watch()`（添加 IO 监听）；
  - `g_ascii_tolower()`、`g_ascii_isupper()`（字符大小写处理）。

---

## 小结
颜色平衡功能通过 `GstColorBalance` 接口为 GStreamer 应用提供了强大的实时视频调色能力。该接口抽象了底层实现细节，无论是硬件加速还是软件处理都能统一访问。通过简单的查询和设置操作，开发者可以轻松实现专业级的视频颜色调整功能。结合适当的用户界面设计，这项技术可广泛应用于视频播放器、直播软件、视频编辑工具等场景，为用户提供个性化的视觉体验。
