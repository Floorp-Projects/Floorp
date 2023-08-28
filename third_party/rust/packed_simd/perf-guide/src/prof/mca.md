# Machine code analysis tools

## The microarchitecture of modern CPUs

While you might have heard of Instruction Set Architectures, such as `x86` or
`arm` or `mips`, the term _microarchitecture_ (also written here as _µ-arch_),
refers to the internal details of an actual family of CPUs, such as Intel's
_Haswell_ or AMD's _Jaguar_.

Replacing scalar code with SIMD code will improve performance on all CPUs
supporting the required vector extensions.
However, due to microarchitectural differences, the actual speed-up at
runtime might vary.

**Example**: a simple example arises when optimizing for AMD K8 CPUs.
The assembly generated for an empty function should look like this:

```asm
nop
ret
```

The `nop` is used to align the `ret` instruction for better performance.
However, the compiler will actually generated the following code:

```asm
repz ret
```

The `repz` instruction will repeat the following instruction until a certain
condition. Of course, in this situation, the function will simply immediately
return, and the `ret` instruction is still aligned.
However, AMD K8's branch predictor performs better with the latter code.

For those looking to absolutely maximize performance for a certain target µ-arch,
you will have to read some CPU manuals, or ask the compiler to do it for you
with `-C target-cpu`.

### Summary of CPU internals

Modern processors are able to execute instructions out-of-order for better performance,
by utilizing tricks such as [branch prediction], [instruction pipelining],
or [superscalar execution].

[branch prediction]: https://en.wikipedia.org/wiki/Branch_predictor
[instruction pipelining]: https://en.wikipedia.org/wiki/Instruction_pipelining
[superscalar execution]: https://en.wikipedia.org/wiki/Superscalar_processor

SIMD instructions are also subject to these optimizations, meaning it can get pretty
difficult to determine where the slowdown happens.
For example, if the profiler reports a store operation is slow, one of two things
could be happening:

- the store is limited by the CPU's memory bandwidth, which is actually an ideal
  scenario, all things considered;

- memory bandwidth is nowhere near its peak, but the value to be stored is at the
  end of a long chain of operations, and this store is where the profiler
  encountered the pipeline stall;

Since most profilers are simple tools which don't understand the subtleties of
instruction scheduling, you

## Analyzing the machine code

Certain tools have knowledge of internal CPU microarchitecture, i.e. they know

- how many physical [register files] a CPU actually has

- what is the latency / throughtput of an instruction

- what [µ-ops] are generated for a set of instructions

and many other architectural details.

[register files]: https://en.wikipedia.org/wiki/Register_file
[µ-ops]: https://en.wikipedia.org/wiki/Micro-operation

These tools are therefore able to provide accurate information as to why some
instructions are inefficient, and where the bottleneck is.

The disadvantage is that the output of these tools requires advanced knowledge
of the target architecture to understand, i.e. they **cannot** point out what
the cause of the issue is explicitly.

## Intel's Architecture Code Analyzer (IACA)

[IACA] is a free tool offered by Intel for analyzing the performance of various
computational kernels.

Being a proprietary, closed source tool, it _only_ supports Intel's µ-arches.

[IACA]: https://software.intel.com/en-us/articles/intel-architecture-code-analyzer

## llvm-mca

<!--
TODO: once LLVM 7 gets released, write a chapter on using llvm-mca
with SIMD disassembly.
-->
