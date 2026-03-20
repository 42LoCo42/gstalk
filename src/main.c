#include "gstreamer.c"
#include "input.c"
#include "menu.c"
#include "pipewire.c"
#include "util.c"

static int usage() {
	errx(
		1,
		""
		"usage: %s <rx port> <tx host> <tx port>",
		APP_NAME
	);
}

int main(int argc, char** argv) {
	store();

	const char* rx_port = shift();
	const char* tx_host = shift();
	const char* tx_port = shift();

	launch_pipewire();
	launch_gstreamer(rx_port, tx_host, tx_port);
	launch_menu();
	launch_input();

	return 0;
}
