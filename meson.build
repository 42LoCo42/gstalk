project(
	'gstalk',
	'c',
	default_options: {
		'c_std': 'c23',
		'buildtype': 'debugoptimized',  #
		'warning_level': '3',
	},
)

executable(
	'gstalk',
	'src/main.c',
	c_args: [],
	link_args: [],
	dependencies: [
		dependency('gstreamer-1.0'),
		dependency('libpulse'),
	],
	install: true,
)
