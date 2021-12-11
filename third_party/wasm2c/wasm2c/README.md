# wasm2c: Convert wasm files to C source and header

`wasm2c` takes a WebAssembly module and produces an equivalent C source and
header. Some examples:

```sh
# parse binary file test.wasm and write test.c and test.h
$ wasm2c test.wasm -o test.c

# parse test.wasm, write test.c and test.h, but ignore the debug names, if any
$ wasm2c test.wasm --no-debug-names -o test.c
```

## Tutorial: .wat -> .wasm -> .c

Let's look at a simple example of a factorial function.

```wasm
(func (export "fac") (param $x i32) (result i32)
  (if (result i32) (i32.eq (get_local $x) (i32.const 0))
    (then (i32.const 1))
    (else
      (i32.mul (get_local $x) (call 0 (i32.sub (get_local $x) (i32.const 1))))
    )
  )
)
```

Save this to `fac.wat`. We can convert this to a `.wasm` file by using the
`wat2wasm` tool:

```sh
$ wat2wasm fac.wat -o fac.wasm
```

We can then convert it to a C source and header by using the `wasm2c` tool:

```sh
$ wasm2c fac.wasm -o fac.c
```

This generates two files, `fac.c` and `fac.h`. We'll take a closer look at
these files below, but first let's show a simple example of how to use these
files.

## Using the generated module

To actually use our fac module, we'll use create a new file, `main.c`, that
include `fac.h`, initializes the module, and calls `fac`.

`wasm2c` generates a few symbols for us, `init` and `Z_facZ_ii`. `init`
initializes the module, and `Z_facZ_ii` is our exported `fac` function, but
[name-mangled](https://en.wikipedia.org/wiki/Name_mangling) to include the
function signature.

We can define `WASM_RT_MODULE_PREFIX` before including `fac.h` to generate
these symbols with a prefix, in case we already have a symbol called `init` (or
even `Z_facZ_ii`!) Note that you'll have to compile `fac.c` with this macro
too, for this to work.

```c
#include <stdio.h>
#include <stdlib.h>

/* Uncomment this to define fac_init and fac_Z_facZ_ii instead. */
/* #define WASM_RT_MODULE_PREFIX fac_ */

#include "fac.h"

int main(int argc, char** argv) {
  /* Make sure there is at least one command-line argument. */
  if (argc < 2) return 1;

  /* Convert the argument from a string to an int. We'll implicitly cast the int
  to a `u32`, which is what `fac` expects. */
  u32 x = atoi(argv[1]);

  /* Initialize the fac module. Since we didn't define WASM_RT_MODULE_PREFIX,
  the initialization function is called `init`. */
  init();

  /* Call `fac`, using the mangled name. */
  u32 result = Z_facZ_ii(x);

  /* Print the result. */
  printf("fac(%u) -> %u\n", x, result);

  return 0;
}
```

To compile the executable, we need to use `main.c` and the generated `fac.c`.
We'll also include `wasm-rt-impl.c` which has implementations of the various
`wasm_rt_*` functions used by `fac.c` and `fac.h`.

```sh
$ cc -o fac main.c fac.c wasm-rt-impl.c
```

Now let's test it out!

```sh
$ ./fac 1
fac(1) -> 1
$ ./fac 5
fac(5) -> 120
$ ./fac 10
fac(10) -> 3628800
```

You can take a look at the all of these files in
[wasm2c/examples/fac](/wasm2c/examples/fac).

## Looking at the generated header, `fac.h`

The generated header file looks something like this:

```c
#ifndef FAC_H_GENERATED_
#define FAC_H_GENERATED_
#ifdef __cplusplus
extern "C" {
#endif

#ifndef WASM_RT_INCLUDED_
#define WASM_RT_INCLUDED_

...

#endif  /* WASM_RT_INCLUDED_ */

extern void WASM_RT_ADD_PREFIX(init)(void);

/* export: 'fac' */
extern u32 (*WASM_RT_ADD_PREFIX(Z_facZ_ii))(u32);
#ifdef __cplusplus
}
#endif

#endif  /* FAC_H_GENERATED_ */
```

Let's look at each section. The outer `#ifndef` is standard C boilerplate for a
header. The `extern "C"` part makes sure to not mangle the symbols if using
this header in C++.

```c
#ifndef FAC_H_GENERATED_
#define FAC_H_GENERATED_
#ifdef __cplusplus
extern "C" {
#endif

...

#ifdef __cplusplus
}
#endif
#endif  /* FAC_H_GENERATED_ */
```

This `WASM_RT_INCLUDED_` section contains a number of definitions required for
all WebAssembly modules.

```c
#ifndef WASM_RT_INCLUDED_
#define WASM_RT_INCLUDED_

...

#endif  /* WASM_RT_INCLUDED_ */
```

First we can specify the maximum call depth before trapping. This defaults to
500:

```c
#ifndef WASM_RT_MAX_CALL_STACK_DEPTH
#define WASM_RT_MAX_CALL_STACK_DEPTH 500
#endif
```

Next we can specify a module prefix. This is useful if you are using multiple
modules that may use the same name as an export. Since we only have one module
here, it's fine to use the default which is an empty prefix:

```c
#ifndef WASM_RT_MODULE_PREFIX
#define WASM_RT_MODULE_PREFIX
#endif

#define WASM_RT_PASTE_(x, y) x ## y
#define WASM_RT_PASTE(x, y) WASM_RT_PASTE_(x, y)
#define WASM_RT_ADD_PREFIX(x) WASM_RT_PASTE(WASM_RT_MODULE_PREFIX, x)
```

Next are some convenient typedefs for integers and floats of fixed sizes:

```c
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;
typedef double f64;
```

Next is the `wasm_rt_trap_t` enum, which is used to give the reason a trap
occurred.

```c
typedef enum {
  WASM_RT_TRAP_NONE,
  WASM_RT_TRAP_OOB,
  WASM_RT_TRAP_INT_OVERFLOW,
  WASM_RT_TRAP_DIV_BY_ZERO,
  WASM_RT_TRAP_INVALID_CONVERSION,
  WASM_RT_TRAP_UNREACHABLE,
  WASM_RT_TRAP_CALL_INDIRECT_TABLE_EXPANSION,
  WASM_RT_TRAP_EXHAUSTION,
} wasm_rt_trap_t;
```

Next is the `wasm_rt_type_t` enum, which is used for specifying function
signatures. The four WebAssembly value types are included:

```c
typedef enum {
  WASM_RT_I32,
  WASM_RT_I64,
  WASM_RT_F32,
  WASM_RT_F64,
} wasm_rt_type_t;
```

Next is `wasm_rt_anyfunc_t`, the function signature for a generic function
callback. Since a WebAssembly table can contain functions of any given
signature, it is necessary to convert them to a canonical form:

```c
typedef void (*wasm_rt_anyfunc_t)(void);
```

Next are the definitions for a table element. `func_type` is a function index
as returned by `wasm_rt_register_func_type` described below.

```c
typedef struct {
  uint32_t func_type;
  wasm_rt_anyfunc_t func;
} wasm_rt_elem_t;
```

Next is the definition of a memory instance. The `data` field is a pointer to
`size` bytes of linear memory. The `size` field of `wasm_rt_memory_t` is the
current size of the memory instance in bytes, whereas `pages` is the current
size in pages (65536 bytes.) `max_pages` is the maximum number of pages as
specified by the module, or `0xffffffff` if there is no limit.

```c
typedef struct {
  uint8_t* data;
  uint32_t pages, max_pages;
  uint32_t size;
} wasm_rt_memory_t;
```

Next is the definition of a table instance. The `data` field is a pointer to
`size` elements. Like a memory instance, `size` is the current size of a table,
and `max_size` is the maximum size of the table, or `0xffffffff` if there is no
limit.

```c
typedef struct {
  wasm_rt_elem_t* data;
  uint32_t max_size;
  uint32_t size;
} wasm_rt_table_t;
```

## Symbols that must be defined by the embedder

Next in `fac.h` are a collection of extern symbols that must be implemented by
the embedder (i.e. you) before this C source can be used.

A C implementation of these functions is defined in
[`wasm-rt-impl.h`](wasm-rt-impl.h) and [`wasm-rt-impl.c`](wasm-rt-impl.c).

```c
extern void wasm_rt_trap(wasm_rt_trap_t) __attribute__((noreturn));
extern uint32_t wasm_rt_register_func_type(uint32_t params, uint32_t results, ...);
extern void wasm_rt_allocate_memory(wasm_rt_memory_t*, uint32_t initial_pages, uint32_t max_pages);
extern uint32_t wasm_rt_grow_memory(wasm_rt_memory_t*, uint32_t pages);
extern void wasm_rt_allocate_table(wasm_rt_table_t*, uint32_t elements, uint32_t max_elements);
extern uint32_t wasm_rt_call_stack_depth;
```

`wasm_rt_trap` is a function that is called when the module traps. Some
possible implementations are to throw a C++ exception, or to just abort the
program execution.

`wasm_rt_register_func_type` is a function that registers a function type. It
is a variadic function where the first two arguments give the number of
parameters and results, and the following arguments are the types. For example,
the function `func (param i32 f32) (result f64)` would register the function
type as
`wasm_rt_register_func_type(2, 1, WASM_RT_I32, WASM_RT_F32, WASM_RT_F64)`.

`wasm_rt_allocate_memory` initializes a memory instance, and allocates at least
enough space for the given number of initial pages. The memory must be cleared
to zero.

`wasm_rt_grow_memory` must grow the given memory instance by the given number
of pages. If there isn't enough memory to do so, or the new page count would be
greater than the maximum page count, the function must fail by returning
`0xffffffff`. If the function succeeds, it must return the previous size of the
memory instance, in pages.

`wasm_rt_allocate_table` initializes a table instance, and allocates at least
enough space for the given number of initial elements. The elements must be
cleared to zero.

`wasm_rt_call_stack_depth` is the current stack call depth. Since this is
shared between modules, it must be defined only once, by the embedder.

## Exported symbols

Finally, `fac.h` defines exported symbols provided by the module. In our
example, the only function we exported was `fac`. An additional function is
provided called `init`, which initializes the module and must be called before
the module can be used:

```c
extern void WASM_RT_ADD_PREFIX(init)(void);

/* export: 'fac' */
extern u32 (*WASM_RT_ADD_PREFIX(Z_facZ_ii))(u32);
```

All exported names use `WASM_RT_ADD_PREFIX` (as described above) to allow the
symbols to placed in a namespace as decided by the embedder. All symbols are
also mangled so they include the types of the function signature.

In our example, `Z_facZ_ii` is the mangling for a function named `fac` that
takes one `i32` parameter and returns one `i32` result.

## A quick look at `fac.c`

The contents of `fac.c` are internals, but it is useful to see a little about
how it works.

The first few hundred lines define macros that are used to implement the
various WebAssembly instructions. Their implementations may be interesting to
the curious reader, but are out of scope for this document.

Following those definitions are various initialization functions (`init`,
`init_func_types`, `init_globals`, `init_memory`, `init_table`, and
`init_exports`.) In our example, most of these functions are empty, since the
module doesn't use any globals, memory or tables.

The most interesting part is the definition of the function `fac`:

```c
static u32 fac(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = p0;
  i1 = 0u;
  i0 = i0 == i1;
  if (i0) {
    i0 = 1u;
  } else {
    i0 = p0;
    i1 = p0;
    i2 = 1u;
    i1 -= i2;
    i1 = fac(i1);
    i0 *= i1;
  }
  FUNC_EPILOGUE;
  return i0;
}
```

If you look at the original WebAssembly text in the flat format, you can see
that there is a 1-1 mapping in the output:

```wasm
(func $fac (param $x i32) (result i32)
  get_local $x
  i32.const 0
  i32.eq
  if (result i32)
    i32.const 1
  else
    get_local $x
    get_local $x
    i32.const 1
    i32.sub
    call 0
    i32.mul
  end)
```

This looks different than the factorial function above because it is using the
"flat format" instead of the "folded format". You can use `wat-desugar` to
convert between the two to be sure:

```sh
$ wat-desugar fac-flat.wat --fold -o fac-folded.wat
```

```wasm
(module
  (func (;0;) (param i32) (result i32)
    (if (result i32)  ;; label = @1
      (i32.eq
        (get_local 0)
        (i32.const 0))
      (then
        (i32.const 1))
      (else
        (i32.mul
          (get_local 0)
          (call 0
            (i32.sub
              (get_local 0)
              (i32.const 1)))))))
  (export "fac" (func 0))
  (type (;0;) (func (param i32) (result i32))))
```

The formatting is different and the variable and function names are gone, but
the structure is the same.
