#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <gst/gst.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

//videotestsrc ! ffmpegcolorspace ! x264enc ! rtph264pay ! udpsink host=127.0.0.1 port=5000
GMainLoop *loop;

#define JETSON
//#undef JETSON

#define IMWIDTH 1920
#define IMHEIGHT 1080
#define PORT 5000
#ifdef JETSON
# define HOST "192.168.100.15"
# define CODEC "nv_omx_h264enc"
#else
# define HOST "192.168.100.15"
# define CODEC "x264enc"
#endif

#define LOCKFILE "/var/run/lock/vitstreaming.lock"

static char _T_framebuffer[IMWIDTH * IMHEIGHT] = {0xFF};

static void _t_sigusr1(int sig) {
	int width;
	int height;
	int md = shm_open("vit_image", O_RDONLY,  S_IRUSR | S_IWUSR | S_IRGRP |
				S_IWGRP | S_IROTH | S_IWOTH);
	
	if (NULL == getenv("VIT_WIDTH")) {
		width = IMWIDTH;
	}
	else {
		width = atoi(getenv("VIT_WIDTH"));
	}

	if (NULL == getenv("VIT_HEIGHT")) {
		height = IMHEIGHT;
	}
	else {
		height = atoi(getenv("VIT_HEIGHT"));
	}

	if (md != 0) {
		int rb;
		rb = read(md, _T_framebuffer, width * height);
		if (width * height != rb)
		{
			printf("Looks like frame is broken\n");
			printf("Read only %d bytes\n", rb);
		}
		close(md);
	}
	else {
		printf("Could not open shared memory\n");
	}
}

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
	int width;
	int height;
	if (NULL == getenv("VIT_WIDTH")) {
		width = IMWIDTH;
	}
	else {
		width = atoi(getenv("VIT_WIDTH"));
	}

	if (NULL == getenv("VIT_HEIGHT")) {
		height = IMHEIGHT;
	}
	else {
		height = atoi(getenv("VIT_HEIGHT"));
	}

	size = width * height * 1;

	buffer = gst_buffer_new();
	gst_buffer_set_data(buffer, _T_framebuffer, size);

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
  	GstElement *src, *colorspace, *codec, *wrapper, *netsink;
  	gboolean status;
  	GstCaps *capsRaw;
  	gchar *params;
  	FILE *lf;
	int width, height;
	/* Initialize GStreamer */
	gst_init (&argc, &argv);
	loop = g_main_loop_new(NULL, FALSE);

	/* Create pipeline */
	pipeline = gst_pipeline_new("truba");
	//src = gst_element_factory_make("videotestsrc", "src");
	src = gst_element_factory_make("appsrc", "src");
	colorspace = gst_element_factory_make("ffmpegcolorspace", "colorspaceconverter");
	codec = gst_element_factory_make(CODEC, "codec");
	wrapper = gst_element_factory_make("rtph264pay", "wrapper");
	netsink = gst_element_factory_make("udpsink", "netsink");
	
	if (NULL == getenv("VIT_WIDTH")) {
		width = IMWIDTH;
	}
	else {
		width = atoi(getenv("VIT_WIDTH"));
	}

	if (NULL == getenv("VIT_HEIGHT")) {
		height = IMHEIGHT;
	}
	else {
		height = atoi(getenv("VIT_HEIGHT"));
	}

	/* Set up pipeline */
	capsRaw = gst_caps_new_simple(	"video/x-raw-gray",
					"bpp", G_TYPE_INT, 8,
					"depth", G_TYPE_INT, 8,
					"width", G_TYPE_INT, width,
					"height", G_TYPE_INT, height,
					"framerate", GST_TYPE_FRACTION, 25, 1,
					NULL);
	g_signal_connect(src, "need-data", G_CALLBACK(cb_need_data), NULL);
	g_object_set(G_OBJECT(src), "caps", capsRaw, NULL);
	g_object_set(G_OBJECT(src), "stream-type", 0, "format",
		         GST_FORMAT_TIME, NULL);
	if (NULL == getenv("VIT_HOST"))
		g_object_set(G_OBJECT(netsink), "host", HOST, NULL);
	else {	
		g_object_set(G_OBJECT(netsink), "host", getenv("VIT_HOST"), NULL);
		printf("Connected to host %s\n", getenv("VIT_HOST"));
	}
	g_object_set(G_OBJECT(netsink), "port", PORT, NULL);

	gst_bin_add_many(GST_BIN(pipeline), src, colorspace, codec, wrapper,
		             netsink, NULL);
	status = gst_element_link_many(src, colorspace, codec, wrapper, netsink,
		                           NULL);

	if(!status) {
		printf("Linking elements failed!\n");
	}
	else {
		printf("Linking elements succeed!\n");
	}

	params = NULL;

	/* Create lock file */
	lf = fopen(LOCKFILE, "w");
	fprintf(lf, "%d", getpid());
	fclose(lf);

	/* Setup signal handler */
	if (SIG_ERR == signal(SIGUSR1, _t_sigusr1))
	{
		printf("Failed to spoof signal handler\n");
	}

	//Run
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	g_main_loop_run (loop);
	gst_element_set_state(pipeline, GST_STATE_NULL);

	//Deinit
	gst_object_unref(GST_OBJECT(pipeline));
	g_main_loop_unref(loop);
	return 0;
}
