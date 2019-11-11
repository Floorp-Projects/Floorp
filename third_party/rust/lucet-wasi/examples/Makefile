WASI_CC ?= /opt/wasi-sdk/bin/clang
WASI_LD ?= /opt/wasi-sdk/bin/wasm-ld

default: run-hello

.PHONY: run-hello
run-hello: build/hello.so
	cargo run -p lucet-wasi -- ./build/hello.so

.PHONY: run-hello-all
run-hello-all: run-hello
	cargo run -p lucet-wasi -- ./build/hello.so -- "makefile user"
	GREETING="goodbye" cargo run -p lucet-wasi -- ./build/hello.so -- "makefile user"

build/hello.so: build/hello.wasm ../bindings.json
	cargo run -p lucetc -- $< --bindings ../bindings.json -o $@

build/hello.wasm: hello.c
	mkdir -p build
	$(WASI_CC) $< -o $@

build/hello.wat: build/hello.wasm
	wasm2wat -f $< > $@

.PHONY: run-pseudoquine
run-pseudoquine: build/pseudoquine.so
	cargo run -p lucet-wasi -- ./build/pseudoquine.so --dir "$(CURDIR):/examples"

build/pseudoquine.so: build/pseudoquine.wasm ../bindings.json
	cargo run -p lucetc -- $< --bindings ../bindings.json -o $@

build/pseudoquine.wasm: pseudoquine.c
	mkdir -p build
	$(WASI_CC) $< -o $@

build/pseudoquine.wat: build/pseudoquine.wasm
	wasm2wat -f $< > $@

.PHONY: run-kgt
run-kgt: build/kgt.so
	cargo run -p lucet-wasi -- ./build/kgt.so -- -l bnf -e rrutf8 < build/kgt/examples/expr.bnf

build/kgt.so: build/kgt/build/bin/kgt ../bindings.json
	cargo run -p lucetc -- $< --bindings ../bindings.json -o $@

build/kgt/build/bin/kgt: build/kgt
	NOSTRIP=1 CC=$(WASI_CC) LD=$(WASI_LD) pmake -C build/kgt -r all

build/kgt:
	git clone --recursive https://github.com/katef/kgt.git build/kgt

.PHONY: clean
clean:
	@rm -rf build
