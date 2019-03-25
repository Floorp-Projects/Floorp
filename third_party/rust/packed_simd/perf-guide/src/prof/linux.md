# Performance profiling on Linux

## Using `perf`

[perf](https://perf.wiki.kernel.org/) is the most powerful performance profiler
for Linux, featuring support for various hardware Performance Monitoring Units,
as well as integration with the kernel's performance events framework.

We will only look at how can the `perf` command can be used to profile SIMD code.
Full system profiling is outside of the scope of this book.

### Recording

The first step is to record a program's execution during an average workload.
It helps if you can isolate the parts of your program which have performance
issues, and set up a benchmark which can be easily (re)run.

Build the benchmark binary in release mode, after having enabled debug info:

```sh
$ cargo build --release
Finished release [optimized + debuginfo] target(s) in 0.02s
```

Then use the `perf record` subcommand:

```sh
$ perf record --call-graph=dwarf ./target/release/my-program
[ perf record: Woken up 10 times to write data ]
[ perf record: Captured and wrote 2,356 MB perf.data (292 samples) ]
```

Instead of using `--call-graph=dwarf`, which can become pretty slow, you can use
`--call-graph=lbr` if you have a processor with support for Last Branch Record
(i.e. Intel Haswell and newer).

`perf` will, by default, record the count of CPU cycles it takes to execute
various parts of your program. You can use the `-e` command line option
to enable other performance events, such as `cache-misses`. Use `perf list`
to get a list of all hardware counters supported by your CPU.

### Viewing the report

The next step is getting a bird's eye view of the program's execution.
`perf` provides a `ncurses`-based interface which will get you started.

Use `perf report` to open a visualization of your program's performance:

```sh
perf report --hierarchy -M intel
```

`--hierarchy` will display a tree-like structure of where your program spent
most of its time. `-M intel` enables disassembly output with Intel syntax, which
is subjectively more readable than the default AT&T syntax.

Here is the output from profiling the `nbody` benchmark:

```
- 100,00% nbody
  - 94,18% nbody
    + 93,48% [.] nbody_lib::simd::advance
    + 0,70% [.] nbody_lib::run
    + 5,06% libc-2.28.so
```

If you move with the arrow keys to any node in the tree, you can the press `a`
to have `perf` _annotate_ that node. This means it will:

- disassemble the function

- associate every instruction with the percentage of time which was spent executing it

- interleaves the disassembly with the source code,
  assuming it found the debug symbols
  (you can use `s` to toggle this behaviour)

`perf` will, by default, open the instruction which it identified as being the
hottest spot in the function:

```
0,76  │ movapd xmm2,xmm0
0,38  │ movhlps xmm2,xmm0
      │ addpd  xmm2,xmm0
      │ unpcklpd xmm1,xmm2
12,50 │ sqrtpd xmm0,xmm1
1,52  │ mulpd  xmm0,xmm1
```

In this case, `sqrtpd` will be highlighted in red, since that's the instruction
which the CPU spends most of its time executing.

## Using Valgrind

Valgrind is a set of tools which initially helped C/C++ programmers find unsafe
memory accesses in their code. Nowadays the project also has

- a heap profiler called `massif`

- a cache utilization profiler called `cachegrind`

- a call-graph performance profiler called `callgrind`

<!--
TODO: explain valgrind's dynamic binary translation, warn about massive
slowdown, talk about `kcachegrind` for a GUI
-->
