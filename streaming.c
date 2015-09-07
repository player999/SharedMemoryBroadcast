#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

//videotestsrc ! x264enc ! rtph264pay ! udpsink host=127.0.0.1 port=5000
GMainLoop *loop;

static void
cb_need_data (GstElement *appsrc,
	      guint       unused_size,
	      gpointer    user_data)
{
	static gboolean white = FALSE;
	static GstClockTime timestamp = 0;
	GstBuffer *buffer;
	guint size;
	GstFlowReturn ret;

	size = 385 * 288 * 2;

	buffer = gst_buffer_new_and_alloc (size);

	/* this makes the image black/white */
	memset(GST_BUFFER_DATA(buffer), white ? 0xff : 0x0, size);

	white = !white;

	GST_BUFFER_TIMESTAMP(buffer) = timestamp;
	GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 25);

	timestamp += GST_BUFFER_DURATION(buffer);

	g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
	gst_buffer_unref(buffer);

	if (ret != GST_FLOW_OK) {
		/* something wrong, stop pushing */
		g_main_loop_quit(loop);
	}
}

int main(int argc, char *argv[])
{
	GstElement *pipeline;
  	GstElement *src, *codec, *wrapper, *netsink;
  	gboolean status;
  	GstCaps *capsRaw;
  	gchar *params;

	/* Initialize GStreamer */
	gst_init (&argc, &argv);
	loop = g_main_loop_new(NULL, FALSE);

	/* Create pipeline */
	pipeline = gst_pipeline_new("truba");
	//src = gst_element_factory_make("videotestsrc", "src");
	src = gst_element_factory_make("appsrc", "src");
	codec = gst_element_factory_make("x264enc", "codec");
	wrapper = gst_element_factory_make("rtph264pay", "wrapper");
	netsink = gst_element_factory_make("udpsink", "netsink");

	/* Set up pipeline */
	capsRaw = gst_caps_new_simple(	"video/x-raw-yuv", "format", GST_TYPE_FOURCC,
									GST_MAKE_FOURCC ('I', '4', '2', '0'),
									"width", G_TYPE_INT, 384,
									"height", G_TYPE_INT, 288,
									"framerate", GST_TYPE_FRACTION, 25, 1,
									NULL);
	g_signal_connect(src, "need-data", G_CALLBACK(cb_need_data), NULL);
	g_object_set(G_OBJECT(src), "caps", capsRaw, NULL);
	g_object_set(G_OBJECT(src), "stream-type", 0, "format",
		         GST_FORMAT_TIME, NULL);
	g_object_set(G_OBJECT(netsink), "host", "192.168.100.15", NULL);
	g_object_set(G_OBJECT(netsink), "port", 5000, NULL);

	gst_bin_add_many(GST_BIN(pipeline), src, codec, wrapper, netsink, NULL);
	status = gst_element_link_many(src, codec, wrapper, netsink, NULL);

	if(!status) {
		printf("Linking elements failed!\n");
	}
	else {
		printf("Linking elements succeed!\n");
	}

	params = NULL;
	//Run

	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	g_main_loop_run (loop);
	gst_element_set_state(pipeline, GST_STATE_NULL);

	//Deinit
	gst_object_unref(GST_OBJECT(pipeline));
	g_main_loop_unref(loop);
	return 0;
}