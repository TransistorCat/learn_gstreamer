#include <gst/gst.h>

// Handler for the "pad-added" signal
static void pad_added_handler(GstElement *src, GstPad *new_pad, GstElement *sink);

int main(int argc, char *argv[]) {
  GstElement *pipeline, *source, *convert, *sink;
  GMainLoop *loop;

  g_setenv("GST_DEBUG_DUMP_DOT_DIR", ".", TRUE); //要放在gst_init之前

  // 1. Initialize GStreamer
  gst_init(&argc, &argv);

  // 2. Create the elements
  // We use uridecodebin which will internally create the right elements to decode the URI.
  // This makes it a good example for dynamic pad linking.
  source = gst_element_factory_make("uridecodebin", "source");
  convert = gst_element_factory_make("videoconvert", "convert");
  sink = gst_element_factory_make("autovideosink", "sink");

  // 3. Create the empty pipeline
  pipeline = gst_pipeline_new("test-pipeline");

  if (!pipeline || !source || !convert || !sink) {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  // 4. Build the pipeline. uridecodebin will be linked dynamically later.
  gst_bin_add_many(GST_BIN(pipeline), source, convert, sink, NULL);
  if (!gst_element_link(convert, sink)) {
    g_printerr("Elements could not be linked (convert -> sink).\n");
    gst_object_unref(pipeline);
    return -1;
  }

  // Set the URI to play. A standard test video is used.
  g_object_set(source, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);

  // Connect to the pad-added signal, which will fire when uridecodebin is ready
  g_signal_connect(source, "pad-added", G_CALLBACK(pad_added_handler), convert);

  // --- DOT FILE GENERATION ---

  // Set the environment variable to specify where to save the DOT files.
  // You can also do this from your shell: export GST_DEBUG_DUMP_DOT_DIR=.


  g_print("\nDumping initial pipeline state to 'initial_state.dot'...\n");
  // Use GST_DEBUG_BIN_TO_DOT_FILE for a single, named snapshot before running.
  // This captures the pipeline structure before uridecodebin has created its source pads.
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline),
                            GST_DEBUG_GRAPH_SHOW_ALL,
                            "initial_state");

  // 5. Start playing the pipeline
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  g_print("Pipeline is playing. Waiting for dynamic pads...\n");
  g_print("--> To view the graph, run: dot -Tpng initial_state.dot -o initial_state.png\n\n");


  // 6. Create a GLib Main Loop and wait for EOS or error
  loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);


  // 7. Free resources
  g_main_loop_unref(loop);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}

// This function will be called when uridecodebin has created a new pad for us to link.
static void pad_added_handler(GstElement *src, GstPad *new_pad, GstElement *data) {
  GstPad *sink_pad = gst_element_get_static_pad(data, "sink");
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;
  // Get the parent pipeline to dump its state
  GstElement *pipeline = GST_ELEMENT(gst_element_get_parent(gst_element_get_parent(src)));


  g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

  // If our converter is already linked, we have nothing to do here
  if (gst_pad_is_linked(sink_pad)) {
    g_print("  Pad is already linked. Ignoring.\n");
    goto exit;
  }

  // Check the new pad's type
  new_pad_caps = gst_pad_get_current_caps(new_pad);
  new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
  new_pad_type = gst_structure_get_name(new_pad_struct);
  if (!g_str_has_prefix(new_pad_type, "video/x-raw")) {
    g_print("  Pad has type '%s' which is not raw video. Ignoring.\n", new_pad_type);
    goto exit;
  }

  // Attempt the link
  ret = gst_pad_link(new_pad, sink_pad);
  if (GST_PAD_LINK_FAILED(ret)) {
    g_print("  Type is '%s' but link failed.\n", new_pad_type);
  } else {
    g_print("  Link succeeded (type '%s').\n", new_pad_type);

    // --- TIMESTAMPED DOT FILE GENERATION ---
    g_print("\nDumping pipeline state after linking to a timestamped file...\n");
    // Use GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS to capture this specific moment in time.
    // If more pads were added (e.g., audio), this would generate multiple unique files.
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(pipeline),
                                      GST_DEBUG_GRAPH_SHOW_ALL,
                                      "pad_linked_state");
    g_print("--> A new file named '<timestamp>_pad_linked_state.dot' was created.\n");
    g_print("--> To view it, run: dot -Tpng <timestamp>_pad_linked_state.dot -o linked.png\n\n");
  }

exit:
  if (new_pad_caps != NULL)
    gst_caps_unref(new_pad_caps);
  gst_object_unref(sink_pad);
  gst_object_unref(pipeline);
}
