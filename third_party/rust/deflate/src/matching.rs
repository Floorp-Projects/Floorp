use std::cmp;

use chained_hash_table::{ChainedHashTable, WINDOW_SIZE};
use huffman_table;

const MAX_MATCH: usize = huffman_table::MAX_MATCH as usize;
#[cfg(test)]
const MIN_MATCH: usize = huffman_table::MIN_MATCH as usize;


/// Get the length of the checked match
/// The function returns number of bytes at and including `current_pos` that are the same as the
/// ones at `pos_to_check`
#[inline]
pub fn get_match_length(data: &[u8], current_pos: usize, pos_to_check: usize) -> usize {
    // Unsafe version using unaligned loads for comparison.
    // Faster when benching the matching function alone,
    // but not as significant when running the full thing.
/*
    type Comp = u64;

    use std::mem::size_of;

    let max = cmp::min(data.len() - current_pos, MAX_MATCH);
    let mut left = max;
    let s = size_of::<Comp>();

    unsafe {
        let mut cur = data.as_ptr().offset(current_pos as isize);
        let mut tc = data.as_ptr().offset(pos_to_check as isize);
        while left >= s &&
              (*(cur as *const Comp) == *(tc as *const Comp)) {
                  left -= s;
                  cur = cur.offset(s as isize);
                  tc = tc.offset(s as isize);
              }
        while left > 0 && *cur == *tc {
            left -= 1;
            cur = cur.offset(1);
            tc = tc.offset(1);
        }
    }

    max - left
*/

    // Slightly faster than naive in single bench.
    // Does not use unaligned loads.
    // let l = cmp::min(MAX_MATCH, data.len() - current_pos);

    // let a = unsafe{&data.get_unchecked(current_pos..current_pos + l)};
    // let b = unsafe{&data.get_unchecked(pos_to_check..)};

    // let mut len = 0;

    // for (l, r) in a
    //     .iter()
    //     .zip(b.iter()) {
    //         if *l == *r {
    //             len += 1;
    //             continue;
    //         } else {
    //             break;
    //         }
    //     }
    // len as usize

    // Naive version
    data[current_pos..]
        .iter()
        .zip(data[pos_to_check..].iter())
        .take(MAX_MATCH)
        .take_while(|&(&a, &b)| a == b)
        .count()
}

/// Try finding the position and length of the longest match in the input data.
/// # Returns
/// (length, distance from position)
/// If no match is found that was better than `prev_length` or at all, or we are at the start,
/// the length value returned will be 2.
///
/// # Arguments:
/// `data`: The data to search in.
/// `hash_table`: Hash table to use for searching.
/// `position`: The position in the data to match against.
/// `prev_length`: The length of the previous `longest_match` check to compare against.
/// `max_hash_checks`: The maximum number of matching hash chain positions to check.
pub fn longest_match(
    data: &[u8],
    hash_table: &ChainedHashTable,
    position: usize,
    prev_length: usize,
    max_hash_checks: u16,
) -> (usize, usize) {

    // debug_assert_eq!(position, hash_table.current_head() as usize);

    // If we already have a match at the maximum length,
    // or we can't grow further, we stop here.
    if prev_length >= MAX_MATCH || position + prev_length >= data.len() {
        return (0, 0);
    }

    let limit = if position > WINDOW_SIZE {
        position - WINDOW_SIZE
    } else {
        0
    };

    // Make sure the length is at least one to simplify the matching code, as
    // otherwise the matching code might underflow.
    let prev_length = cmp::max(prev_length, 1);

    let max_length = cmp::min(data.len() - position, MAX_MATCH);

    // The position in the hash chain we are currently checking.
    let mut current_head = position;

    // The best match length we've found so far, and it's distance.
    let mut best_length = prev_length;
    let mut best_distance = 0;

    // The position of the previous value in the hash chain.
    let mut prev_head;

    for _ in 0..max_hash_checks {
        prev_head = current_head;
        current_head = hash_table.get_prev(current_head) as usize;
        if current_head >= prev_head || current_head < limit {
            // If the current hash chain value refers to itself, or is referring to
            // a value that's higher (we only move backwars through the chain),
            // we are at the end and can stop.
            break;
        }

        // We only check further if the match length can actually increase
        // Checking if the end byte and the potential next byte matches is generally
        // more likely to give a quick answer rather than checking from the start first, given
        // that the hashes match.
        // If there is no previous match, best_length will be 1 and the two first bytes will
        // be checked instead.
        // Since we've made sure best_length is always at least 1, this shouldn't underflow.
        if data[position + best_length - 1..position + best_length + 1] ==
            data[current_head + best_length - 1..current_head + best_length + 1]
        {
            // Actually check how many bytes match.
            // At the moment this will check the two bytes we just checked again,
            // though adding code for skipping these bytes may not result in any speed
            // gain due to the added complexity.
            let length = get_match_length(data, position, current_head);
            if length > best_length {
                best_length = length;
                best_distance = position - current_head;
                if length == max_length {
                    // We are at the max length, so there is no point
                    // searching any longer
                    break;
                }
            }
        }
    }

    if best_length > prev_length {
        (best_length, best_distance)
    } else {
        (0, 0)
    }
}

/// Try finding the position and length of the longest match in the input data using fast zlib
/// hash skipping algorithm.
/// # Returns
/// (length, distance from position)
/// If no match is found that was better than `prev_length` or at all, or we are at the start,
/// the length value returned will be 2.
///
/// # Arguments:
/// `data`: The data to search in.
/// `hash_table`: Hash table to use for searching.
/// `position`: The position in the data to match against.
/// `prev_length`: The length of the previous `longest_match` check to compare against.
/// `max_hash_checks`: The maximum number of matching hash chain positions to check.
#[cfg(test)]
pub fn longest_match_fast(
    data: &[u8],
    hash_table: &ChainedHashTable,
    position: usize,
    prev_length: usize,
    max_hash_checks: u16,
) -> (usize, usize) {

    // debug_assert_eq!(position, hash_table.current_head() as usize);

    // If we already have a match at the maximum length,
    // or we can't grow further, we stop here.
    if prev_length >= MAX_MATCH || position + prev_length >= data.len() {
        return (0, 0);
    }

    let limit = if position > WINDOW_SIZE {
        position - WINDOW_SIZE
    } else {
        0
    };

    // Make sure the length is at least one to simplify the matching code, as
    // otherwise the matching code might underflow.
    let prev_length = cmp::max(prev_length, 1);

    let max_length = cmp::min((data.len() - position), MAX_MATCH);

    // The position in the hash chain we are currently checking.
    let mut current_head = position;

    // The best match length we've found so far, and it's distance.
    let mut best_length = prev_length;
    let mut best_distance = 0;
    // The offset from the start of the match of the hash chain we are traversing.
    let mut offset = 0;

    // The position of the previous value in the hash chain.
    let mut prev_head;

    for _ in 0..max_hash_checks {
        prev_head = current_head;
        current_head = hash_table.get_prev(current_head) as usize;
        if current_head >= prev_head || current_head < limit + offset {
            // If the current hash chain value refers to itself, or is referring to
            // a value that's higher (we only move backwars through the chain),
            // we are at the end and can stop.
            break;
        }

        let offset_head = current_head - offset;

        // We only check further if the match length can actually increase
        // Checking if the end byte and the potential next byte matches is generally
        // more likely to give a quick answer rather than checking from the start first, given
        // that the hashes match.
        // If there is no previous match, best_length will be 1 and the two first bytes will
        // be checked instead.
        // Since we've made sure best_length is always at least 1, this shouldn't underflow.
        if data[position + best_length - 1..position + best_length + 1] ==
            data[offset_head + best_length - 1..offset_head + best_length + 1]
        {
            // Actually check how many bytes match.
            // At the moment this will check the two bytes we just checked again,
            // though adding code for skipping these bytes may not result in any speed
            // gain due to the added complexity.
            let length = get_match_length(data, position, offset_head);
            if length > best_length {
                best_length = length;
                best_distance = position - offset_head;
                if length == max_length {
                    // We are at the max length, so there is no point
                    // searching any longer
                    break;
                }

                // Find the position in the match where the next has position is the furthest away.
                // By moving to a different hash chain we can potentially skip a lot of checks,
                // saving time.
                // We avoid doing this for matches that extend past the starting position, as
                // those will contain positions that are not in the hash table yet.
                if best_distance > best_length {
                    offset = hash_table.farthest_next(offset_head, length);
                    current_head = offset_head + offset;
                }
            }
        }
    }

    if best_length > prev_length {
        (best_length, best_distance)
    } else {
        (0, 0)
    }
}

// Get the longest match from the current position of the hash table.
#[inline]
#[cfg(test)]
pub fn longest_match_current(data: &[u8], hash_table: &ChainedHashTable) -> (usize, usize) {
    use compression_options::MAX_HASH_CHECKS;
    longest_match(
        data,
        hash_table,
        hash_table.current_head() as usize,
        MIN_MATCH as usize - 1,
        MAX_HASH_CHECKS,
    )
}

#[cfg(test)]
mod test {
    use chained_hash_table::{filled_hash_table, HASH_BYTES, ChainedHashTable};
    use super::{get_match_length, longest_match, longest_match_fast};

    /// Test that match lengths are calculated correctly
    #[test]
    fn match_length() {
        let test_arr = [5u8, 5, 5, 5, 5, 9, 9, 2, 3, 5, 5, 5, 5, 5];
        let l = get_match_length(&test_arr, 9, 0);
        assert_eq!(l, 5);
        let l2 = get_match_length(&test_arr, 9, 7);
        assert_eq!(l2, 0);
        let l3 = get_match_length(&test_arr, 10, 0);
        assert_eq!(l3, 4);
    }

    /// Test that we get the longest of the matches
    #[test]
    fn get_longest_match() {
        let test_data = b"xTest data, Test_data,zTest data";
        let hash_table = filled_hash_table(&test_data[..23 + 1 + HASH_BYTES - 1]);

        let (length, distance) = super::longest_match_current(test_data, &hash_table);

        // We check that we get the longest match, rather than the shorter, but closer one.
        assert_eq!(distance, 22);
        assert_eq!(length, 9);
        let test_arr2 = [
            10u8,
            10,
            10,
            10,
            10,
            10,
            10,
            10,
            2,
            3,
            5,
            10,
            10,
            10,
            10,
            10,
        ];
        let hash_table = filled_hash_table(&test_arr2[..HASH_BYTES + 1 + 1 + 2]);
        let (length, distance) = super::longest_match_current(&test_arr2, &hash_table);

        assert_eq!(distance, 1);
        assert_eq!(length, 4);
    }

    /// Make sure we can get a match at index zero
    #[test]
    fn match_index_zero() {
        let test_data = b"AAAAAAA";

        let mut hash_table = ChainedHashTable::from_starting_values(test_data[0], test_data[1]);
        for (n, &b) in test_data[2..5].iter().enumerate() {
            hash_table.add_hash_value(n, b);
        }

        let (match_length, match_dist) = longest_match(test_data, &hash_table, 1, 0, 4096);

        assert_eq!(match_dist, 1);
        assert!(match_length == 6);
    }

    /// Test for fast_zlib algorithm.
    /// Check that it doesn't give worse matches than the default one.
    /// ignored by default as it's slow, and best ran in release mode.
    #[ignore]
    #[test]
    fn fast_match_at_least_equal() {
        use test_utils::get_test_data;
        for start_pos in 10000..50000 {
            const NUM_CHECKS: u16 = 400;
            let data = get_test_data();
            let hash_table = filled_hash_table(&data[..start_pos + 1]);
            let pos = hash_table.current_head() as usize;

            let naive_match = longest_match(&data[..], &hash_table, pos, 0, NUM_CHECKS);
            let fast_match = longest_match_fast(&data[..], &hash_table, pos, 0, NUM_CHECKS);

            if fast_match.0 > naive_match.0 {
                println!("Fast match found better match!");
            }

            assert!(fast_match.0 >= naive_match.0,
                    "naive match had better length! start_pos: {}, naive: {:?}, fast {:?}"
                    , start_pos, naive_match, fast_match);
            assert!(fast_match.1 >= naive_match.1,
                "naive match had better dist! start_pos: {} naive {:?}, fast {:?}"
                    , start_pos, naive_match, fast_match);
        }

    }
}


#[cfg(all(test, feature = "benchmarks"))]
mod bench {
    use test_std::Bencher;
    use test_utils::get_test_data;
    use chained_hash_table::filled_hash_table;
    use super::{longest_match, longest_match_fast};
    #[bench]
    fn matching(b: &mut Bencher) {
        const POS: usize = 29000;
        let data = get_test_data();
        let hash_table = filled_hash_table(&data[..POS + 1]);
        let pos = hash_table.current_head() as usize;
        println!("M: {:?}", longest_match(&data[..], &hash_table, pos, 0, 4096));
        b.iter( ||
          longest_match(&data[..], &hash_table, pos, 0, 4096)
        );
    }

    #[bench]
    fn fast_matching(b: &mut Bencher) {
        const POS: usize = 29000;
        let data = get_test_data();
        let hash_table = filled_hash_table(&data[..POS + 1]);
        let pos = hash_table.current_head() as usize;
        println!("M: {:?}", longest_match_fast(&data[..], &hash_table, pos, 0, 4096));
        b.iter( ||
          longest_match_fast(&data[..], &hash_table, pos, 0, 4096)
        );
    }
}
