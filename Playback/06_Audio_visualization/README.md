# GStreamer 回放教程 6：Audio visualization（音频可视化）

本文是对 playback-tutorial-6.c 的中文学习笔记，演示如何在 GStreamer 中启用音频可视化功能，将音频流转换为视觉效果，为音乐播放器增添炫酷的视觉体验。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/playback/audio-visualization.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 `GST_PLAY_FLAG_VIS` 标志启用音频可视化：
  - 当播放纯音频流时自动激活可视化效果。
  - 如果媒体已包含视频，该标志无效果。
- 通过 `vis-plugin` 属性指定可视化元素：
  - 默认使用 `goom` 可视化插件。
  - 可以自定义选择其他可视化元素。
- 插件注册表查询：使用 `gst_registry_feature_filter()` 查找所有可视化插件。
- 元素工厂系统：理解 `GstPlugin`、`GstPluginFeature`、`GstElementFactory` 的层次结构。
- 可视化插件分类：通过元素类别字符串 "Visualization" 筛选相关插件。

---

## 代码结构概览（playback-tutorial-6.c）
- 定义 `filter_vis_features()` 过滤函数：筛选注册表中的可视化插件。
- 查询可用插件：使用 `gst_registry_feature_filter()` 获取所有可视化元素工厂。
- 插件选择逻辑：遍历插件列表，优先选择 GOOM 或第一个可用插件。
- 创建 `playbin` 管道：设置网络音频流 URI。
- 启用可视化标志：添加 `GST_PLAY_FLAG_VIS` 到 `playbin` 标志。
- 设置可视化插件：通过 `vis-plugin` 属性指定选中的可视化元素。

---

## 工作流程（音频可视化）
1. `gst_init()` 初始化；定义可视化插件过滤函数。
2. 查询注册表：`gst_registry_feature_filter()` 获取所有可视化插件列表。
3. 遍历插件列表：打印可用插件名称，选择 GOOM 或备选插件。
4. 创建可视化元素：使用 `gst_element_factory_create()` 实例化选中的插件。
5. 构建 `playbin` 管道：设置音频流 URI（如网络电台）。
6. 启用可视化：获取当前标志，添加 `GST_PLAY_FLAG_VIS`，重新设置。
7. 指定可视化插件：通过 `vis-plugin` 属性传递创建的可视化元素。
8. 开始播放：`gst_element_set_state(pipeline, GST_STATE_PLAYING)`。
9. 等待结束：监听 Bus 消息直到 EOS 或错误。
10. 清理资源：释放插件列表、Bus、管道等资源。

---

## 典型输出（示例片段）
```
Available visualization plugins:
  Object Detection Overlay
  Waveform oscilloscope
  Synaescope
  Frequency spectrum scope
  Stereo visualizer
  Monoscope
  GOOM: what a GOOM! 2k1 edition
  GOOM: what a GOOM!
Selected 'GOOM: what a GOOM!'

[播放开始，显示音频可视化效果]
```
- 首先列出系统中所有可用的可视化插件
- 自动选择 GOOM 插件（如果可用）作为默认可视化效果
- 播放音频时同时显示对应的视觉效果窗口

---

## 常见问题与排错
- 没有可视化插件：确保安装了 `gst-plugins-good` 或 `gst-plugins-bad` 包，包含常用可视化元素。
- GOOM 插件不可用：
  - 检查是否安装了 `gstreamer1.0-plugins-good` 包。
  - 可以选择其他可视化插件如 `monoscope`、`wavescope` 等。
- 可视化窗口不显示：
  - 确保系统支持图形输出（X11、Wayland 等）。
  - 检查是否有其他程序占用显示资源。
- 网络音频流无法播放：验证 URI 可访问性，检查网络连接和防火墙设置。
- 视频流中可视化无效：`GST_PLAY_FLAG_VIS` 仅对纯音频流有效，包含视频的媒体会忽略此标志。

---

## 扩展与自定义

### 1) 用户选择可视化插件
添加交互式插件选择功能：
```c
static GstElementFactory* choose_visualization_plugin(GList *list) {
  gint choice, count = 0;
  GList *walk;
  
  g_print("Available visualization plugins:\n");
  for (walk = list; walk != NULL; walk = g_list_next(walk)) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY(walk->data);
    g_print("%d: %s\n", count++, gst_element_factory_get_longname(factory));
  }
  
  g_print("Choose plugin (0-%d): ", count-1);
  scanf("%d", &choice);
  
  walk = g_list_nth(list, choice);
  return walk ? GST_ELEMENT_FACTORY(walk->data) : NULL;
}
```

### 2) 可视化参数配置
为可视化插件设置自定义属性：
```c
static void configure_visualization(GstElement *vis_plugin) {
  /* GOOM 特定配置 */
  if (g_str_has_prefix(GST_OBJECT_NAME(vis_plugin), "goom")) {
    g_object_set(vis_plugin, "width", 800, "height", 600, NULL);
  }
  
  /* Monoscope 配置 */
  if (g_str_has_prefix(GST_OBJECT_NAME(vis_plugin), "monoscope")) {
    g_object_set(vis_plugin, "klass", "monoscope", NULL);
  }
}
```

### 3) 多可视化效果切换
实现运行时切换不同可视化效果：
```c
static void switch_visualization(GstElement *pipeline, GstElement *new_vis) {
  gst_element_set_state(pipeline, GST_STATE_PAUSED);
  g_object_set(pipeline, "vis-plugin", new_vis, NULL);
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}
```

### 4) 自定义可视化窗口
控制可视化输出窗口属性：
```c
static void setup_visualization_window(GstElement *pipeline) {
  GstElement *video_sink;
  
  /* 创建自定义视频输出 */
  video_sink = gst_element_factory_make("xvimagesink", "video-output");
  g_object_set(video_sink, 
               "force-aspect-ratio", TRUE,
               "window-width", 640,
               "window-height", 480,
               NULL);
  
  g_object_set(pipeline, "video-sink", video_sink, NULL);
}
```

---

## 关键 API 速查
- 标志与属性：
  - `GST_PLAY_FLAG_VIS`（启用音频可视化标志）；
  - `vis-plugin`（指定可视化插件属性）；
  - `flags`（playbin 标志属性）。
- 注册表查询：
  - `gst_registry_get()`（获取全局插件注册表）；
  - `gst_registry_feature_filter()`（按条件过滤插件特性）；
  - `gst_plugin_feature_list_free()`（释放插件列表）。
- 元素工厂：
  - `GST_IS_ELEMENT_FACTORY()`（检查是否为元素工厂）；
  - `gst_element_factory_get_klass()`（获取元素类别字符串）；
  - `gst_element_factory_get_longname()`（获取元素长名称）；
  - `gst_element_factory_create()`（从工厂创建元素实例）。
- 常用可视化插件：
  - `goom`、`goom2k1`（GOOM 系列）；
  - `monoscope`（单声道示波器）；
  - `wavescope`（波形示波器）；
  - `spectrascope`（频谱分析器）。

---

## 小结
音频可视化功能为 GStreamer 应用提供了将声音转换为视觉效果的强大能力。通过简单的标志设置和插件选择，开发者可以轻松为音频播放器添加炫酷的视觉体验。GStreamer 的插件架构使得可视化效果具有高度的可扩展性，从简单的波形显示到复杂的 3D 效果都能支持。结合适当的用户界面和参数配置，这项技术可广泛应用于音乐播放器、DJ 软件、音频分析工具等场景，为用户提供丰富的视听体验。
