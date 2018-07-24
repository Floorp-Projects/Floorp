/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case)]

extern crate fnv;
extern crate fxhash;

use fnv::FnvHashSet;
use fxhash::FxHashSet;
use std::collections::HashSet;
use std::os::raw::{c_char, c_void};
use std::slice;

/// Keep this in sync with Params in Bench.cpp.
#[derive(Debug)]
#[repr(C)]
pub struct Params
{
  config_name: *const c_char,
  num_inserts: usize,
  num_successful_lookups: usize,
  num_failing_lookups: usize,
  num_iterations: usize,
  remove_inserts: bool,
}

#[no_mangle]
pub extern "C" fn Bench_Rust_HashSet(params: *const Params, vals: *const *const c_void,
                                     len: usize) {
    let hs: HashSet<_> = std::collections::HashSet::default();
    Bench_Rust(hs, params, vals, len);
}

#[no_mangle]
pub extern "C" fn Bench_Rust_FnvHashSet(params: *const Params, vals: *const *const c_void,
                                        len: usize) {
    let hs = FnvHashSet::default();
    Bench_Rust(hs, params, vals, len);
}

#[no_mangle]
pub extern "C" fn Bench_Rust_FxHashSet(params: *const Params, vals: *const *const c_void,
                                       len: usize) {
    let hs = FxHashSet::default();
    Bench_Rust(hs, params, vals, len);
}

// Keep this in sync with all the other Bench_*() functions.
fn Bench_Rust<H: std::hash::BuildHasher>(mut hs: HashSet<*const c_void, H>, params: *const Params,
                                         vals: *const *const c_void, len: usize) {
    let params = unsafe { &*params };
    let vals = unsafe { slice::from_raw_parts(vals, len) };

    for j in 0..params.num_inserts {
        hs.insert(vals[j]);
    }

    for _i in 0..params.num_successful_lookups {
        for j in 0..params.num_inserts {
            assert!(hs.contains(&vals[j]));
        }
    }

    for _i in 0..params.num_failing_lookups {
        for j in params.num_inserts..params.num_inserts*2 {
            assert!(!hs.contains(&vals[j]));
        }
    }

    for _i in 0..params.num_iterations {
      let mut n = 0;
      for _ in hs.iter() {
        n += 1;
      }
      assert!(params.num_inserts == n);
      assert!(hs.len() == n);
    }

    if params.remove_inserts {
        for j in 0..params.num_inserts {
            assert!(hs.remove(&vals[j]));
        }
        assert!(hs.len() == 0);
    } else {
        assert!(hs.len() == params.num_inserts);
    }
}

