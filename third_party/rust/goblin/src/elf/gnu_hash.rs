//! A Gnu Hash table as 4 sections:
//!
//!   1. Header
//!   2. Bloom Filter
//!   3. Hash Buckets
//!   4. Hash Values
//!
//! The header has is an array of four (4) u32s:
//!
//!   1. nbuckets
//!   2. symndx
//!   3. maskwords
//!   4. shift2
//!
//! See: https://blogs.oracle.com/solaris/gnu-hash-elf-sections-v2

/// GNU hash function: takes a string and returns the u32 hash of that string
pub fn hash(symbol: &str) -> u32 {
    const HASH_SEED: u32 = 5381;
    let mut hash = HASH_SEED;
    for b in symbol.as_bytes() {
        hash = hash
            .wrapping_mul(33)
            .wrapping_add(u32::from(*b));
    }
    hash
}

#[cfg(test)]
mod tests {
    use super::hash;
    #[test]
    fn test_hash() {
        assert_eq!(hash("")             , 0x00001505);
        assert_eq!(hash("printf")       , 0x156b2bb8);
        assert_eq!(hash("exit")         , 0x7c967e3f);
        assert_eq!(hash("syscall")      , 0xbac212a0);
        assert_eq!(hash("flapenguin.me"), 0x8ae9f18e);
    }
}

macro_rules! elf_gnu_hash_impl {
    ($size:ty) => {

        use core::slice;
        use core::mem;
        use crate::strtab::Strtab;
        use super::sym;

        pub struct GnuHash<'process> {
            nbuckets: u32,
            symindex: usize,
            shift2: u32,
            maskbits: u32,
            bloomwords: &'process [$size], // either 32 or 64 bit masks, depending on platform
            maskwords_bitmask: u32,
            buckets: &'process [u32],
            hashvalues: &'process [u32],
            symtab: &'process [sym::Sym],
        }

        impl<'process> GnuHash<'process> {
            pub unsafe fn new(hashtab: *const u32, total_dynsyms: usize, symtab: &'process [sym::Sym]) -> GnuHash<'process> {
                let nbuckets = *hashtab;
                let symindex = *hashtab.add(1) as usize;
                let maskwords = *hashtab.add(2) as usize; // how many words our bloom filter mask has
                let shift2 = *hashtab.add(3);
                let bloomwords_ptr = hashtab.add(4) as *const $size;
                let buckets_ptr = bloomwords_ptr.add(maskwords) as *const u32;
                let buckets = slice::from_raw_parts(buckets_ptr, nbuckets as usize);
                let hashvalues_ptr = buckets_ptr.add(nbuckets as usize);
                let hashvalues = slice::from_raw_parts(hashvalues_ptr, total_dynsyms - symindex);
                let bloomwords = slice::from_raw_parts(bloomwords_ptr, maskwords);
                GnuHash {
                    nbuckets,
                    symindex,
                    shift2,
                    maskbits: mem::size_of::<usize>() as u32,
                    bloomwords,
                    hashvalues,
                    buckets,
                    maskwords_bitmask: ((maskwords as i32) - 1) as u32,
                    symtab,
                }
            }

            #[inline(always)]
            fn lookup(&self,
                      symbol: &str,
                      hash: u32,
                      strtab: &Strtab)
                      -> Option<&sym::Sym> {
                let mut idx = self.buckets[(hash % self.nbuckets) as usize] as usize;
                // println!("lookup idx = buckets[hash % nbuckets] = {}", idx);
                if idx == 0 {
                    return None;
                }
                let mut hash_idx = idx - self.symindex;
                let hash = hash & !1;
                // TODO: replace this with an iterator
                loop {
                    let symbol_ = &self.symtab[idx];
                    let h2 = self.hashvalues[hash_idx];
                    idx += 1;
                    hash_idx += 1;
                    let name = &strtab[symbol_.st_name as usize];
                    // println!("{}: h2 0x{:x} resolves to: {}", i, h2, name);
                    if hash == (h2 & !1) && name == symbol {
                        // println!("lookup match for {} at: 0x{:x}", symbol, symbol_.st_value);
                        return Some(symbol_);
                    }
                    if h2 & 1 == 1 {
                        break;
                    } // end of chain
                }
                None
            }

            #[inline(always)]
            fn filter(&self, hash: u32) -> bool {
                let bloom_idx = (hash / self.maskbits) & self.maskwords_bitmask;
                let h2 = hash >> self.shift2;
                let bitmask = (1u64 << (hash % self.maskbits)) | (1u64 << (h2 % self.maskbits));
                // println!("lookup: maskwords: {} bloom_idx: {} bitmask: {} shift2: {}", self.maskwords, bloom_idx, bitmask, self.shift2);
                let filter = self.bloomwords[bloom_idx as usize] as usize; // FIXME: verify this is safe ;)
                filter & (bitmask as usize) != (bitmask as usize) // if true, def _don't have_
            }

            /// Given a name, a hash of that name, a strtab to cross-reference names, maybe returns a Sym
            pub fn find(&self,
                        name: &str,
                        hash: u32,
                        strtab: &Strtab)
                        -> Option<&sym::Sym> {
                if self.filter(hash) {
                    None
                } else {
                    self.lookup(name, hash, strtab)
                }
            }
        }
    }
}
