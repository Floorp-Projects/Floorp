all:
	cd .. && cargo build
	g++ -Wall -o test test.cpp ../target/debug/librust*.a -ldl -lpthread -lrt -lgcc_s -lpthread -lc -lm -std=c++0x
	./test
