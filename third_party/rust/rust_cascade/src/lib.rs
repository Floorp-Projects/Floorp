extern crate bit_reverse;
extern crate bit_vec;
extern crate byteorder;
extern crate digest;
extern crate murmurhash3;
extern crate rand;

use bit_reverse::ParallelReverse;
use bit_vec::BitVec;
use byteorder::ReadBytesExt;
use murmurhash3::murmurhash3_x86_32;

use std::io::{Error, ErrorKind, Read};

#[derive(Debug)]
pub struct Bloom {
    level: u32,
    n_hash_funcs: u32,
    size: usize,
    bitvec: BitVec,
}

pub fn calculate_n_hash_funcs(error_rate: f32) -> u32 {
    ((1.0 / error_rate).ln() / (2.0_f32).ln()).ceil() as u32
}

pub fn calculate_size(elements: usize, error_rate: f32) -> usize {
    let n_hash_funcs = calculate_n_hash_funcs(error_rate);
    let hashes = n_hash_funcs as f32;
    (1.0_f32
        - (hashes * (elements as f32 + 0.5) / (1.0_f32 - error_rate.powf(1.0 / hashes)).ln()))
    .ceil() as usize
}

impl Bloom {
    pub fn new(size: usize, n_hash_funcs: u32, level: u32) -> Bloom {
        let bitvec: BitVec = BitVec::from_elem(size, false);

        Bloom {
            level: level,
            n_hash_funcs: n_hash_funcs,
            size: size,
            bitvec: bitvec,
        }
    }

    pub fn from_bytes(cursor: &mut &[u8]) -> Result<Bloom, Error> {
        // Load the layer metadata. bloomer.py writes size, nHashFuncs and level as little-endian
        // unsigned ints.
        let size = cursor.read_u32::<byteorder::LittleEndian>()? as usize;
        let n_hash_funcs = cursor.read_u32::<byteorder::LittleEndian>()?;
        let level = cursor.read_u32::<byteorder::LittleEndian>()?;

        let shifted_size = size.wrapping_shr(3);
        let byte_count = if size % 8 != 0 {
            shifted_size + 1
        } else {
            shifted_size
        };

        let mut bitvec_buf = vec![0u8; byte_count];
        cursor.read_exact(&mut bitvec_buf)?;

        // swap the bits, since the bit order of our python libraries differs
        let v: Vec<u8> = bitvec_buf.into_iter().map(|x| x.swap_bits()).collect();

        Ok(Bloom {
            level,
            n_hash_funcs,
            size,
            bitvec: BitVec::from_bytes(&v),
        })
    }

    fn hash(&self, n_fn: u32, key: &[u8]) -> usize {
        let hash_seed = (n_fn << 16) + self.level;
        let h = murmurhash3_x86_32(key, hash_seed) as usize % self.size;
        h
    }

    pub fn put(&mut self, item: &[u8]) {
        for i in 0..self.n_hash_funcs {
            let index = self.hash(i, item);
            self.bitvec.set(index, true);
        }
    }

    pub fn has(&self, item: &[u8]) -> bool {
        for i in 0..self.n_hash_funcs {
            match self.bitvec.get(self.hash(i, item)) {
                Some(false) => return false,
                Some(true) => (),
                None => panic!(
                    "access outside the bloom filter bit vector (this is almost certainly a bug)"
                ),
            }
        }

        true
    }

    pub fn clear(&mut self) {
        self.bitvec.clear()
    }
}

#[derive(Debug)]
pub struct Cascade {
    filter: Bloom,
    child_layer: Option<Box<Cascade>>,
}

impl Cascade {
    pub fn new(size: usize, n_hash_funcs: u32) -> Cascade {
        return Cascade::new_layer(size, n_hash_funcs, 1);
    }

    pub fn from_bytes(bytes: &[u8]) -> Result<Option<Box<Cascade>>, Error> {
        if bytes.len() == 0 {
            return Ok(None);
        }
        let mut cursor = bytes;
        let version = cursor.read_u16::<byteorder::LittleEndian>()?;
        if version != 1 {
            return Err(Error::new(ErrorKind::InvalidInput, "Invalid version"));
        }
        Ok(Some(Box::new(Cascade {
            filter: Bloom::from_bytes(&mut cursor)?,
            child_layer: Cascade::from_bytes(cursor)?,
        })))
    }

    fn new_layer(size: usize, n_hash_funcs: u32, layer: u32) -> Cascade {
        Cascade {
            filter: Bloom::new(size, n_hash_funcs, layer),
            child_layer: Option::None,
        }
    }

    pub fn initialize(&mut self, entries: Vec<Vec<u8>>, exclusions: Vec<Vec<u8>>) {
        let mut false_positives = Vec::new();
        for entry in &entries {
            self.filter.put(entry);
        }

        for entry in exclusions {
            if self.filter.has(&entry) {
                false_positives.push(entry);
            }
        }

        if false_positives.len() > 0 {
            let n_hash_funcs = calculate_n_hash_funcs(0.5);
            let size = calculate_size(false_positives.len(), 0.5);
            let mut child = Box::new(Cascade::new_layer(
                size,
                n_hash_funcs,
                self.filter.level + 1,
            ));
            child.initialize(false_positives, entries);
            self.child_layer = Some(child);
        }
    }

    pub fn has(&self, entry: &[u8]) -> bool {
        if self.filter.has(&entry) {
            match self.child_layer {
                Some(ref child) => {
                    let child_value = !child.has(entry);
                    return child_value;
                }
                None => {
                    return true;
                }
            }
        }
        return false;
    }

    pub fn check(&self, entries: Vec<Vec<u8>>, exclusions: Vec<Vec<u8>>) -> bool {
        for entry in entries {
            if !self.has(&entry) {
                return false;
            }
        }

        for entry in exclusions {
            if self.has(&entry) {
                return false;
            }
        }

        true
    }
}

#[cfg(test)]
mod tests {
    use calculate_n_hash_funcs;
    use calculate_size;
    use rand::Rng;
    use Bloom;
    use Cascade;

    #[test]
    fn bloom_test_bloom_size() {
        let error_rate = 0.01;
        let elements = 1024;
        let n_hash_funcs = calculate_n_hash_funcs(error_rate);
        let size = calculate_size(elements, error_rate);

        let bloom = Bloom::new(size, n_hash_funcs, 0);
        assert!(bloom.bitvec.len() == 9829);
    }

    #[test]
    fn bloom_test_put() {
        let error_rate = 0.01;
        let elements = 1024;
        let n_hash_funcs = calculate_n_hash_funcs(error_rate);
        let size = calculate_size(elements, error_rate);

        let mut bloom = Bloom::new(size, n_hash_funcs, 0);
        let key: &[u8] = b"foo";

        bloom.put(key);
    }

    #[test]
    fn bloom_test_has() {
        let error_rate = 0.01;
        let elements = 1024;
        let n_hash_funcs = calculate_n_hash_funcs(error_rate);
        let size = calculate_size(elements, error_rate);

        let mut bloom = Bloom::new(size, n_hash_funcs, 0);
        let key: &[u8] = b"foo";

        bloom.put(key);
        assert!(bloom.has(key) == true);
        assert!(bloom.has(b"bar") == false);
    }

    #[test]
    fn bloom_test_from_bytes() {
        let src: Vec<u8> = vec![
            0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x41, 0x00,
        ];

        match Bloom::from_bytes(&mut &src[..]) {
            Ok(mut bloom) => {
                assert!(bloom.has(b"this") == true);
                assert!(bloom.has(b"that") == true);
                assert!(bloom.has(b"other") == false);

                bloom.put(b"other");
                assert!(bloom.has(b"other") == true);
            }
            Err(_) => {
                panic!("Parsing failed");
            }
        };

        let short: Vec<u8> = vec![
            0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x41,
        ];
        match Bloom::from_bytes(&mut &short[..]) {
            Ok(_) => {
                panic!("Parsing should fail; data is truncated");
            }
            Err(_) => {}
        };
    }

    #[test]
    fn bloom_test_from_file() {
        let v = include_bytes!("../test_data/test_bf");
        let bloom = Bloom::from_bytes(&mut &v[..]).expect("parsing Bloom should succeed");
        assert!(bloom.has(b"this") == true);
        assert!(bloom.has(b"that") == true);
        assert!(bloom.has(b"yet another test") == false);
    }

    #[test]
    fn cascade_test() {
        // thread_rng is often the most convenient source of randomness:
        let mut rng = rand::thread_rng();

        // create some entries and exclusions
        let mut foo: Vec<Vec<u8>> = Vec::new();
        let mut bar: Vec<Vec<u8>> = Vec::new();

        for i in 0..500 {
            let s = format!("{}", i);
            let bytes = s.into_bytes();
            foo.push(bytes);
        }

        for _ in 0..100 {
            let idx = rng.gen_range(0, foo.len());
            bar.push(foo.swap_remove(idx));
        }

        let error_rate = 0.5;
        let elements = 500;
        let n_hash_funcs = calculate_n_hash_funcs(error_rate);
        let size = calculate_size(elements, error_rate);

        let mut cascade = Cascade::new(size, n_hash_funcs);
        cascade.initialize(foo.clone(), bar.clone());

        assert!(cascade.check(foo.clone(), bar.clone()) == true);
    }

    #[test]
    fn cascade_from_file_bytes_test() {
        let v = include_bytes!("../test_data/test_mlbf");
        let cascade = Cascade::from_bytes(v)
            .expect("parsing Cascade should succeed")
            .expect("Cascade should be Some");
        assert!(cascade.has(b"test") == true);
        assert!(cascade.has(b"another test") == true);
        assert!(cascade.has(b"yet another test") == true);
        assert!(cascade.has(b"blah") == false);
        assert!(cascade.has(b"blah blah") == false);
        assert!(cascade.has(b"blah blah blah") == false);

        let v = include_bytes!("../test_data/test_short_mlbf");
        Cascade::from_bytes(v).expect_err("parsing truncated Cascade should fail");
    }
}
