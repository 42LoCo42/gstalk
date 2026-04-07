#include "gstreamer.c"
#include "input.c"
#include "menu.c"
#include "pipewire.c"
#include "util.c"

#include <assert.h>
#include <getopt.h>
#include <string.h>

int main(int argc, char** argv) {
	const char* rx_port = NULL;
	const char* tx_host = NULL;
	const char* tx_port = NULL;

	const char* ignore_pat = NULL;

	struct option longopts[] = {
		{
			.name    = "help",
			.has_arg = no_argument,
			.flag    = NULL,
			.val     = 'h',
		},
		{
			.name    = "version",
			.has_arg = no_argument,
			.flag    = NULL,
			.val     = 'V',
		},
		{
			.name    = "receive",
			.has_arg = required_argument,
			.flag    = NULL,
			.val     = 'r',
		},
		{
			.name    = "transmit",
			.has_arg = required_argument,
			.flag    = NULL,
			.val     = 't',
		},
		{
			.name    = "ignore",
			.has_arg = required_argument,
			.flag    = NULL,
			.val     = 'i',
		},
		{0},
	};

	for(;;) {
		switch(getopt_long(argc, argv, "hVr:t:i:", longopts, NULL)) {
		case -1:
			goto launch;

		case '?':
			usage(true);

		case 'h':
			usage(false);

		case 'V':
			printf("%s %s\n", APP_NAME, VERSION);
			exit(0);

		case 'r':
			rx_port = optarg;
			break;

		case 't':
			char* sep = strchr(optarg, ':');
			if(sep == NULL) {
				warnx("transmit is not in host:port format!");
				usage(true);
			}

			*sep    = 0;
			tx_host = optarg;
			tx_port = sep + 1;
			break;

		case 'i':
			ignore_pat = optarg;
			break;
		}
	}

launch:
	if(rx_port == NULL) {
		warnx("receive is unset!");
		usage(true);
	}

	if(tx_host == NULL || tx_port == NULL) {
		warnx("transmit is unset!");
		usage(true);
	}

	launch_pipewire(ignore_pat);
	launch_gstreamer(rx_port, tx_host, tx_port);
	launch_menu();
	launch_input();

	return 0;
}
