# bit_reverse
[![Crates Shield](https://img.shields.io/crates/v/bit_reverse.svg "Crates.io")](https://crates.io/crates/bit_reverse) [![Build Shield](https://travis-ci.org/EugeneGonzalez/bit_reverse.svg?branch=master "TravisCI")](https://travis-ci.org/EugeneGonzalez/bit_reverse) [![Build status](https://ci.appveyor.com/api/projects/status/hkj3312s9v7rhw3p/branch/master?svg=true)](https://ci.appveyor.com/project/EugeneGonzalez/bit-reverse/branch/master)


### Library Objective
This library provides a number of ways to compute the bit reversal of all primitive integers.
There are currently 3 different algorithms implemented: Bitwise, Parallel, and Lookup reversal.

### Example
```rust
use bit_reverse::ParallelReverse;

assert_eq!(0xA0u8.swap_bits(), 0x05u8);
```
This library is very simple to uses just import the crate and the algorithm you want to use.
Then you can call `swap_bits`() on any primitive integer. If you want to try a different
algorithm just change the use statement and now your program will use the algorithm instead.

### YMMV Performance Comparison
I wouldn't use `BitwiseReverse` as it is mainly there for completeness and is strictly inferior
to `ParallelReverse`, which is a Bitwise Parallel Reverse and thus an order of magnitude faster.
For small sizes, <= 16 bits, `LookupReverse` is the fastest but it doesn't scale as well as 
`ParallelReverse` this is because `ParallelReverse` does a constant number of operations for
every size (assuming your cpu has a hardware byte swap instruction). `LookupReverse` needs more
lookups, ANDs, and ORs for each size increase. Thus `ParallelReverse` performs a little better
at 32 bits and much better at 64 bits. These runtime characteristics are based on a Intel(R)
Core(TM) i7-4770K CPU @ 3.50GHz.

### Memory Consumption
`BitwiseReverse` and `ParallelReverse` both only use a couple of stack variables for their
computations. `LookupReverse` on the other hand statically allocates 256 u8s or 256 bytes to
do its computations. `LookupReverse`'s memory cost is shared by all of the types 
'LookupReverse` supports.

### no_std Compatible
To link to core instead of STD, disable default features for this library in your Cargo.toml.
[Cargo choosing features](http://doc.crates.io/specifying-dependencies.html#choosing-features)
