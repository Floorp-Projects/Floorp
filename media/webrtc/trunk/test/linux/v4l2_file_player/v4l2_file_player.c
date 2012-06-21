/**
 * WebRTC Internal Tooling - Looping V4L2 player
 * Author: Patrik Höglund (phoglund@webrtc.org)
 *
 * This file implements a basic raw video file player which loops its input
 * file indefinitely. It is intended to be used with the v4l2loopback driver in
 * order to play the contents of a file as if it were a webcam. Therefore, this
 * program will write to a v4l2loopback device, say /dev/video1, and the driver
 * will then reflect the data unmodified to any reading processes.
 *
 * This program basically does the same thing as
 * gst-launch-0.10 -v filesrc location=resources/foreman_cif.yuv ! \
 *   videoparse width=352 height=288 ! v4l2sink device=/dev/video1
 *
 * , but with looping. Looping is unfortunately impossible to do with a regular
 * gstreamer pipeline, hence the existence of this program.
 *
 * It can be run something like this:
 * v4l2_file_player foreman_cif_short.yuv 352 288 /dev/video1 >& /dev/null
 *
 * The program might print warnings about failed ioctls (for instance for
 * VIDIOC_G_FMT), but that doesn't seem to be a problem.
 *
 * To test, you can run for instance mplayer tv:// -tv device=/dev/video1 and
 * verify that you see your video playing repeatedly.
 *
 * This code is based off the hello world example in the gstreamer manual.
 * You can find the original code in chapter 10, "Your First Application":
 * http://gstreamer.freedesktop.org/data/doc/gstreamer/head/manual/html/
 */

#include <gst/gst.h>
#include <glib.h>

typedef struct {
  GMainLoop* main_loop;
  GstElement* pipeline;
} main_loop_and_pipeline_t;

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
  main_loop_and_pipeline_t *main_loop_and_pipeline =
      (main_loop_and_pipeline_t*)data;

  GMainLoop *loop = main_loop_and_pipeline->main_loop;
  GstElement *pipeline = main_loop_and_pipeline->pipeline;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      /* Reached end of input video - restart */
      gst_element_set_state(pipeline, GST_STATE_READY);
      gst_element_set_state(pipeline, GST_STATE_PLAYING);
      break;

    case GST_MESSAGE_ERROR: {
      gchar *debug;
      GError *error;

      gst_message_parse_error(msg, &error, &debug);
      g_free(debug);

      g_printerr("Error: %s\n", error->message);
      g_error_free(error);

      g_main_loop_quit(loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

int main(int argc, char *argv[]) {
  gst_init(&argc, &argv);

  GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

  /* Check input arguments */
  if (argc != 5 || atoi(argv[2]) == 0 || atoi(argv[3]) == 0) {
    g_printerr("Usage: %s <filename> <width> <height> <device>\n\n", argv[0]);
    g_printerr("Arguments:\n");
    g_printerr("  filename: Path to the video file.\n");
    g_printerr("  width:    Video width.\n");
    g_printerr("  height:   Video height.\n");
    g_printerr("  device:   Device to write to (like /dev/video1).\n");
    return -1;
  }

  /* Create gstreamer pipeline elements. */
  GstElement *pipeline = gst_pipeline_new("looping-video-player");
  GstElement *source = gst_element_factory_make("filesrc", "file-source");
  GstElement *parse = gst_element_factory_make("videoparse", "video-parse");
  GstElement *v4l2_sink = gst_element_factory_make("v4l2sink", "v4l2-sink");

  if (!pipeline || !source || !parse || !v4l2_sink) {
    g_printerr("One GST element could not be created. Exiting.\n");
    return -1;
  }

  /* Set up input parameters. */
  g_object_set(G_OBJECT (source), "location", argv[1], NULL);
  g_object_set(G_OBJECT (parse), "width", atoi(argv[2]), NULL);
  g_object_set(G_OBJECT (parse), "height", atoi(argv[3]), NULL);
  g_object_set(G_OBJECT (v4l2_sink), "device", argv[4], NULL);

  /* Add a callback to we can react to end-of-stream and errors. */
  main_loop_and_pipeline_t main_loop_and_pipeline;
  main_loop_and_pipeline.main_loop = main_loop;
  main_loop_and_pipeline.pipeline = pipeline;

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE (pipeline));
  gst_bus_add_watch(bus, bus_call, &main_loop_and_pipeline);
  gst_object_unref(bus);

  /* Assemble the pipeline. */
  gst_bin_add_many(GST_BIN (pipeline), source, parse, v4l2_sink, NULL);
  gst_element_link(source, parse);
  gst_element_link(parse, v4l2_sink);

  /* Start playing. */
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  g_main_loop_run(main_loop);

  /* Never reached. */
  return 0;
}
