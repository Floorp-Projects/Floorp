// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::sync::Once;

#[allow(clippy::module_name_repetitions)]
pub struct OnceResult<T> {
    once: Once,
    v: Option<T>,
}

impl<T> OnceResult<T> {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            once: Once::new(),
            v: None,
        }
    }

    pub fn call_once<F: FnOnce() -> T>(&mut self, f: F) -> &T {
        let v = &mut self.v;
        self.once.call_once(|| {
            *v = Some(f());
        });
        self.v.as_ref().unwrap()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    static mut STATIC_ONCE_RESULT: OnceResult<u64> = OnceResult::new();

    #[test]
    fn static_update() {
        assert_eq!(*unsafe { STATIC_ONCE_RESULT.call_once(|| 23) }, 23);
        assert_eq!(*unsafe { STATIC_ONCE_RESULT.call_once(|| 24) }, 23);
    }
}
