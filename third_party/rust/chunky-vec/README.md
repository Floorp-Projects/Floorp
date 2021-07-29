# Chunky Vec

[![License: MIT/Apache-2.0](https://img.shields.io/crates/l/chunky-vec.svg)](#license)

This crate provides a pin-safe, append-only vector which guarantees never
to move the storage for an element once it has been allocated.
