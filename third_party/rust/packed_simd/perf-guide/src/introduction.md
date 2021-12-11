# Introduction

## What is SIMD

<!-- TODO:
describe what SIMD is, which algorithms can benefit from it,
give usage examples
-->

## History of SIMD in Rust

<!-- TODO:
discuss history of unstable std::simd,
stabilization of std::arch, etc.
-->

## Discover packed_simd

<!-- TODO: describe scope of this project -->

Writing fast and portable SIMD algorithms using `packed_simd` is, unfortunately,
not trivial. There are many pitfals that one should be aware of, and some idioms
that help avoid those pitfalls.

This book attempts to document these best practices and provides practical examples
on how to apply the tips to _your_ code.
