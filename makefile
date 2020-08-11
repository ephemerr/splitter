all:
	cmake --build ./build

install: all

configure:
	cmake . -B./build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug

run:
	./build/splitter_test
