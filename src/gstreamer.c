#pragma once

void launch_pipeline(const char* pipeline_src);

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#include "util.c"

#include <gst/gst.h>

void launch_pipeline(const char* pipeline_src) {
	gst_init(NULL, NULL);

	GError*     error    = NULL;
	GstElement* pipeline = gst_parse_launch(pipeline_src, &error);
	if(pipeline == NULL) errx(1, "launch failed: %s", error->message);

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

#endif
