#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE

#include <err.h>
#include <gst/gst.h>
#include <math.h>
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
				(arr).ptr, ((arr).cap = (arr).cap ? (arr).cap << 1 : 16),      \
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

static void
pa_source_cb(pa_context*, const pa_source_info* i, int is_last, void*) {
	if(is_last) return;

	AudioSource source = {
		.index = i->index,
		.name  = strdup(i->description),
	};

	ArrayAdd(audioSources, source);
}

static void
pa_sink_input_cb(pa_context*, const pa_sink_input_info* i, int is_last, void*) {
	if(is_last) {
		pthread_barrier_wait(&barrier);
		return;
	}

	AudioSource source = {
		.index = i->index,
	};

	if(asprintf(
		   &source.name, "%s: %s", pa_proplist_gets(i->proplist, "node.name"),
		   i->name
	   ) == 1)
		die("asprintf audio source name");

	ArrayAdd(audioSources, source);
}

static void context_cb(pa_context* context, void*) {
	switch(pa_context_get_state(context)) {
	case PA_CONTEXT_READY:
		pthread_barrier_wait(&barrier);
		break;

	default:
	}
}

static void print_audio_sources(void) {
	if(pthread_barrier_init(&barrier, NULL, 2) < 0) die("thread barrier");

	pa_threaded_mainloop* mainloop = pa_threaded_mainloop_new();
	if(mainloop == NULL) die("PA mainloop");

	pa_mainloop_api* mainloop_api = pa_threaded_mainloop_get_api(mainloop);
	if(mainloop_api == NULL) die("PA mainloop API");

	pa_context* context = pa_context_new(mainloop_api, NULL);
	if(context == NULL) die("PA context");

	pa_context_set_state_callback(context, context_cb, NULL);
	if(pa_context_connect(context, NULL, 0, NULL) < 0)
		die("PA context connect");

	if(pa_threaded_mainloop_start(mainloop) < 0) die("PA mainloop run");
	pthread_barrier_wait(&barrier);

	pa_context_get_source_info_list(context, pa_source_cb, NULL);
	pa_context_get_sink_input_info_list(context, pa_sink_input_cb, NULL);
	pthread_barrier_wait(&barrier);

	pa_context_disconnect(context);
	pa_context_unref(context);
	pa_threaded_mainloop_stop(mainloop);
	pa_threaded_mainloop_free(mainloop);

	uint32_t largest_index = 0;
	for(size_t i = 0; i < audioSources.len; i++) {
		AudioSource it = audioSources.ptr[i];
		if(it.index > largest_index) largest_index = it.index;
	}

	for(size_t i = 0; i < audioSources.len; i++) {
		AudioSource it = audioSources.ptr[i];
		printf("%*u: %s\n", (int) log10(largest_index) + 2, it.index, it.name);
	}
}

[[noreturn]] static void _usage(const char* app) {
	errx(
		1,
		""
		"usage: %s mode...\n"
		"  mode ls: lists all pulseaudio sink inputs\n"
		"  mode rx: <port>\n"
		"  mode tx: <host> <port> [ID]",
		app
	);
}

#define usage() _usage(argv[0])

int main(int argc, char** argv) {
	if(argc < 2) usage();

	char* pipeline_src = NULL;

	if(strcmp(argv[1], "ls") == 0) {
		print_audio_sources();
		return 0;
	} else if(strcmp(argv[1], "rx") == 0) {
		if(argc != 3) usage();

		if(asprintf(&pipeline_src, receiver_src, argv[2]) == -1)
			die("asprintf receiver pipeline");
	} else if(strcmp(argv[1], "tx") == 0) {
		char* id;
		if(argc == 4) {
			print_audio_sources();
			printf("ID: ");

			size_t n = 0;
			if(getline(&id, &n, stdin) == -1) die("getline ID");
			id[strcspn(id, "\n")] = 0;
		} else if(argc == 5) {
			id = argv[4];
		} else {
			usage();
		}

		if(asprintf(&pipeline_src, transmitter_src, id, argv[2], argv[3]) == -1)
			die("asprintf transmitter pipeline");
	} else {
		usage();
	}

	printf("launching pipeline: %s\n", pipeline_src);

	gst_init(&argc, &argv);

	GError*     error    = NULL;
	GstElement* pipeline = gst_parse_launch(pipeline_src, &error);
	if(pipeline == NULL) gst_die("launch pipeline");

	GstStateChangeReturn ret =
		gst_element_set_state(pipeline, GST_STATE_PLAYING);
	if(ret == GST_STATE_CHANGE_FAILURE) die("play pipeline");

	GstBus* bus = gst_element_get_bus(pipeline);
	if(bus == NULL) die("get pipeline bus");

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
