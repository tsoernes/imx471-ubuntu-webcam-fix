/* SPDX-License-Identifier: GPL-2.0-only
 * Exercise the real relay lifecycle using synthetic frames, no camera device.
 */
#define main relay_entrypoint
#include "../relay/v4l2-relayd.c"
#undef main

static gint frames;

static void
frame_received (GstElement *sink G_GNUC_UNUSED,
                GstBuffer *buffer G_GNUC_UNUSED,
                GstPad *pad G_GNUC_UNUSED,
                gpointer data G_GNUC_UNUSED)
{
  g_atomic_int_inc (&frames);
}

static void
drive (gint milliseconds)
{
  gint64 deadline = g_get_monotonic_time () + milliseconds * 1000;
  do {
    while (g_main_context_iteration (NULL, FALSE))
      ;
    g_usleep (1000);
  } while (g_get_monotonic_time () < deadline);
}

int
main (int argc, char **argv)
{
  gst_init (&argc, &argv);
  opt_isolate_input = g_strcmp0 (g_getenv ("RELAY_TEST_ISOLATE"), "1") == 0;
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "V4L2_RELAYD", 0, "relay tests");
  loop = g_main_loop_new (NULL, FALSE);
  opt_input = "videotestsrc is-live=true ! video/x-raw,format=YUY2,width=160,height=120,framerate=30/1 ! identity";
  opt_splash = "videotestsrc num-buffers=2 pattern=black ! video/x-raw,format=YUY2,width=160,height=120,framerate=30/1 ! identity";
  opt_output = "appsrc name=appsrc is-live=true max-buffers=3 leaky-type=downstream caps=video/x-raw,format=YUY2,width=160,height=120,framerate=30/1 ! fakesink name=test-sink signal-handoffs=true sync=false";
  output_pipeline = output_pipeline_create ();
  g_assert_nonnull (output_pipeline);
  GstElement *sink = gst_bin_get_by_name (GST_BIN (output_pipeline), "test-sink");
  g_signal_connect (sink, "handoff", G_CALLBACK (frame_received), NULL);
  gst_object_unref (sink);
  gst_element_set_state (output_pipeline, GST_STATE_PLAYING);
  drive (100);

  for (guint cycle = 0; cycle < 5; ++cycle) {
    gint before = g_atomic_int_get (&frames);
    input_pipeline_enable ();
    drive (400);
    g_assert_cmpint (g_atomic_int_get (&frames) - before, >=, 8);
    GstElement *original = input_pipeline;
    GWeakRef weak;
    g_weak_ref_init (&weak, G_OBJECT (original));

    /* A quick device probe/reopen must cancel the pending destruction. */
    input_pipeline_disable ();
    if (opt_isolate_input) {
      g_assert_null (input_pipeline);
      g_assert_cmpint (source_worker_pid, ==, 0);
      g_assert_cmpint (source_worker_fd, ==, -1);
      input_pipeline_enable ();
      drive (200);
      input_pipeline_disable ();
    } else {
      g_assert_cmpuint (idle_release_id, !=, 0);
      input_pipeline_enable ();
      g_assert_cmpuint (idle_release_id, ==, 0);
      g_assert_true (input_pipeline == original);
      drive (200);
    }

    /* Sustained idle must drop both the graph and its bus-watch reference. */
    input_pipeline_disable ();
    drive (3100);
    g_assert_null (input_pipeline);
    g_assert_cmpuint (input_bus_watch_id, ==, 0);
    g_assert_cmpuint (idle_release_id, ==, 0);
    GObject *remaining = g_weak_ref_get (&weak);
    g_assert_null (remaining);
    g_weak_ref_clear (&weak);
    g_assert_cmpint (GST_STATE (output_pipeline), ==, GST_STATE_PLAYING);
    g_print ("cycle %u: delivered frames, reopened, idle graph finalized%s\n",
             cycle + 1, opt_isolate_input ? ", worker reaped" : ", pending release cancelled on rapid reopen");
  }

  if (output_bus_watch_id)
    g_source_remove (output_bus_watch_id);
  if (splash_bus_watch_id)
    g_source_remove (splash_bus_watch_id);
  gst_element_set_state (splash_pipeline, GST_STATE_NULL);
  gst_clear_object (&splash_pipeline);
  gst_element_set_state (output_pipeline, GST_STATE_NULL);
  gst_clear_object (&output_pipeline);
  g_main_loop_unref (loop);
  gst_deinit ();
  return 0;
}
