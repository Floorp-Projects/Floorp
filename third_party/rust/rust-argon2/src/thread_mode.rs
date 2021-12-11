// Copyright (c) 2017 Xidorn Quan <me@upsuper.org>
// Copyright (c) 2017 Martijn Rijkeboer <mrr@sru-systems.com>
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/// The thread mode used to perform the hashing.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ThreadMode {
    /// Run in one thread.
    Sequential,

    /// Run in the same number of threads as the number of lanes.
    Parallel,
}

impl ThreadMode {
    /// Create a thread mode from the threads count.
    pub fn from_threads(threads: u32) -> ThreadMode {
        if threads > 1 {
            ThreadMode::Parallel
        } else {
            ThreadMode::Sequential
        }
    }
}

impl Default for ThreadMode {
    fn default() -> ThreadMode {
        ThreadMode::Sequential
    }
}


#[cfg(test)]
mod tests {

    use super::*;

    #[test]
    fn default_returns_correct_thread_mode() {
        assert_eq!(ThreadMode::default(), ThreadMode::Sequential);
    }

    #[test]
    fn from_threads_returns_correct_thread_mode() {
        assert_eq!(ThreadMode::from_threads(0), ThreadMode::Sequential);
        assert_eq!(ThreadMode::from_threads(1), ThreadMode::Sequential);
        assert_eq!(ThreadMode::from_threads(2), ThreadMode::Parallel);
        assert_eq!(ThreadMode::from_threads(10), ThreadMode::Parallel);
        assert_eq!(ThreadMode::from_threads(100), ThreadMode::Parallel);
    }
}
