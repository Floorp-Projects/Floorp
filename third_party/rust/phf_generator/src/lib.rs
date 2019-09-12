#![doc(html_root_url="https://docs.rs/phf_generator/0.7")]
extern crate phf_shared;
extern crate rand;

use phf_shared::PhfHash;
use rand::{SeedableRng, Rng};
use rand::rngs::SmallRng;

const DEFAULT_LAMBDA: usize = 5;

const FIXED_SEED: u64 = 1234567890;

pub struct HashState {
    pub key: u64,
    pub disps: Vec<(u32, u32)>,
    pub map: Vec<usize>,
}

pub fn generate_hash<H: PhfHash>(entries: &[H]) -> HashState {
    let mut rng = SmallRng::seed_from_u64(FIXED_SEED);
    loop {
        if let Some(s) = try_generate_hash(entries, &mut rng) {
            return s;
        }
    }
}

fn try_generate_hash<H: PhfHash>(entries: &[H], rng: &mut SmallRng) -> Option<HashState> {
    struct Bucket {
        idx: usize,
        keys: Vec<usize>,
    }

    struct Hashes {
        g: u32,
        f1: u32,
        f2: u32,
    }

    let key = rng.gen();

    let hashes: Vec<_> = entries.iter()
                                .map(|entry| {
                                    let hash = phf_shared::hash(entry, key);
                                    let (g, f1, f2) = phf_shared::split(hash);
                                    Hashes {
                                        g: g,
                                        f1: f1,
                                        f2: f2,
                                    }
                                })
                                .collect();

    let buckets_len = (entries.len() + DEFAULT_LAMBDA - 1) / DEFAULT_LAMBDA;
    let mut buckets = (0..buckets_len)
                          .map(|i| {
                              Bucket {
                                  idx: i,
                                  keys: vec![],
                              }
                          })
                          .collect::<Vec<_>>();

    for (i, hash) in hashes.iter().enumerate() {
        buckets[(hash.g % (buckets_len as u32)) as usize].keys.push(i);
    }

    // Sort descending
    buckets.sort_by(|a, b| a.keys.len().cmp(&b.keys.len()).reverse());

    let table_len = entries.len();
    let mut map = vec![None; table_len];
    let mut disps = vec![(0u32, 0u32); buckets_len];

    // store whether an element from the bucket being placed is
    // located at a certain position, to allow for efficient overlap
    // checks. It works by storing the generation in each cell and
    // each new placement-attempt is a new generation, so you can tell
    // if this is legitimately full by checking that the generations
    // are equal. (A u64 is far too large to overflow in a reasonable
    // time for current hardware.)
    let mut try_map = vec![0u64; table_len];
    let mut generation = 0u64;

    // the actual values corresponding to the markers above, as
    // (index, key) pairs, for adding to the main map once we've
    // chosen the right disps.
    let mut values_to_add = vec![];

    'buckets: for bucket in &buckets {
        for d1 in 0..(table_len as u32) {
            'disps: for d2 in 0..(table_len as u32) {
                values_to_add.clear();
                generation += 1;

                for &key in &bucket.keys {
                    let idx = (phf_shared::displace(hashes[key].f1, hashes[key].f2, d1, d2) %
                               (table_len as u32)) as usize;
                    if map[idx].is_some() || try_map[idx] == generation {
                        continue 'disps;
                    }
                    try_map[idx] = generation;
                    values_to_add.push((idx, key));
                }

                // We've picked a good set of disps
                disps[bucket.idx] = (d1, d2);
                for &(idx, key) in &values_to_add {
                    map[idx] = Some(key);
                }
                continue 'buckets;
            }
        }

        // Unable to find displacements for a bucket
        return None;
    }

    Some(HashState {
        key: key,
        disps: disps,
        map: map.into_iter().map(|i| i.unwrap()).collect(),
    })
}
