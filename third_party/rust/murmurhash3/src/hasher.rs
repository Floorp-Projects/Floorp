use std::hash::Hasher;
use std::collections::hash_state::HashState;

use mmh3_32::murmurhash3_x86_32;

pub struct Murmur3Hasher {
    seed: u32,
    bytes: Vec<u8>,
}

#[derive(Clone, Copy)]
pub struct Murmur3HashState {
    seed: u32,
}

impl Murmur3HashState {
    pub fn new() -> Murmur3HashState {
        return Murmur3HashState { seed: 0 };
    }

    pub fn with_seed(seed: u32) -> Murmur3HashState {
        return Murmur3HashState { seed: seed };
    }
}


impl Hasher for Murmur3Hasher {
    fn finish(&self) -> u64 {
        return murmurhash3_x86_32(&self.bytes, self.seed) as u64;
    }

    fn write(&mut self, bytes: &[u8]) {
        self.bytes.push_all(bytes);
    }
}

impl HashState for Murmur3HashState {
    type Hasher = Murmur3Hasher;

    fn hasher(&self) -> Murmur3Hasher {
        return Murmur3Hasher { seed: self.seed, bytes: vec![] };
    }
}

#[cfg(test)]
mod test {
    use super::Murmur3HashState;
    use std::collections::hash_map::HashMap;

    #[test]
    fn use_in_hashmap() {
        let mut hashmap = HashMap::with_hash_state(Murmur3HashState::new());
        hashmap.insert("one", 1);
        hashmap.insert("two", 2);

        assert!(hashmap.len() == 2);

        assert!(*hashmap.get("one").unwrap() == 1);
        assert!(*hashmap.get("two").unwrap() == 2);
    }
}
