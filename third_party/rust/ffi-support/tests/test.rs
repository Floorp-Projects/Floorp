/* Copyright 2018-2019 Mozilla Foundation
 *
 * Licensed under the Apache License (Version 2.0), or the MIT license,
 * (the "Licenses") at your option. You may not use this file except in
 * compliance with one of the Licenses. You may obtain copies of the
 * Licenses at:
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *    http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Licenses is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the Licenses for the specific language governing permissions and
 * limitations under the Licenses.
 */

//! This test is a stress test meant to trigger some bugs seen prior to the use
//! of handlemaps. See /docs/design/test-faster.md for why it's split -- TLDR:
//! it uses rayon and is hard to rewrite with normal threads.

use ffi_support::{ConcurrentHandleMap, ExternError};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

fn with_error<F: FnOnce(&mut ExternError) -> T, T>(callback: F) -> T {
    let mut e = ExternError::success();
    let result = callback(&mut e);
    if let Some(m) = unsafe { e.get_and_consume_message() } {
        panic!("unexpected error: {}", m);
    }
    result
}

struct DropChecking {
    counter: Arc<AtomicUsize>,
    id: usize,
}
impl Drop for DropChecking {
    fn drop(&mut self) {
        let val = self.counter.fetch_add(1, Ordering::SeqCst);
        log::debug!("Dropped {} :: {}", self.id, val);
    }
}
#[test]
fn test_concurrent_drop() {
    use rand::prelude::*;
    use rayon::prelude::*;
    let _ = env_logger::try_init();
    let drop_counter = Arc::new(AtomicUsize::new(0));
    let id = Arc::new(AtomicUsize::new(1));
    let map = ConcurrentHandleMap::new();
    let count = 1000;
    let mut handles = (0..count)
        .into_par_iter()
        .map(|_| {
            let id = id.fetch_add(1, Ordering::SeqCst);
            let handle = with_error(|e| {
                map.insert_with_output(e, || {
                    log::debug!("Created {}", id);
                    DropChecking {
                        counter: drop_counter.clone(),
                        id,
                    }
                })
            });
            (id, handle)
        })
        .collect::<Vec<_>>();

    handles.shuffle(&mut thread_rng());

    assert_eq!(drop_counter.load(Ordering::SeqCst), 0);
    handles.par_iter().for_each(|(id, h)| {
        with_error(|e| {
            map.call_with_output(e, *h, |val| {
                assert_eq!(val.id, *id);
            })
        });
    });

    assert_eq!(drop_counter.load(Ordering::SeqCst), 0);

    handles.par_iter().for_each(|(id, h)| {
        with_error(|e| {
            map.call_with_output(e, *h, |val| {
                assert_eq!(val.id, *id);
            })
        });
    });

    handles.par_iter().for_each(|(id, h)| {
        let item = map
            .remove_u64(*h)
            .expect("remove to succeed")
            .expect("item to exist");
        assert_eq!(item.id, *id);
        let h = map.insert(item).into_u64();
        map.delete_u64(h).expect("delete to succeed");
    });
    assert_eq!(drop_counter.load(Ordering::SeqCst), count);
}
