# 0.2.0

- `encode_byte` now returns `[u8; 2]` instead of `(u8, u8)`, as in practice this
  tends to be more convenient.

- The use of `std` which requires the `alloc` trait has been split into the
  `alloc` feature.

- `base16` has been relicensed as CC0-1.0 from dual MIT/Apache-2.0.

# 0.2.1

- Make code more bulletproof in the case of panics when using the `decode_buf`
  or `encode_config_buf` functions.

    Previously, if the innermost encode/decode function paniced, code that
    inspected the contents of the Vec (either in a Drop implementation, or by
    catching the panic) could read from uninitialized memory.

    However, I don't believe it is possible for the innermost encode/decode
    functions to panic, so I don't think there was any risk in previous
    versions.

    I don't believe the panic is possible because only two panics exist in the generated assembly (both only in debug configuration, and not in release). The two panics are respectively:

    - a debug_assert verifying the caller performed a check (which it does).

    - a usize overflow check on an index variable, which is impossible as we've
      already tested for that.

    That said, this is some powerful rationalization, so I'm cutting a new version
    with this fix anyway.

- Additionally, several functions that previously used unsafe internally now
  either use less unsafe, or are entirely safe.
