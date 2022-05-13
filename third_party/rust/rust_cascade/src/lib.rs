//! # rust-cascade
//!
//! A library for creating and querying the cascading bloom filters described by
//! Larisch, Choffnes, Levin, Maggs, Mislove, and Wilson in
//! "CRLite: A Scalable System for Pushing All TLS Revocations to All Browsers"
//! <https://www.ieee-security.org/TC/SP2017/papers/567.pdf>

extern crate byteorder;
extern crate murmurhash3;
extern crate rand;
extern crate sha2;

use byteorder::{ByteOrder, LittleEndian, ReadBytesExt};
use murmurhash3::murmurhash3_x86_32;
#[cfg(feature = "builder")]
use rand::rngs::OsRng;
#[cfg(feature = "builder")]
use rand::RngCore;
use sha2::{Digest, Sha256};
use std::convert::{TryFrom, TryInto};
use std::fmt;
use std::io::{ErrorKind, Read};
use std::mem::size_of;

#[derive(Debug)]
pub enum CascadeError {
    LongSalt,
    TooManyLayers,
    Collision,
    UnknownHashFunction,
    CapacityViolation(&'static str),
    Parse(&'static str),
}

impl fmt::Display for CascadeError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            CascadeError::LongSalt => {
                write!(f, "Cannot serialize a filter with a salt of length >= 256.")
            }
            CascadeError::TooManyLayers => {
                write!(f, "Cannot serialize a filter with >= 255 layers.")
            }
            CascadeError::Collision => {
                write!(f, "Collision between included and excluded sets.")
            }
            CascadeError::UnknownHashFunction => {
                write!(f, "Unknown hash function.")
            }
            CascadeError::CapacityViolation(function) => {
                write!(f, "Unexpected call to {}", function)
            }
            CascadeError::Parse(reason) => {
                write!(f, "Cannot parse cascade: {}", reason)
            }
        }
    }
}

/// A Bloom filter representing a specific layer in a multi-layer cascading Bloom filter.
/// The same hash function is used for all layers, so it is not encoded here.
struct Bloom {
    /// How many hash functions this filter uses
    n_hash_funcs: u32,
    /// The bit length of the filter
    size: u32,
    /// The data of the filter
    data: Vec<u8>,
}

#[repr(u8)]
#[derive(Copy, Clone, PartialEq)]
/// These enumerations need to match the python filter-cascade project:
/// <https://github.com/mozilla/filter-cascade/blob/v0.3.0/filtercascade/fileformats.py>
pub enum HashAlgorithm {
    MurmurHash3 = 1,
    Sha256l32 = 2, // low 32 bits of sha256
    Sha256 = 3,    // all 256 bits of sha256
}

impl fmt::Display for HashAlgorithm {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", *self as u8)
    }
}

impl TryFrom<u8> for HashAlgorithm {
    type Error = CascadeError;
    fn try_from(value: u8) -> Result<HashAlgorithm, CascadeError> {
        match value {
            // Naturally, these need to match the enum declaration
            1 => Ok(Self::MurmurHash3),
            2 => Ok(Self::Sha256l32),
            3 => Ok(Self::Sha256),
            _ => Err(CascadeError::UnknownHashFunction),
        }
    }
}

/// A CascadeIndexGenerator provides one-time access to a table of pseudorandom functions H_ij
/// in which each function is of the form
///     H(s: &[u8], r: u32) -> usize
/// and for which 0 <= H(s,r) < r for all s, r.
/// The pseudorandom functions share a common key, represented as a octet string, and the table can
/// be constructed from this key alone. The functions are pseudorandom with respect to s, but not
/// r. For a uniformly random key/table, fixed r, and arbitrary strings m0 and m1,
///      H_ij(m0, r) is computationally indistinguishable from H_ij(m1,r)
/// for all i,j.
///
/// A call to next_layer() increments i and resets j.
/// A call to next_index(s, r) increments j, and outputs some value H_ij(s) with 0 <= H_ij(s) < r.

#[derive(Debug)]
enum CascadeIndexGenerator {
    MurmurHash3 {
        key: Vec<u8>,
        counter: u32,
        depth: u8,
    },
    Sha256l32 {
        key: Vec<u8>,
        counter: u32,
        depth: u8,
    },
    Sha256Ctr {
        key: Vec<u8>,
        counter: u32,
        state: [u8; 32],
        state_available: u8,
    },
}

impl PartialEq for CascadeIndexGenerator {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (
                CascadeIndexGenerator::MurmurHash3 { key: ref a, .. },
                CascadeIndexGenerator::MurmurHash3 { key: ref b, .. },
            )
            | (
                CascadeIndexGenerator::Sha256l32 { key: ref a, .. },
                CascadeIndexGenerator::Sha256l32 { key: ref b, .. },
            )
            | (
                CascadeIndexGenerator::Sha256Ctr { key: ref a, .. },
                CascadeIndexGenerator::Sha256Ctr { key: ref b, .. },
            ) => a == b,
            _ => false,
        }
    }
}

impl CascadeIndexGenerator {
    fn new(hash_alg: HashAlgorithm, key: Vec<u8>) -> Self {
        match hash_alg {
            HashAlgorithm::MurmurHash3 => Self::MurmurHash3 {
                key,
                counter: 0,
                depth: 1,
            },
            HashAlgorithm::Sha256l32 => Self::Sha256l32 {
                key,
                counter: 0,
                depth: 1,
            },
            HashAlgorithm::Sha256 => Self::Sha256Ctr {
                key,
                counter: 0,
                state: [0; 32],
                state_available: 0,
            },
        }
    }

    fn next_layer(&mut self) {
        match self {
            Self::MurmurHash3 {
                ref mut counter,
                ref mut depth,
                ..
            }
            | Self::Sha256l32 {
                ref mut counter,
                ref mut depth,
                ..
            } => {
                *counter = 0;
                *depth += 1;
            }
            Self::Sha256Ctr { .. } => (),
        }
    }

    fn next_index(&mut self, salt: &[u8], range: u32) -> usize {
        let index = match self {
            Self::MurmurHash3 {
                key,
                ref mut counter,
                depth,
            } => {
                let hash_seed = (*counter << 16) + *depth as u32;
                *counter += 1;
                murmurhash3_x86_32(key, hash_seed)
            }

            Self::Sha256l32 {
                key,
                ref mut counter,
                depth,
            } => {
                let mut hasher = Sha256::new();
                hasher.update(salt);
                hasher.update(counter.to_le_bytes());
                hasher.update(depth.to_le_bytes());
                hasher.update(&key);
                *counter += 1;
                u32::from_le_bytes(
                    hasher.finalize()[0..4]
                        .try_into()
                        .expect("sha256 should have given enough bytes"),
                )
            }

            Self::Sha256Ctr {
                key,
                ref mut counter,
                ref mut state,
                ref mut state_available,
            } => {
                // |bytes_needed| is the minimum number of bytes needed to represent a value in [0, range).
                let bytes_needed = ((range.next_power_of_two().trailing_zeros() + 7) / 8) as usize;
                let mut index_arr = [0u8; 4];
                for byte in index_arr.iter_mut().take(bytes_needed) {
                    if *state_available == 0 {
                        let mut hasher = Sha256::new();
                        hasher.update(counter.to_le_bytes());
                        hasher.update(salt);
                        hasher.update(&key);
                        hasher.finalize_into(state.into());
                        *state_available = state.len() as u8;
                        *counter += 1;
                    }
                    *byte = state[state.len() - *state_available as usize];
                    *state_available -= 1;
                }
                LittleEndian::read_u32(&index_arr)
            }
        };
        (index % range) as usize
    }
}

impl Bloom {
    /// `new_crlite_bloom` creates an empty bloom filter for a layer of a cascade with the
    /// parameters specified in [LCL+17, Section III.C].
    ///
    /// # Arguments
    /// * `include_capacity` - the number of elements that will be encoded at the new layer.
    /// * `exclude_capacity` - the number of elements in the complement of the encoded set.
    /// * `top_layer`        - whether this is the top layer of the filter.
    #[cfg(feature = "builder")]
    pub fn new_crlite_bloom(
        include_capacity: usize,
        exclude_capacity: usize,
        top_layer: bool,
    ) -> Self {
        assert!(include_capacity != 0 && exclude_capacity != 0);

        let r = include_capacity as f64;
        let s = exclude_capacity as f64;

        // The desired false positive rate for the top layer is
        //   p = r/(sqrt(2)*s).
        // With this setting, the number of false positives (which will need to be
        // encoded at the second layer) is expected to be a factor of sqrt(2)
        // smaller than the number of elements encoded at the top layer.
        //
        // At layer i > 1 we try to ensure that the number of elements to be
        // encoded at layer i+1 is half the number of elements encoded at
        // layer i. So we take p = 1/2.
        let log2_fp_rate = match top_layer {
            true => (r / s).log2() - 0.5f64,
            false => -1f64,
        };

        // the number of hash functions (k) and the size of the bloom filter (m) are given in
        // [LCL+17] as k = log2(1/p)  and  m = r log2(1/p) / ln(2).
        //
        // If this formula gives a value of m < 256, we take m=256 instead. This results in very
        // slightly sub-optimal size, but gives us the added benefit of doing less hashing.
        let n_hash_funcs = (-log2_fp_rate).round() as u32;
        let size = match (r * (-log2_fp_rate) / (f64::ln(2f64))).round() as u32 {
            size if size >= 256 => size,
            _ => 256,
        };

        Bloom {
            n_hash_funcs,
            size,
            data: vec![0u8; ((size + 7) / 8) as usize],
        }
    }

    /// `read` attempts to decode the Bloom filter represented by the bytes in the given reader.
    ///
    /// # Arguments
    /// * `reader` - The encoded representation of this Bloom filter. May be empty. May include
    /// additional data describing further Bloom filters.
    /// The format of an encoded Bloom filter is:
    /// [1 byte] - the hash algorithm to use in the filter
    /// [4 little endian bytes] - the length in bits of the filter
    /// [4 little endian bytes] - the number of hash functions to use in the filter
    /// [1 byte] - which layer in the cascade this filter is
    /// [variable length bytes] - the filter itself (must be of minimal length)
    pub fn read<R: Read>(
        reader: &mut R,
    ) -> Result<Option<(Bloom, usize, HashAlgorithm)>, CascadeError> {
        let hash_algorithm_val = match reader.read_u8() {
            Ok(val) => val,
            // If reader is at EOF, there is no bloom filter.
            Err(e) if e.kind() == ErrorKind::UnexpectedEof => return Ok(None),
            Err(_) => return Err(CascadeError::Parse("read error")),
        };
        let hash_algorithm = HashAlgorithm::try_from(hash_algorithm_val)?;

        let size = reader
            .read_u32::<byteorder::LittleEndian>()
            .or(Err(CascadeError::Parse("truncated at layer size")))?;
        let n_hash_funcs = reader
            .read_u32::<byteorder::LittleEndian>()
            .or(Err(CascadeError::Parse("truncated at layer hash count")))?;
        let layer = reader
            .read_u8()
            .or(Err(CascadeError::Parse("truncated at layer number")))?;

        let byte_count = ((size + 7) / 8) as usize;
        let mut data = vec![0; byte_count];
        reader
            .read_exact(&mut data)
            .or(Err(CascadeError::Parse("truncated at layer data")))?;
        let bloom = Bloom {
            n_hash_funcs,
            size,
            data,
        };
        Ok(Some((bloom, layer as usize, hash_algorithm)))
    }

    fn has(&self, generator: &mut CascadeIndexGenerator, salt: &[u8]) -> bool {
        for _ in 0..self.n_hash_funcs {
            let bit_index = generator.next_index(salt, self.size);
            assert!(bit_index < self.size as usize);
            let byte_index = bit_index / 8;
            let mask = 1 << (bit_index % 8);
            if self.data[byte_index] & mask == 0 {
                return false;
            }
        }
        true
    }

    #[cfg(feature = "builder")]
    fn insert(&mut self, generator: &mut CascadeIndexGenerator, salt: &[u8]) {
        for _ in 0..self.n_hash_funcs {
            let bit_index = generator.next_index(salt, self.size);
            let byte_index = bit_index / 8;
            let mask = 1 << (bit_index % 8);
            self.data[byte_index] |= mask;
        }
    }

    pub fn approximate_size_of(&self) -> usize {
        size_of::<Bloom>() + self.data.len()
    }
}

impl fmt::Display for Bloom {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "n_hash_funcs={} size={}", self.n_hash_funcs, self.size)
    }
}

/// A multi-layer cascading Bloom filter.
pub struct Cascade {
    /// The Bloom filter for this layer in the cascade
    filters: Vec<Bloom>,
    /// The salt in use, if any
    salt: Vec<u8>,
    /// The hash algorithm / index generating function to use
    hash_algorithm: HashAlgorithm,
    /// Whether the logic should be inverted
    inverted: bool,
}

impl Cascade {
    /// from_bytes attempts to decode and return a multi-layer cascading Bloom filter.
    ///
    /// # Arguments
    /// `bytes` - The encoded representation of the Bloom filters in this cascade. Starts with 2
    /// little endian bytes indicating the version. The current version is 2. The Python
    /// filter-cascade project defines the formats, see
    /// <https://github.com/mozilla/filter-cascade/blob/v0.3.0/filtercascade/fileformats.py>
    ///
    /// May be of length 0, in which case `None` is returned.
    pub fn from_bytes(bytes: Vec<u8>) -> Result<Option<Self>, CascadeError> {
        if bytes.is_empty() {
            return Ok(None);
        }
        let mut reader = bytes.as_slice();
        let version = reader
            .read_u16::<byteorder::LittleEndian>()
            .or(Err(CascadeError::Parse("truncated at version")))?;

        let mut filters = vec![];
        let mut salt = vec![];
        let mut top_hash_alg = None;
        let mut inverted = false;

        if version > 2 {
            return Err(CascadeError::Parse("unknown version"));
        }

        if version == 2 {
            let inverted_val = reader
                .read_u8()
                .or(Err(CascadeError::Parse("truncated at inverted")))?;
            if inverted_val > 1 {
                return Err(CascadeError::Parse("invalid value for inverted"));
            }
            inverted = 0 != inverted_val;
            let salt_len: usize = reader
                .read_u8()
                .or(Err(CascadeError::Parse("truncated at salt length")))?
                .into();
            if salt_len >= 256 {
                return Err(CascadeError::Parse("salt too long"));
            }
            if salt_len > 0 {
                let mut salt_bytes = vec![0; salt_len];
                reader
                    .read_exact(&mut salt_bytes)
                    .or(Err(CascadeError::Parse("truncated at salt")))?;
                salt = salt_bytes;
            }
        }

        while let Some((filter, layer_number, layer_hash_alg)) = Bloom::read(&mut reader)? {
            filters.push(filter);

            if layer_number != filters.len() {
                return Err(CascadeError::Parse("irregular layer numbering"));
            }

            if *top_hash_alg.get_or_insert(layer_hash_alg) != layer_hash_alg {
                return Err(CascadeError::Parse("Inconsistent hash algorithms"));
            }
        }

        if filters.is_empty() {
            return Err(CascadeError::Parse("missing filters"));
        }

        let hash_algorithm = top_hash_alg.ok_or(CascadeError::Parse("missing hash algorithm"))?;

        Ok(Some(Cascade {
            filters,
            salt,
            hash_algorithm,
            inverted,
        }))
    }

    /// to_bytes encodes a cascade in the version 2 format.
    pub fn to_bytes(&self) -> Result<Vec<u8>, CascadeError> {
        if self.salt.len() >= 256 {
            return Err(CascadeError::LongSalt);
        }
        if self.filters.len() >= 255 {
            return Err(CascadeError::TooManyLayers);
        }
        let mut out = vec![];
        let version: u16 = 2;
        let inverted: u8 = self.inverted.into();
        let salt_len: u8 = self.salt.len() as u8;
        let hash_alg: u8 = self.hash_algorithm as u8;
        out.extend_from_slice(&version.to_le_bytes());
        out.push(inverted);
        out.push(salt_len);
        out.extend_from_slice(&self.salt);
        for (layer, bloom) in self.filters.iter().enumerate() {
            out.push(hash_alg);
            out.extend_from_slice(&bloom.size.to_le_bytes());
            out.extend_from_slice(&bloom.n_hash_funcs.to_le_bytes());
            out.push((1 + layer) as u8); // 1-indexed
            out.extend_from_slice(&bloom.data);
        }
        Ok(out)
    }

    /// has determines if the given sequence of bytes is in the cascade.
    ///
    /// # Arguments
    /// `entry` - The bytes to query
    pub fn has(&self, entry: Vec<u8>) -> bool {
        // Query filters 0..self.filters.len() until we get a non-membership result.
        // If this occurs at an even index filter, the element *is not* included.
        // ... at an odd-index filter, the element *is* included.
        let mut generator = CascadeIndexGenerator::new(self.hash_algorithm, entry);
        let mut rv = false;
        for filter in &self.filters {
            if filter.has(&mut generator, &self.salt) {
                rv = !rv;
                generator.next_layer();
            } else {
                break;
            }
        }
        if self.inverted {
            rv = !rv;
        }
        rv
    }

    pub fn invert(&mut self) {
        self.inverted = !self.inverted;
    }

    /// Determine the approximate amount of memory in bytes used by this
    /// Cascade. Because this implementation does not integrate with the
    /// allocator, it can't get an accurate measurement of how much memory it
    /// uses. However, it can make a reasonable guess, assuming the sizes of
    /// the bloom filters are large enough to dominate the overall allocated
    /// size.
    pub fn approximate_size_of(&self) -> usize {
        size_of::<Cascade>()
            + self
                .filters
                .iter()
                .map(|x| x.approximate_size_of())
                .sum::<usize>()
            + self.salt.len()
    }
}

impl fmt::Display for Cascade {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        writeln!(
            f,
            "salt={:?} inverted={} hash_algorithm={}",
            self.salt, self.inverted, self.hash_algorithm,
        )?;
        for filter in &self.filters {
            writeln!(f, "\t[{}]", filter)?;
        }
        Ok(())
    }
}

/// A CascadeBuilder creates a Cascade with layers given by `Bloom::new_crlite_bloom`.
///
/// A builder is initialized using [`CascadeBuilder::default`] or [`CascadeBuilder::new`]. Prefer `default`. The `new` constructor
/// allows the user to specify sensitive internal details such as the hash function and the domain
/// separation parameter.
///
/// Both constructors take `include_capacity` and an `exclude_capacity` parameters. The
/// `include_capacity` is the number of elements that will be encoded in the Cascade. The
/// `exclude_capacity` is size of the complement of the encoded set.
///
/// The encoded set is specified through calls to [`CascadeBuilder::include`]. Its complement is specified through
/// calls to [`CascadeBuilder::exclude`]. The cascade is built with a call to [`CascadeBuilder::finalize`].
///
/// The builder will track of the number of calls to `include` and `exclude`.
/// The caller is responsible for making *exactly* `include_capacity` calls to `include`
/// followed by *exactly* `exclude_capacity` calls to `exclude`.
/// Calling `exclude` before all `include` calls have been made will result in a panic!().
/// Calling `finalize` before all `exclude` calls have been made will result in a panic!().
///
#[cfg(feature = "builder")]
pub struct CascadeBuilder {
    filters: Vec<Bloom>,
    salt: Vec<u8>,
    hash_algorithm: HashAlgorithm,
    to_include: Vec<CascadeIndexGenerator>,
    to_exclude: Vec<CascadeIndexGenerator>,
    status: BuildStatus,
}

#[cfg(feature = "builder")]
impl CascadeBuilder {
    pub fn default(include_capacity: usize, exclude_capacity: usize) -> Self {
        let mut salt = vec![0u8; 16];
        OsRng.fill_bytes(&mut salt);
        CascadeBuilder::new(
            HashAlgorithm::Sha256,
            salt,
            include_capacity,
            exclude_capacity,
        )
    }

    pub fn new(
        hash_algorithm: HashAlgorithm,
        salt: Vec<u8>,
        include_capacity: usize,
        exclude_capacity: usize,
    ) -> Self {
        CascadeBuilder {
            filters: vec![Bloom::new_crlite_bloom(
                include_capacity,
                exclude_capacity,
                true,
            )],
            salt,
            to_include: vec![],
            to_exclude: vec![],
            hash_algorithm,
            status: BuildStatus(include_capacity, exclude_capacity),
        }
    }

    pub fn include(&mut self, item: Vec<u8>) -> Result<(), CascadeError> {
        match self.status {
            BuildStatus(ref mut cap, _) if *cap > 0 => *cap -= 1,
            _ => return Err(CascadeError::CapacityViolation("include")),
        }
        let mut generator = CascadeIndexGenerator::new(self.hash_algorithm, item);
        self.filters[0].insert(&mut generator, &self.salt);
        self.to_include.push(generator);

        Ok(())
    }

    pub fn exclude(&mut self, item: Vec<u8>) -> Result<(), CascadeError> {
        match self.status {
            BuildStatus(0, ref mut cap) if *cap > 0 => *cap -= 1,
            _ => return Err(CascadeError::CapacityViolation("exclude")),
        }
        let mut generator = CascadeIndexGenerator::new(self.hash_algorithm, item);
        if self.filters[0].has(&mut generator, &self.salt) {
            self.to_exclude.push(generator);
        }
        Ok(())
    }

    /// `exclude_threaded` is like `exclude` but it stores false positives in a caller-owned
    /// `ExcludeSet`. This allows the caller to exclude items in parallel.
    pub fn exclude_threaded(&self, exclude_set: &mut ExcludeSet, item: Vec<u8>) {
        exclude_set.size += 1;
        let mut generator = CascadeIndexGenerator::new(self.hash_algorithm, item);
        if self.filters[0].has(&mut generator, &self.salt) {
            exclude_set.set.push(generator);
        }
    }

    /// `collect_exclude_set` merges an `ExcludeSet` into the internal storage of the CascadeBuilder.
    pub fn collect_exclude_set(
        &mut self,
        exclude_set: &mut ExcludeSet,
    ) -> Result<(), CascadeError> {
        match self.status {
            BuildStatus(0, ref mut cap) if *cap >= exclude_set.size => *cap -= exclude_set.size,
            _ => return Err(CascadeError::CapacityViolation("exclude")),
        }
        self.to_exclude.append(&mut exclude_set.set);

        Ok(())
    }

    fn push_layer(&mut self) -> Result<(), CascadeError> {
        // At even layers we encode elements of to_include. At odd layers we encode elements of
        // to_exclude. In both cases, we track false positives by filtering the complement of the
        // encoded set through the newly produced bloom filter.
        let at_even_layer = self.filters.len() % 2 == 0;
        let (to_encode, to_filter) = match at_even_layer {
            true => (&mut self.to_include, &mut self.to_exclude),
            false => (&mut self.to_exclude, &mut self.to_include),
        };

        // split ownership of `salt` away from `to_encode` and `to_filter`
        // We need an immutable reference to salt during `to_encode.iter_mut()`
        let mut bloom = Bloom::new_crlite_bloom(to_encode.len(), to_filter.len(), false);

        let salt = self.salt.as_slice();

        to_encode.iter_mut().for_each(|x| {
            x.next_layer();
            bloom.insert(x, salt)
        });

        let mut delta = to_filter.len();
        to_filter.retain_mut(|x| {
            x.next_layer();
            bloom.has(x, salt)
        });
        delta -= to_filter.len();

        if delta == 0 {
            // Check for collisions between the |to_encode| and |to_filter| sets.
            // The implementation of PartialEq for CascadeIndexGenerator will successfully
            // identify cases where the user called |include(item)| and |exclude(item)| for the
            // same item. It will not identify collisions in the underlying hash function.
            for x in to_encode.iter_mut() {
                if to_filter.contains(x) {
                    return Err(CascadeError::Collision);
                }
            }
        }

        self.filters.push(bloom);
        Ok(())
    }

    pub fn finalize(mut self) -> Result<Box<Cascade>, CascadeError> {
        match self.status {
            BuildStatus(0, 0) => (),
            _ => return Err(CascadeError::CapacityViolation("finalize")),
        }

        loop {
            if self.to_exclude.is_empty() {
                break;
            }
            self.push_layer()?;

            if self.to_include.is_empty() {
                break;
            }
            self.push_layer()?;
        }

        Ok(Box::new(Cascade {
            filters: self.filters,
            salt: self.salt,
            hash_algorithm: self.hash_algorithm,
            inverted: false,
        }))
    }
}

/// BuildStatus is used to ensure that the `include`, `exclude`, and `finalize` calls to
/// CascadeBuilder are made in the right order. The (a,b) state indicates that the
/// CascadeBuilder is waiting for `a` calls to `include` and `b` calls to `exclude`.
#[cfg(feature = "builder")]
struct BuildStatus(usize, usize);

/// CascadeBuilder::exclude takes `&mut self` so that it can count exclusions and push items to
/// self.to_exclude. The bulk of the work it does, however, can be done with an immutable reference
/// to the top level bloom filter. An `ExcludeSet` is used by `CascadeBuilder::exclude_threaded` to
/// track the changes to a `CascadeBuilder` that would be made with a call to
/// `CascadeBuilder::exclude`.
#[cfg(feature = "builder")]
#[derive(Default)]
pub struct ExcludeSet {
    size: usize,
    set: Vec<CascadeIndexGenerator>,
}

#[cfg(test)]
mod tests {
    use Bloom;
    use Cascade;
    #[cfg(feature = "builder")]
    use CascadeBuilder;
    #[cfg(feature = "builder")]
    use CascadeError;
    use CascadeIndexGenerator;
    #[cfg(feature = "builder")]
    use ExcludeSet;
    use HashAlgorithm;

    #[test]
    fn bloom_v1_test_from_bytes() {
        let src: Vec<u8> = vec![
            0x01, 0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x41, 0x00,
        ];
        let mut reader = src.as_slice();

        match Bloom::read(&mut reader) {
            Ok(Some((bloom, 1, HashAlgorithm::MurmurHash3))) => {
                assert!(bloom.has(
                    &mut CascadeIndexGenerator::new(HashAlgorithm::MurmurHash3, b"this".to_vec()),
                    &vec![]
                ));
                assert!(bloom.has(
                    &mut CascadeIndexGenerator::new(HashAlgorithm::MurmurHash3, b"that".to_vec()),
                    &vec![]
                ));
                assert!(!bloom.has(
                    &mut CascadeIndexGenerator::new(HashAlgorithm::MurmurHash3, b"other".to_vec()),
                    &vec![]
                ));
            }
            Ok(_) => panic!("Parsing failed"),
            Err(_) => panic!("Parsing failed"),
        };
        assert!(reader.is_empty());

        let short: Vec<u8> = vec![
            0x01, 0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x41,
        ];
        assert!(Bloom::read(&mut short.as_slice()).is_err());

        let empty: Vec<u8> = Vec::new();
        let mut reader = empty.as_slice();
        match Bloom::read(&mut reader) {
            Ok(should_be_none) => assert!(should_be_none.is_none()),
            Err(_) => panic!("Parsing failed"),
        };
    }

    #[test]
    fn bloom_v3_unsupported() {
        let src: Vec<u8> = vec![0x03, 0x01, 0x00];
        assert!(Bloom::read(&mut src.as_slice()).is_err());
    }

    #[test]
    fn cascade_v1_murmur_from_file_bytes_test() {
        let v = include_bytes!("../test_data/test_v1_murmur_mlbf").to_vec();
        let cascade = Cascade::from_bytes(v)
            .expect("parsing Cascade should succeed")
            .expect("Cascade should be Some");
        // Key format is SHA256(issuer SPKI) + serial number
        let key_for_revoked_cert_1 = vec![
            0x2e, 0xb2, 0xd5, 0xa8, 0x60, 0xfe, 0x50, 0xe9, 0xc2, 0x42, 0x36, 0x85, 0x52, 0x98,
            0x01, 0x50, 0xe4, 0x5d, 0xb5, 0x32, 0x1a, 0x5b, 0x00, 0x5e, 0x26, 0xd6, 0x76, 0x25,
            0x3a, 0x40, 0x9b, 0xf5, 0x06, 0x2d, 0xf5, 0x68, 0xa0, 0x51, 0x31, 0x08, 0x20, 0xd7,
            0xec, 0x43, 0x27, 0xe1, 0xba, 0xfd,
        ];
        assert!(cascade.has(key_for_revoked_cert_1));
        let key_for_revoked_cert_2 = vec![
            0xf1, 0x1c, 0x3d, 0xd0, 0x48, 0xf7, 0x4e, 0xdb, 0x7c, 0x45, 0x19, 0x2b, 0x83, 0xe5,
            0x98, 0x0d, 0x2f, 0x67, 0xec, 0x84, 0xb4, 0xdd, 0xb9, 0x39, 0x6e, 0x33, 0xff, 0x51,
            0x73, 0xed, 0x69, 0x8f, 0x00, 0xd2, 0xe8, 0xf6, 0xaa, 0x80, 0x48, 0x1c, 0xd4,
        ];
        assert!(cascade.has(key_for_revoked_cert_2));
        let key_for_valid_cert = vec![
            0x99, 0xfc, 0x9d, 0x40, 0xf1, 0xad, 0xb1, 0x63, 0x65, 0x61, 0xa6, 0x1d, 0x68, 0x3d,
            0x9e, 0xa6, 0xb4, 0x60, 0xc5, 0x7d, 0x0c, 0x75, 0xea, 0x00, 0xc3, 0x41, 0xb9, 0xdf,
            0xb9, 0x0b, 0x5f, 0x39, 0x0b, 0x77, 0x75, 0xf7, 0xaf, 0x9a, 0xe5, 0x42, 0x65, 0xc9,
            0xcd, 0x32, 0x57, 0x10, 0x77, 0x8e,
        ];
        assert!(!cascade.has(key_for_valid_cert));

        assert_eq!(cascade.approximate_size_of(), 15408);

        let v = include_bytes!("../test_data/test_v1_murmur_short_mlbf").to_vec();
        assert!(Cascade::from_bytes(v).is_err());
    }

    #[test]
    fn cascade_v2_sha256l32_from_file_bytes_test() {
        let v = include_bytes!("../test_data/test_v2_sha256l32_mlbf").to_vec();
        let cascade = Cascade::from_bytes(v)
            .expect("parsing Cascade should succeed")
            .expect("Cascade should be Some");

        assert!(cascade.salt.len() == 0);
        assert!(cascade.inverted == false);
        assert!(cascade.has(b"this".to_vec()) == true);
        assert!(cascade.has(b"that".to_vec()) == true);
        assert!(cascade.has(b"other".to_vec()) == false);
        assert_eq!(cascade.approximate_size_of(), 1001);
    }

    #[test]
    fn cascade_v2_sha256l32_with_salt_from_file_bytes_test() {
        let v = include_bytes!("../test_data/test_v2_sha256l32_salt_mlbf").to_vec();
        let cascade = Cascade::from_bytes(v)
            .expect("parsing Cascade should succeed")
            .expect("Cascade should be Some");

        assert!(cascade.salt == b"nacl".to_vec());
        assert!(cascade.inverted == false);
        assert!(cascade.has(b"this".to_vec()) == true);
        assert!(cascade.has(b"that".to_vec()) == true);
        assert!(cascade.has(b"other".to_vec()) == false);
        assert_eq!(cascade.approximate_size_of(), 1001);
    }

    #[test]
    fn cascade_v2_murmur_from_file_bytes_test() {
        let v = include_bytes!("../test_data/test_v2_murmur_mlbf").to_vec();
        let cascade = Cascade::from_bytes(v)
            .expect("parsing Cascade should succeed")
            .expect("Cascade should be Some");

        assert!(cascade.salt.len() == 0);
        assert!(cascade.inverted == false);
        assert!(cascade.has(b"this".to_vec()) == true);
        assert!(cascade.has(b"that".to_vec()) == true);
        assert!(cascade.has(b"other".to_vec()) == false);
        assert_eq!(cascade.approximate_size_of(), 992);
    }

    #[test]
    fn cascade_v2_murmur_inverted_from_file_bytes_test() {
        let v = include_bytes!("../test_data/test_v2_murmur_inverted_mlbf").to_vec();
        let cascade = Cascade::from_bytes(v)
            .expect("parsing Cascade should succeed")
            .expect("Cascade should be Some");

        assert!(cascade.salt.len() == 0);
        assert!(cascade.inverted == true);
        assert!(cascade.has(b"this".to_vec()) == true);
        assert!(cascade.has(b"that".to_vec()) == true);
        assert!(cascade.has(b"other".to_vec()) == false);
        assert_eq!(cascade.approximate_size_of(), 1058);
    }

    #[test]
    fn cascade_v2_sha256l32_inverted_from_file_bytes_test() {
        let v = include_bytes!("../test_data/test_v2_sha256l32_inverted_mlbf").to_vec();
        let cascade = Cascade::from_bytes(v)
            .expect("parsing Cascade should succeed")
            .expect("Cascade should be Some");

        assert!(cascade.salt.len() == 0);
        assert!(cascade.inverted == true);
        assert!(cascade.has(b"this".to_vec()) == true);
        assert!(cascade.has(b"that".to_vec()) == true);
        assert!(cascade.has(b"other".to_vec()) == false);
        assert_eq!(cascade.approximate_size_of(), 1061);
    }

    #[test]
    fn cascade_v2_sha256ctr_from_file_bytes_test() {
        let v = include_bytes!("../test_data/test_v2_sha256ctr_salt_mlbf").to_vec();
        let cascade = Cascade::from_bytes(v)
            .expect("parsing Cascade should succeed")
            .expect("Cascade should be Some");

        assert!(cascade.salt == b"nacl".to_vec());
        assert!(cascade.inverted == false);
        assert!(cascade.has(b"this".to_vec()) == true);
        assert!(cascade.has(b"that".to_vec()) == true);
        assert!(cascade.has(b"other".to_vec()) == false);
        assert_eq!(cascade.approximate_size_of(), 1070);
    }

    #[test]
    fn cascade_empty() {
        let cascade = Cascade::from_bytes(Vec::new()).expect("parsing Cascade should succeed");
        assert!(cascade.is_none());
    }

    #[test]
    fn cascade_test_from_bytes() {
        let unknown_version: Vec<u8> = vec![0xff, 0xff, 0x00, 0x00];
        match Cascade::from_bytes(unknown_version) {
            Ok(_) => panic!("Cascade::from_bytes allows unknown version."),
            Err(_) => (),
        }

        let first_layer_is_zero: Vec<u8> = vec![
            0x01, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];
        match Cascade::from_bytes(first_layer_is_zero) {
            Ok(_) => panic!("Cascade::from_bytes allows zero indexed layers."),
            Err(_) => (),
        }

        let second_layer_is_three: Vec<u8> = vec![
            0x01, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
            0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00,
        ];
        match Cascade::from_bytes(second_layer_is_three) {
            Ok(_) => panic!("Cascade::from_bytes allows non-sequential layers."),
            Err(_) => (),
        }
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_collision() {
        let mut builder = CascadeBuilder::default(1, 1);
        builder.include(b"collision!".to_vec()).ok();
        builder.exclude(b"collision!".to_vec()).ok();
        assert!(matches!(builder.finalize(), Err(CascadeError::Collision)));
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_exclude_too_few() {
        let mut builder = CascadeBuilder::default(1, 1);
        builder.include(b"1".to_vec()).ok();
        assert!(matches!(
            builder.finalize(),
            Err(CascadeError::CapacityViolation(_))
        ));
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_include_too_few() {
        let mut builder = CascadeBuilder::default(1, 1);
        assert!(matches!(
            builder.exclude(b"1".to_vec()),
            Err(CascadeError::CapacityViolation(_))
        ));
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_include_too_many() {
        let mut builder = CascadeBuilder::default(1, 1);
        builder.include(b"1".to_vec()).ok();
        assert!(matches!(
            builder.include(b"2".to_vec()),
            Err(CascadeError::CapacityViolation(_))
        ));
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_exclude_too_many() {
        let mut builder = CascadeBuilder::default(1, 1);
        builder.include(b"1".to_vec()).ok();
        builder.exclude(b"2".to_vec()).ok();
        assert!(matches!(
            builder.exclude(b"3".to_vec()),
            Err(CascadeError::CapacityViolation(_))
        ));
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_exclude_threaded_no_collect() {
        let mut builder = CascadeBuilder::default(1, 3);
        let mut exclude_set = ExcludeSet::default();
        builder.include(b"1".to_vec()).ok();
        builder.exclude_threaded(&mut exclude_set, b"2".to_vec());
        builder.exclude_threaded(&mut exclude_set, b"3".to_vec());
        builder.exclude_threaded(&mut exclude_set, b"4".to_vec());
        assert!(matches!(
            builder.finalize(),
            Err(CascadeError::CapacityViolation(_))
        ));
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_exclude_threaded_too_many() {
        let mut builder = CascadeBuilder::default(1, 3);
        let mut exclude_set = ExcludeSet::default();
        builder.include(b"1".to_vec()).ok();
        builder.exclude_threaded(&mut exclude_set, b"2".to_vec());
        builder.exclude_threaded(&mut exclude_set, b"3".to_vec());
        builder.exclude_threaded(&mut exclude_set, b"4".to_vec());
        builder.exclude_threaded(&mut exclude_set, b"5".to_vec());
        assert!(matches!(
            builder.collect_exclude_set(&mut exclude_set),
            Err(CascadeError::CapacityViolation(_))
        ));
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_exclude_threaded() {
        let mut builder = CascadeBuilder::default(1, 3);
        let mut exclude_set = ExcludeSet::default();
        builder.include(b"1".to_vec()).ok();
        builder.exclude_threaded(&mut exclude_set, b"2".to_vec());
        builder.exclude_threaded(&mut exclude_set, b"3".to_vec());
        builder.exclude_threaded(&mut exclude_set, b"4".to_vec());
        builder.collect_exclude_set(&mut exclude_set).ok();
        builder.finalize().ok();
    }

    #[cfg(feature = "builder")]
    fn cascade_builder_test_generate(hash_alg: HashAlgorithm, inverted: bool) {
        let total = 10_000_usize;
        let included = 100_usize;

        let salt = vec![0u8; 16];
        let mut builder =
            CascadeBuilder::new(hash_alg, salt, included, (total - included) as usize);
        for i in 0..included {
            builder.include(i.to_le_bytes().to_vec()).ok();
        }
        for i in included..total {
            builder.exclude(i.to_le_bytes().to_vec()).ok();
        }
        let mut cascade = builder.finalize().unwrap();

        if inverted {
            cascade.invert()
        }

        // Ensure we can serialize / deserialize
        let cascade_bytes = cascade.to_bytes().expect("failed to serialize cascade");

        let cascade = Cascade::from_bytes(cascade_bytes)
            .expect("failed to deserialize cascade")
            .expect("cascade should not be None here");

        // Ensure each query gives the correct result
        for i in 0..included {
            assert!(cascade.has(i.to_le_bytes().to_vec()) == true ^ inverted)
        }
        for i in included..total {
            assert!(cascade.has(i.to_le_bytes().to_vec()) == false ^ inverted)
        }
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_generate_murmurhash3_inverted() {
        cascade_builder_test_generate(HashAlgorithm::MurmurHash3, true);
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_generate_murmurhash3() {
        cascade_builder_test_generate(HashAlgorithm::MurmurHash3, false);
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_generate_sha256l32() {
        cascade_builder_test_generate(HashAlgorithm::Sha256l32, false);
    }

    #[test]
    #[cfg(feature = "builder")]
    fn cascade_builder_test_generate_sha256() {
        cascade_builder_test_generate(HashAlgorithm::Sha256, false);
    }
}
