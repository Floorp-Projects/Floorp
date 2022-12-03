// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use ::rand::{thread_rng, RngCore};

#[must_use]
pub fn random(size: usize) -> Vec<u8> {
    let mut rng = thread_rng();
    let mut buf = vec![0; size];
    rng.fill_bytes(&mut buf);
    buf
}
