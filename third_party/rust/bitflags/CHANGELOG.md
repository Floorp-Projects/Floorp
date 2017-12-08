# 1.0.0
- **[breaking change]** Macro now generates [associated constants](https://doc.rust-lang.org/reference/items.html#associated-constants) (#24)

- **[breaking change]** Minimum supported version is Rust **1.20**, due to usage of associated constants

- After being broken in 0.9, the `#[deprecated]` attribute is now supported again (#112)

- Other improvements to unit tests and documentation (#106 and #115)

## How to update your code to use associated constants
Assuming the following structure definition:
```rust
bitflags! {
  struct Something: u8 {
     const FOO = 0b01,
     const BAR = 0b10
  }
}
```
In 0.9 and older you could do:
```rust
let x = FOO.bits | BAR.bits;
```
Now you must use:
```rust
let x = Something::FOO.bits | Something::BAR.bits;
```

# 0.9.1
- Fix the implementation of `Formatting` traits when other formatting traits were present in scope (#105)

# 0.9.0
- **[breaking change]** Use struct keyword instead of flags to define bitflag types (#84)

- **[breaking change]** Terminate const items with semicolons instead of commas (#87)

- Implement the `Hex`, `Octal`, and `Binary` formatting traits (#86)

- Printing an empty flag value with the `Debug` trait now prints "(empty)" instead of nothing (#85)

- The `bitflags!` macro can now be used inside of a fn body, to define a type local to that function (#74)

# 0.8.2
- Update feature flag used when building bitflags as a dependency of the Rust toolchain

# 0.8.1
- Allow bitflags to be used as a dependency of the Rust toolchain

# 0.8.0
- Add support for the experimental `i128` and `u128` integer types (#57)
- Add set method: `flags.set(SOME_FLAG, true)` or `flags.set(SOME_FLAG, false)` (#55)
  This may break code that defines its own set method

# 0.7.1
*(yanked)*

# 0.7.0
- Implement the Extend trait (#49)
- Allow definitions inside the `bitflags!` macro to refer to items imported from other modules (#51)

# 0.6.0
- The `no_std` feature was removed as it is now the default
- The `assignment_operators` feature was remove as it is now enabled by default
- Some clippy suggestions have been applied
