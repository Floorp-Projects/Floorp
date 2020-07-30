# WASI Examples

These are some examples showing how to build and run WASI programs with the Lucet runtime. By
default, the Makefile in this directory builds and runs a Hello World program, but see below for
other examples.

By default, these examples look for the WASI SDK to be installed in `/opt/wasi-sdk`, which is
present in the Lucet `devenv` environment. If you want to use your own SDK, you can override the
environment variables `WASI_CC` and `WASI_LD` with paths to your own `clang` and `wasm-ld`.

## Hello World

In addition to the standard Hello World behavior, this program also shows the use of command-line
arguments and environment variables:

```
cargo run -p lucet-wasi -- ./build/hello.so
   Compiling lucet-wasi v0.1.0 (/lucet/lucet-wasi)
    Finished dev [unoptimized + debuginfo] target(s) in 1.91s
     Running `/lucet/target/debug/lucet-wasi ./build/hello.so`
hello, wasi!
```

```
cargo run -p lucet-wasi -- ./build/hello.so -- "readme reader"
    Finished dev [unoptimized + debuginfo] target(s) in 0.10s
     Running `/lucet/target/debug/lucet-wasi ./build/hello.so -- 'readme reader'`
hello, readme reader!
```

```
GREETING="goodbye" cargo run -p lucet-wasi -- ./build/hello.so -- "readme reader"
    Finished dev [unoptimized + debuginfo] target(s) in 0.11s
     Running `/lucet/target/debug/lucet-wasi ./build/hello.so -- 'readme reader'`
goodbye, readme reader!
```

Use the `make run-hello` or `make run-hello-all` targets to run these variations.

## KGT

This example shows that a realistically-sized program, using standard IO and command-line arguments,
can be compiled against WASI with no modifications to its source. In addition to the WASI SDK
requirements mentioned above, the `kgt` example requires `pmake` to be installed:

```
# make run-kgt
cargo run -p lucet-wasi -- ./build/kgt.so -- -l bnf -e rrutf8 < build/kgt/examples/expr.bnf
    Finished dev [unoptimized + debuginfo] target(s) in 0.12s
     Running `/lucet/target/debug/lucet-wasi ./build/kgt.so -- -l bnf -e rrutf8`
expr:
    │├──╮── term ── "+" ── expr ──╭──┤│
        │                         │
        ╰───────── term ──────────╯

term:
    │├──╮── factor ── "*" ── term ──╭──┤│
        │                           │
        ╰───────── factor ──────────╯

factor:
    │├──╮── "(" ── expr ── ")" ──╭──┤│
        │                        │
        ╰──────── const ─────────╯

const:
    │├── integer ──┤│

```
