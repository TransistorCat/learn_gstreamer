# GStreamer 回放教程 4：Progressive streaming（渐进式下载）

本文是对 playback-tutorial-4.c 的中文学习笔记，演示如何在 GStreamer 中实现渐进式下载，通过启用 `playbin` 的 `DOWNLOAD` 标志将流媒体数据本地存储，提升网络弹性和用户体验。

参考官方教程：
- https://gstreamer.freedesktop.org/documentation/tutorials/playback/progressive-streaming.html?gi-language=c

---

## 目标与要点（知识点）
- 使用 `playbin` 的 `GST_PLAY_FLAG_DOWNLOAD` 标志启用渐进式下载：
  - 将所有下载数据存储在本地临时文件中。
  - 支持快速回放已下载的片段，无需重新下载。
  - 通过 `queue2` 元素自动管理本地存储。
- 监听 `deep-notify::temp-location` 信号获取临时文件位置。
- 使用 `Buffering Query` 查询下载进度和可用数据范围。
- 实现可视化进度条显示下载状态和播放位置。
- 通过 `ring-buffer-max-size` 属性限制缓存文件大小。

---

## 代码结构概览（playback-tutorial-4.c）
- 创建 `playbin` 并设置网络媒体 `uri`。
- 启用下载标志：获取 `flags` 属性，添加 `GST_PLAY_FLAG_DOWNLOAD`，重新设置。
- 连接 `deep-notify::temp-location` 信号：监听 `queue2` 元素的临时文件位置变化。
- 可选：设置 `ring-buffer-max-size` 限制临时文件大小。
- 处理 `GST_MESSAGE_BUFFERING` 消息：根据缓冲级别暂停/恢复播放。
- 定时刷新 UI：使用 `gst_query_new_buffering()` 查询下载范围，绘制可视化进度条。
- 显示当前播放位置：查询 `position` 和 `duration`，在进度条中标记。

---

## 工作流程（渐进式下载）
1. `gst_init()` 初始化；创建 `playbin` 并设置网络媒体 `uri`。
2. 启用下载功能：`flags |= GST_PLAY_FLAG_DOWNLOAD`。
3. 连接临时文件位置监听：`g_signal_connect(pipeline, "deep-notify::temp-location", ...)`。
4. 可选：设置缓存大小限制 `ring-buffer-max-size`。
5. `gst_element_set_state(pipeline, GST_STATE_PLAYING)` 开始播放和下载。
6. 处理 Bus 消息：特别关注 `BUFFERING` 消息，根据缓冲级别控制播放状态。
7. 定时更新 UI：查询缓冲范围，绘制下载进度和播放位置。
8. 结束：将元素置为 `NULL`，释放资源（临时文件默认自动删除）。

---

## 典型输出（示例片段）
```
Temporary file: /tmp/gstreamer-1.0/gst_temp_XXXXXX
[----    >           ] Buffering:  45%
[---------->         ]                
[----------------    ] Buffering:  89%
[------------------X-]                
```
- 第一行显示临时文件路径
- 进度条中 `-` 表示已下载区域，`>` 表示正常播放位置，`X` 表示缓冲中的位置
- 缓冲不足时显示百分比，充足时显示空格

---

## 常见问题与排错
- 网络连接问题：确保 URI 可访问，检查防火墙和代理设置。
- 临时文件找不到：
  - Linux/macOS：通常在 `/tmp/gstreamer-1.0/` 目录下；
  - Windows：可能在"临时 Internet 文件"文件夹中，使用控制台查找。
- 缓冲频繁暂停：网络带宽不足，考虑降低媒体质量或增加初始缓冲时间。
- 磁盘空间不足：设置 `ring-buffer-max-size` 限制临时文件大小。
- 直播流不适用：渐进式下载主要用于点播内容，直播流缓冲逻辑不同。

---

## 扩展与自定义

### 1) 保留临时文件
默认情况下临时文件在程序退出时删除，可在 `got_location` 回调中保留：
```c
static void got_location (GstObject *gstobject, GstObject *prop_object, 
                         GParamSpec *prop, gpointer data) {
  gchar *location;
  g_object_get (G_OBJECT (prop_object), "temp-location", &location, NULL);
  g_print ("Temporary file: %s\n", location);
  
  /* 保留临时文件 */
  g_object_set (G_OBJECT (prop_object), "temp-remove", FALSE, NULL);
  
  g_free (location);
}
```

### 2) 限制缓存大小
通过 `ring-buffer-max-size` 属性控制临时文件最大大小：
```c
/* 限制为 4MB */
g_object_set (pipeline, "ring-buffer-max-size", (guint64)4000000, NULL);
```
超出限制时会覆盖已播放的区域，节省磁盘空间。

### 3) 高级缓冲查询
获取详细的缓冲信息：
```c
GstQuery *query = gst_query_new_buffering (GST_FORMAT_PERCENT);
if (gst_element_query (pipeline, query)) {
  gint n_ranges = gst_query_get_n_buffering_ranges (query);
  for (gint i = 0; i < n_ranges; i++) {
    gint64 start, stop;
    gst_query_parse_nth_buffering_range (query, i, &start, &stop);
    g_print ("Range %d: %ld%% - %ld%%\n", i, start, stop);
  }
}
gst_query_unref (query);
```

### 4) 自定义进度显示
可以创建图形界面的进度条替代文本显示：
```c
/* 获取下载百分比 */
gint64 start, stop;
if (gst_query_parse_nth_buffering_range (query, 0, &start, &stop)) {
  gdouble download_progress = (gdouble)stop / 100.0;
  /* 更新 GUI 进度条 */
  update_progress_bar (download_progress);
}
```

---

## 关键 API 速查
- 标志与属性：
  - `GST_PLAY_FLAG_DOWNLOAD`（启用渐进式下载）；
  - `ring-buffer-max-size`（限制临时文件大小）；
  - `temp-location`、`temp-remove`（临时文件控制）。
- 信号：
  - `deep-notify::temp-location`（临时文件位置变化）。
- 查询：
  - `gst_query_new_buffering()`（创建缓冲查询）；
  - `gst_query_get_n_buffering_ranges()`（获取缓冲范围数量）；
  - `gst_query_parse_nth_buffering_range()`（解析具体范围）。
- Bus 消息：
  - `GST_MESSAGE_BUFFERING`（缓冲状态变化）；
  - `gst_message_parse_buffering()`（解析缓冲级别）。
- 元素：
  - `queue2`（内部队列元素，负责数据存储和缓冲）。

---

## 小结
渐进式下载通过 `playbin` 的 `DOWNLOAD` 标志提供了网络弹性的流媒体播放体验。它将下载的数据本地存储，支持快速回放和网络中断恢复，同时提供丰富的查询接口用于实现进度显示和用户交互。这种技术在现代流媒体应用中广泛使用，特别适合网络条件不稳定的环境。通过合理配置缓存策略和用户界面，可以显著提升播放体验。
