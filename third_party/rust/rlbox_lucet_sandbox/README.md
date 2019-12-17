# RLBOX Lucet Sandbox Integration
Integration with RLBox sandboxing API to leverage the sandboxing in WASM modules compiled with lucet compiler.

For details about the RLBox sandboxing APIs, see [here](https://github.com/PLSysSec/rlbox_api_cpp17).

Currently lucet supports only Linux x64, therefore this repo will only work in this environment.

## Building/Running the tests

You can build and run the tests using cmake with the following commands.

```bash
cmake -S . -B ./build
cmake --build ./build --target all
cmake --build ./build --target test
```

## Using this library

First, build the rlbox_lucet_sandbox repo with

```bash
cmake -S . -B ./build
cmake --build ./build --target all
```
(Note: The parallel build is currently broken for first build. Incremental parallel build works fine.)

This lucet/wasm integration with RLBox depends on 3 external tools/libraries that are pulled in **automatically** to run the tests included in this repo.

1. [A clang compiler with support for WASM/WASI backend, and the WASI sysroot](https://github.com/CraneStation/wasi-sdk). This allows you to compile C/C++ code to WASM modules usable outside of web browsers (in desktop applications).
2. [The **modified** lucet compiler](https://github.com/shravanrn/lucet/) that compiles the produced WASM/WASI module to a native binary.
3.  [The RLBox APIs]((https://github.com/PLSysSec/rlbox_api_cpp17)) - A set of APIs that allow easy use of sandboxed libraries.

In the below steps, you can either use the automatically pulled in versions as described below, or download the tools yourself.

In order to sandbox a library of your choice.

- Build the sources of your library along with the file `c_src/lucet_sandbox_wrapper.c` and passing the flag `--export-all` to the linker using the clang compiler described above. This will produce a wasm module. The required clang compiler is available in the path `build/_deps/wasiclang-src/opt/wasi-sdk/bin/clang`.
For instance, to edit an existing `make` based build system, you can run the commmand.

```bash
build/_deps/wasiclang-src/opt/wasi-sdk/bin/clang c_src/lucet_sandbox_wrapper.c -c c_src/lucet_sandbox_wrapper.o

CC=build/_deps/wasiclang-src/opt/wasi-sdk/bin/clang                            \
CXX=build/_deps/wasiclang-src/opt/wasi-sdk/bin/clang++                         \
C_FLAGS="--sysroot build/_deps/wasiclang-src/opt/wasi-sdk/share/wasi-sysroot/" \
LD=build/_deps/wasiclang-src/opt/wasi-sdk/bin/wasm-ld                          \
LDLIBS=lucet_sandbox_wrapper.o                                                 \
LDFLAGS=-Wl,--export-all                                                       \
make
```

- Assuming the above command produced the wasm module `libFoo`, compile this to an ELF shared library using the modified lucetc compiler as shown below.

```bash
build/_deps/lucet/target/release/lucetc                   \
    --bindings build/_deps/lucet/lucet-wasi/bindings.json \
    --guard-size "4GiB"                                   \
    --min-reserved-size "4GiB"                            \
    --max-reserved-size "4GiB"                            \
    libFoo                                                \
    -o libWasmFoo.so
```
- Finally you can write sandboxed code, just as you would with any other RLBox sandbox, such as in the short example below. For more detailed examples, please refer to the tutorial in the [RLBox Repo]((https://github.com/PLSysSec/rlbox_api_cpp17)).


```c++
#include "rlbox_lucet_sandbox.hpp"
#include "rlbox.hpp"

int main()
{
    rlbox_sandbox<rlbox_lucet_sandbox> sandbox;
    sandbox.create_sandbox("libWasmFoo.so");
    // Invoke function bar with parameter 1
    sandbox.invoke_sandbox_function(bar, 1);
    sandbox.destroy_sandbox();
    return 0;
}
```

- To compile the above example, you must include the rlbox header files in `build/_deps/rlbox-src/code/include`, the integration header files in `include/` and the lucet_sandbox library in `build/cargo/{debug or release}/librlbox_lucet_sandbox.so`. For instance, you can compile the above with

```bash
g++ -std=c++17 example.cpp -o example -I build/_deps/rlbox-src/code/include -I include -l:build/cargo/release/librlbox_lucet_sandbox.so
```

## Contributing Code

1. To contribute code, it is recommended you install clang-tidy which the build
uses if available. Install using:

   On Ubuntu:
```bash
sudo apt install clang-tidy
```
   On Arch Linux:
```bash
sudo pacman -S clang-tidy
```

2. It is recommended you treat warnings as errors during development. Also, you
probably want to use the debug build. To do this, adjust your build settings as
shown below

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DWERR=ON -S . -B ./build
```

3. After making changes to the source, add any new required tests and run all
tests as described earlier.

4. To make sure all code/docs are formatted with, we use clang-format.
Install using:

   On Ubuntu:
```bash
sudo apt install clang-format
```
   On Arch Linux:
```bash
sudo pacman -S clang-format
```

5. Format code with the format-source target:
```bash
cmake --build ./build --target format-source
```

6. Submit the pull request.
