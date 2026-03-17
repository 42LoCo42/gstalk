run: build
	./build/gstalk

build:
	if [ ! -d build ]; then just _wipe; fi
	meson compile -C build

leaks: build
	valgrind --leak-check=full --show-leak-kinds=all ./build/gstalk ls

wipe: _wipe
	just run

_wipe:
	meson setup --wipe build
