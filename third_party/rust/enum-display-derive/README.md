# Derive Display trait for enums

This crate can derive a `Display` implementation for very simple enums,
like the following one:

```rust
#[macro_use]
extern crate enum_display_derive;

use std::fmt::Display;

#[derive(Display)]
enum FizzBuzz {
   Fizz,
   Buzz,
   FizzBuzz,
   Number(u64),
}

fn fb(i: u64) {
   match (i % 3, i % 5) {
       (0, 0) => FizzBuzz::FizzBuzz,
       (0, _) => FizzBuzz::Fizz,
       (_, 0) => FizzBuzz::Buzz,
       (_, _) => FizzBuzz::Number(i),
   }
}

fn main() {
   for i in 0..100 {
       println!("{}", fb(i));
   }
}
```

## License

Licensed under either of
 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)
at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any
additional terms or conditions.`
