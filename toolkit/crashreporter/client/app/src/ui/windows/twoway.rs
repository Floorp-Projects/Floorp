/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;

/// A two-way hashmap.
#[derive(Debug)]
pub struct TwoWay<K, V> {
    forward: HashMap<K, V>,
    reverse: HashMap<V, K>,
}

impl<K, V> Default for TwoWay<K, V> {
    fn default() -> Self {
        TwoWay {
            forward: Default::default(),
            reverse: Default::default(),
        }
    }
}

impl<K: Eq + std::hash::Hash + Clone, V: Eq + std::hash::Hash + Clone> TwoWay<K, V> {
    pub fn insert(&mut self, key: K, value: V) {
        self.forward.insert(key.clone(), value.clone());
        self.reverse.insert(value, key);
    }

    pub fn forward(&self) -> &HashMap<K, V> {
        &self.forward
    }

    pub fn reverse(&self) -> &HashMap<V, K> {
        &self.reverse
    }
}
