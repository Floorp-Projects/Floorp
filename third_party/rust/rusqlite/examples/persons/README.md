# Persons example

## Run

```
$ cargo run --example persons
```

## Run (wasm32-wasi)

### Requisites

- [wasi-sdk](https://github.com/WebAssembly/wasi-sdk)
- [wasmtime](https://wasmtime.dev/)

```
# Set to wasi-sdk directory
$ export WASI_SDK_PATH=`<wasi-sdk-path>`
$ export CC_wasm32_wasi="${WASI_SDK_PATH}/bin/clang --sysroot=${WASI_SDK_PATH}/share/wasi-sysroot"
# Build
$ cargo build --example persons --target wasm32-wasi --release --features bundled
# Run
$ wasmtime target/wasm32-wasi/release/examples/persons.wasm
Found persons:
ID: 1, Name: Steven
ID: 2, Name: John
ID: 3, Name: Alex
```
