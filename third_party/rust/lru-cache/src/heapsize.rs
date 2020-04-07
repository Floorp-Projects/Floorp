extern crate heapsize;

use self::heapsize::HeapSizeOf;
use std::hash::{Hash, BuildHasher};

use LruCache;

impl<K: Eq + Hash + HeapSizeOf, V: HeapSizeOf, S: BuildHasher> HeapSizeOf for LruCache<K, V, S> {
    fn heap_size_of_children(&self) -> usize {
        self.map.heap_size_of_children()
    }
}
