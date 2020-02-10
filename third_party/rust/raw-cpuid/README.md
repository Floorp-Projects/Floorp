# cpuid [![Build Status](https://travis-ci.org/gz/rust-cpuid.svg)](https://travis-ci.org/gz/rust-cpuid) [![Crates.io](https://img.shields.io/crates/v/raw_cpuid.svg)](https://crates.io/crates/raw-cpuid)

A library to parse the x86 CPUID instruction, written in rust with no external dependencies. The implementation closely resembles the Intel CPUID manual description. The library does only depend on libcore.

The code should be in sync with the latest March 2018 revision of the Intel Architectures Software Developer’s Manual.

## Library usage
```rust
let cpuid = CpuId::new();

match cpuid.get_vendor_info() {
    Some(vf) => assert!(vf.as_string() == "GenuineIntel"),
    None => ()
}

let has_sse = match cpuid.get_feature_info() {
    Some(finfo) => finfo.has_sse(),
    None => false
};

if has_sse {
    println!("CPU supports SSE!");
}

match cpuid.get_cache_parameters() {
    Some(cparams) => {
        for cache in cparams {
            let size = cache.associativity() * cache.physical_line_partitions() * cache.coherency_line_size() * cache.sets();
            println!("L{}-Cache size is {}", cache.level(), size);
        }
    },
    None => println!("No cache parameter information available"),
}
```

## Documentation
* [API Documentation](https://docs.rs/raw-cpuid/)
* [Examples](https://github.com/gz/rust-cpuid/tree/master/examples)
