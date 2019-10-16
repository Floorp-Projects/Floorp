all: target/debug/libaudio_thread_priority.a
	g++ atp_test.cpp target/debug/libaudio_thread_priority.a -I. -lpthread -ldbus-1 -ldl -g -o atp_test

check:
	@./atp_test && echo "test passed" || echo "test failed"

target/debug/libaudio_thread_priority.a:
	cargo build
