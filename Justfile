run: build
	./build/gstalk

build:
	if [ ! -d build ]; then just _wipe; fi
	meson compile -C build

wipe: _wipe
	just run

_wipe:
	meson setup --wipe build
