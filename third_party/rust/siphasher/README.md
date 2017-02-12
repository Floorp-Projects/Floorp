SipHash implementation for Rust
===============================

SipHash was recently removed from rust-core.

This crate brings `SipHasher`, `SipHasher13` and `SipHash24` back.
It is based on the original implementation from rust-core and exposes the
same API.

In addition, it can return 128-bit tags.

The `sip` module implements the standard 64-bit mode, whereas the `sip128`
module implements the experimental 128-bit mode.

Usage
-----
In `Cargo.toml`:

```toml
[dependencies]
siphasher = "~0.1"
```

64-bit mode:
```rust
extern crate siphasher;

use siphasher::sip::{SipHasher, SipHasher13, SipHasher24};
```

128-bit mode:
```rust
use siphasher::sip128::{Hasher128, Siphasher, SipHasher13, SipHasher24};
```

[API documentation](https://docs.rs/siphasher/)
-----------------------------------------------
