all: target/debug/libaudio_thread_priority.a
	g++ atp_test.cpp target/debug/libaudio_thread_priority.a -I. -lpthread -ldbus-1 -ldl -g

target/debug/libaudio_thread_priority.a:
	cargo build
