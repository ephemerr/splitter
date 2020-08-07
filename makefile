all:
	cmake --build ./build

install: all


run:
	./build/splitter_test
