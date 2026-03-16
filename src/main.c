#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE

#include <err.h>
#include <gst/gst.h>
#include <pthread.h>
#include <pulse/pulseaudio.h>
#include <stdio.h>

#define die(...) err(1, __VA_ARGS__)
#define gst_die(fmt, ...)                                                      \
	errx(1, fmt ": %s", __VA_ARGS__ __VA_OPT__(, ) error->message);

const char transmitter_src[] = {
#embed "transmitter.txt"
	, 0
};

const char receiver_src[] = {
#embed "receiver.txt"
	, 0
};

#define Array(t)                                                               \
	typedef struct {                                                           \
		t*     ptr;                                                            \
		size_t len;                                                            \
		size_t cap;                                                            \
	}

#define ArrayAdd(arr, x)                                                       \
	do {                                                                       \
		if((arr).len >= (arr).cap)                                             \
			(arr).ptr = reallocarray(                                          \
				(arr).ptr, (arr).cap ? ((arr).cap <<= 2) : 16,                 \
				sizeof((arr.ptr)[0])                                           \
			);                                                                 \
                                                                               \
		arr.ptr[(arr).len++] = (x);                                            \
	} while(0)

typedef struct {
	uint32_t index;
	char*    name;
} AudioSource;

Array(AudioSource) AudioSources;
static AudioSources audioSources = {0};

static pthread_barrier_t barrier = {0};

static void sink_info_cb(
	pa_context* context, const pa_sink_input_info* i, int is_last,
	void* userdata
) {
	(void) userdata;

	if(is_last) {
		pthread_barrier_wait(&barrier);
		pa_context_disconnect(context);
		return;
	}

	AudioSource source = {
		.index = i->index,
	};

	if(asprintf(
		   &source.name, "%s: %s", pa_proplist_gets(i->proplist, "node.name"),
		   i->name
	   ) == 1)
		die("asprintf failed");

	ArrayAdd(audioSources, source);
}

static void context_cb(pa_context* context, void* userdata) {
	(void) userdata;

	switch(pa_context_get_state(context)) {
	case PA_CONTEXT_READY:
		pa_operation* op =
			pa_context_get_sink_input_info_list(context, sink_info_cb, NULL);
		if(op == NULL) die("get sink input list failed");
		break;

	default:
	}
}

static void* pulse_fuckery(void*) {
	pa_mainloop* mainloop = pa_mainloop_new();
	if(mainloop == NULL) die("mainloop failed");

	pa_mainloop_api* mainloop_api = pa_mainloop_get_api(mainloop);
	if(mainloop_api == NULL) die("mainloop_api failed");

	pa_context* context = pa_context_new(mainloop_api, NULL);
	if(context == NULL) die("context failed");

	pa_context_set_state_callback(context, context_cb, NULL);
	if(pa_context_connect(context, NULL, 0, NULL) < 0)
		die("context connect failed");

	int ret = 0;
	if(pa_mainloop_run(mainloop, &ret) < 0) die("mainloop run failed");

	return NULL;
}

void _usage(const char* app) {
	errx(
		1,
		""
		"usage: %s mode...\n"
		"  mode ls: lists all pulseaudio sink inputs\n"
		"  mode rx: <port>\n"
		"  mode tx: <dev> <host> <port>",
		app
	);
}

#define usage() _usage(argv[0])

int main(int argc, char** argv) {
	if(argc < 2) usage();

	char* pipeline_src = NULL;

	if(strcmp(argv[1], "ls") == 0) {
		if(pthread_barrier_init(&barrier, NULL, 2) < 0)
			die("pthread_barrier_init");

		pthread_t pa_thread = {0};
		if(pthread_create(&pa_thread, NULL, pulse_fuckery, NULL) != 0)
			die("thread create failed");

		pthread_barrier_wait(&barrier);

		for(size_t i = 0; i < audioSources.len; i++) {
			AudioSource it = audioSources.ptr[i];
			printf("%u: %s\n", it.index, it.name);
		}

		return 0;
	} else if(strcmp(argv[1], "rx") == 0) {
		if(argc != 3) usage();
		if(asprintf(&pipeline_src, receiver_src, argv[2]) == -1)
			die("asprintf failed");
	} else if(strcmp(argv[1], "tx") == 0) {
		if(argc != 5) usage();
		if(asprintf(
			   &pipeline_src, transmitter_src, argv[2], argv[3], argv[4]
		   ) == -1)
			die("asprintf failed");
	} else {
		usage();
	}

	gst_init(&argc, &argv);

	GError*     error    = NULL;
	GstElement* pipeline = gst_parse_launch(pipeline_src, &error);
	if(pipeline == NULL) gst_die("launch failed");

	GstStateChangeReturn ret =
		gst_element_set_state(pipeline, GST_STATE_PLAYING);
	if(ret == GST_STATE_CHANGE_FAILURE) die("play failed");

	GstBus* bus = gst_element_get_bus(pipeline);
	if(bus == NULL) die("get bus failed");

	GstMessage* msg = gst_bus_timed_pop_filtered(
		bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS
	);

	if(msg != NULL) {
		GError* error;
		gchar*  debug;

		switch(GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &error, &debug);
			g_printerr(
				"error in element %s: %s\n", GST_OBJECT_NAME(msg->src),
				error->message
			);
			g_printerr("debug: %s\n", debug ? debug : "N/A");
			g_clear_error(&error);
			g_free(debug);
			break;

		case GST_MESSAGE_EOS:
			g_print("end of stream!\n");
			break;

		default:
			g_printerr("unexpected message received!\n");
			break;
		}
	}
}
