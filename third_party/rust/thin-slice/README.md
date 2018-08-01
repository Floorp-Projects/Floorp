# thin-slice

An owned slice that packs the slice storage into a single word when possible.

## Usage

```rust
extern crate thin_slice;

use std::mem;
use thin_slice::ThinBoxedSlice;

struct Ticket {
    numbers: Box<[u8]>,
    winning: bool,
}

struct ThinTicket {
    numbers: ThinBoxedSlice<u8>,
    winning: bool,
}

fn main() {
    let nums = vec![4, 8, 15, 16, 23, 42].into_boxed_slice();
    let ticket = ThinTicket {
        numbers: nums.into(),
        winning: false,
    };
    println!("Numbers: {:?}", ticket.numbers);

    println!("size_of::<usize>():      {}", mem::size_of::<usize>());
    println!("size_of::<Ticket>():     {}", mem::size_of::<Ticket>());
    println!("size_of::<ThinTicket>(): {}", mem::size_of::<ThinTicket>());
}
```

Output on `x86_64`:

```
Numbers: [4, 8, 15, 16, 23, 42]
size_of::<usize>():      8
size_of::<Ticket>():     24
size_of::<ThinTicket>(): 16
```

## License

thin-slice is distributed under the terms of the
[Mozilla Public License, v. 2.0](https://www.mozilla.org/MPL/2.0/).
