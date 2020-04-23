/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Reexports of hashbrown, without and with FxHash

use fxhash;

pub use hashbrown::{hash_map as map, HashMap, HashSet};

/// Hash map that uses the Fx hasher
pub type FxHashMap<K, V> = HashMap<K, V, fxhash::FxBuildHasher>;
/// Hash set that uses the Fx hasher
pub type FxHashSet<T> = HashSet<T, fxhash::FxBuildHasher>;
