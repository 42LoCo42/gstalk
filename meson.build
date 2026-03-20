project(
	'gstalk',
	'c',
	default_options: {
		'c_std': 'c23',
		'buildtype': 'debugoptimized',  #
		'warning_level': '2',
	},
)

executable(
	'gstalk',
	'src/main.c',
	'src/gstreamer.c',
	'src/input.c',
	'src/menu.c',
	'src/pipewire.c',
	'src/util.c',
	c_args: [
		'-Wno-empty-translation-unit',
		'-Wno-pragma-once-outside-header',
	],
	link_args: [
		'-lm',
	],
	dependencies: [
		dependency('gstreamer-1.0'),
		dependency('libpipewire-0.3'),
		dependency('notcurses'),
	],
	install: true,
)
