#pragma once

void launch_gstreamer(
	const char* rx_port, const char* tx_host, const char* tx_port
);

void toggleMute(bool mute);
void toggleDeaf(bool deaf);

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#define _POSIX_C_SOURCE 200112L
#include "util.c"

#include <gst/gst.h>
#include <stdio.h>

static const char receiver_src[] = {
#embed "receiver.txt"
	, 0
};

static const char transmitter_src[] = {
#embed "transmitter.txt"
	, 0
};

static GstElement* rx_pipeline = NULL;
static GstElement* tx_pipeline = NULL;

static char         buf[8192];
static GstElement** tgt = NULL;

static pthread_barrier_t barrier = {0};

static void* pipeline_fn(void*) {
	gst_init(NULL, NULL);

	GError* error = NULL;
	*tgt          = gst_parse_launch(buf, &error);
	if(*tgt == NULL) errx(1, "launch failed: %s", error->message);

	GstStateChangeReturn ret = gst_element_set_state(*tgt, GST_STATE_PLAYING);
	if(ret == GST_STATE_CHANGE_FAILURE) die("play pipeline");

	GstBus* bus = gst_element_get_bus(*tgt);
	if(bus == NULL) die("get pipeline bus");

	pthread_barrier_wait(&barrier);

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

	return NULL;
}

void launch_gstreamer(
	const char* rx_port, const char* tx_host, const char* tx_port
) {
	pthread_barrier_init(&barrier, NULL, 2);

	pthread_t rx_thread = {0};
	snprintf(buf, sizeof(buf), receiver_src, rx_port);
	tgt = &rx_pipeline;
	pthread_create(&rx_thread, NULL, pipeline_fn, NULL);
	pthread_barrier_wait(&barrier);

	pthread_t tx_thread = {0};
	snprintf(buf, sizeof(buf), transmitter_src, tx_host, tx_port);
	tgt = &tx_pipeline;
	pthread_create(&tx_thread, NULL, pipeline_fn, NULL);
	pthread_barrier_wait(&barrier);
}

void toggleMute(bool mute) {
	GstStateChangeReturn ret = gst_element_set_state(
		tx_pipeline, mute ? GST_STATE_PAUSED : GST_STATE_PLAYING
	);

	if(ret == GST_STATE_CHANGE_FAILURE) die("toggle mute");
}

void toggleDeaf(bool deaf) {
	GstStateChangeReturn ret = gst_element_set_state(
		rx_pipeline, deaf ? GST_STATE_PAUSED : GST_STATE_PLAYING
	);

	if(ret == GST_STATE_CHANGE_FAILURE) die("toggle mute");
}

#endif
