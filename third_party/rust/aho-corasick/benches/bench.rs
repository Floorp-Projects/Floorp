#![feature(test)]

extern crate aho_corasick;
extern crate test;

use std::iter;

use aho_corasick::{Automaton, AcAutomaton, Transitions};
use test::Bencher;

const HAYSTACK_RANDOM: &'static str = include_str!("random.txt");
const HAYSTACK_SHERLOCK: &'static str = include_str!("sherlock.txt");

fn bench_aut_no_match<P: AsRef<[u8]>, T: Transitions>(
    b: &mut Bencher,
    aut: AcAutomaton<P, T>,
    haystack: &str,
) {
    b.bytes = haystack.len() as u64;
    b.iter(|| assert!(aut.find(haystack).next().is_none()));
}

fn bench_box_aut_no_match<P: AsRef<[u8]>, T: Transitions>(
    b: &mut Bencher,
    aut: AcAutomaton<P, T>,
    haystack: &str,
) {
    b.bytes = haystack.len() as u64;
    let aut: &Automaton<P> = &aut;
    b.iter(|| assert!(Automaton::find(&aut, haystack).next().is_none()));
}

fn bench_full_aut_no_match<P: AsRef<[u8]>, T: Transitions>(
    b: &mut Bencher,
    aut: AcAutomaton<P, T>,
    haystack: &str,
) {
    let aut = aut.into_full();
    b.bytes = haystack.len() as u64;
    b.iter(|| assert!(aut.find(haystack).next().is_none()));
}

fn bench_full_aut_overlapping_no_match<P: AsRef<[u8]>, T: Transitions>(
    b: &mut Bencher,
    aut: AcAutomaton<P, T>,
    haystack: &str,
) {
    let aut = aut.into_full();
    b.bytes = haystack.len() as u64;
    b.iter(|| assert!(aut.find_overlapping(haystack).count() == 0));
}

fn bench_naive_no_match<S>(b: &mut Bencher, needles: Vec<S>, haystack: &str)
        where S: Into<String> {
    b.bytes = haystack.len() as u64;
    let needles: Vec<String> = needles.into_iter().map(Into::into).collect();
    b.iter(|| assert!(!naive_find(&needles, haystack)));
}

#[bench]
fn bench_construction(b: &mut Bencher) {
    b.iter(|| {
        AcAutomaton::new(test::black_box(
            [
                "ADL", "ADl", "AdL", "Adl", "BAK", "BAk", "BAK", "BaK", "Bak", "BaK", "HOL",
                "HOl", "HoL", "Hol", "IRE", "IRe", "IrE", "Ire", "JOH", "JOh", "JoH", "Joh", "SHE",
                "SHe", "ShE", "She", "WAT", "WAt", "WaT", "Wat", "aDL", "aDl", "adL", "adl", "bAK",
                "bAk", "bAK", "baK", "bak", "baK", "hOL", "hOl", "hoL", "hol", "iRE", "iRe",
                "irE", "ire", "jOH", "jOh", "joH", "joh", "sHE", "sHe", "shE", "she", "wAT", "wAt",
                "waT", "wat", "ſHE", "ſHe", "ſhE", "ſhe",
            ].iter()
                .map(|x| *x),
        ))
    })
}

#[bench]
fn bench_full_construction(b: &mut Bencher) {
    b.iter(|| {
        AcAutomaton::new(test::black_box(
            [
                "ADL", "ADl", "AdL", "Adl", "BAK", "BAk", "BAK", "BaK", "Bak", "BaK", "HOL",
                "HOl", "HoL", "Hol", "IRE", "IRe", "IrE", "Ire", "JOH", "JOh", "JoH", "Joh", "SHE",
                "SHe", "ShE", "She", "WAT", "WAt", "WaT", "Wat", "aDL", "aDl", "adL", "adl", "bAK",
                "bAk", "bAK", "baK", "bak", "baK", "hOL", "hOl", "hoL", "hol", "iRE", "iRe",
                "irE", "ire", "jOH", "jOh", "joH", "joh", "sHE", "sHe", "shE", "she", "wAT", "wAt",
                "waT", "wat", "ſHE", "ſHe", "ſhE", "ſhe",
            ].iter()
                .map(|x| *x),
        )).into_full()
    })
}

fn haystack_same(letter: char) -> String {
    iter::repeat(letter).take(10000).collect()
}

macro_rules! aut_benches {
    ($prefix:ident, $aut:expr, $bench:expr) => {
        mod $prefix {
#![allow(unused_imports)]
use aho_corasick::{Automaton, AcAutomaton, Sparse};
use test::Bencher;

use super::{
    HAYSTACK_RANDOM, haystack_same,
    bench_aut_no_match, bench_box_aut_no_match,
    bench_full_aut_no_match, bench_full_aut_overlapping_no_match,
};

#[bench]
fn ac_one_byte(b: &mut Bencher) {
    let aut = $aut(vec!["a"]);
    $bench(b, aut, &haystack_same('z'));
}

#[bench]
fn ac_one_prefix_byte_no_match(b: &mut Bencher) {
    let aut = $aut(vec!["zbc"]);
    $bench(b, aut, &haystack_same('y'));
}

#[bench]
fn ac_one_prefix_byte_every_match(b: &mut Bencher) {
    // We lose the benefit of `memchr` because the first byte matches
    // in every position in the haystack.
    let aut = $aut(vec!["zbc"]);
    $bench(b, aut, &haystack_same('z'));
}

#[bench]
fn ac_one_prefix_byte_random(b: &mut Bencher) {
    let aut = $aut(vec!["zbc\x00"]);
    $bench(b, aut, HAYSTACK_RANDOM);
}

#[bench]
fn ac_two_bytes(b: &mut Bencher) {
    let aut = $aut(vec!["a", "b"]);
    $bench(b, aut, &haystack_same('z'));
}

#[bench]
fn ac_two_diff_prefix(b: &mut Bencher) {
    let aut = $aut(vec!["abcdef", "bmnopq"]);
    $bench(b, aut, &haystack_same('z'));
}

#[bench]
fn ac_two_one_prefix_byte_every_match(b: &mut Bencher) {
    let aut = $aut(vec!["zbcdef", "zmnopq"]);
    $bench(b, aut, &haystack_same('z'));
}

#[bench]
fn ac_two_one_prefix_byte_no_match(b: &mut Bencher) {
    let aut = $aut(vec!["zbcdef", "zmnopq"]);
    $bench(b, aut, &haystack_same('y'));
}

#[bench]
fn ac_two_one_prefix_byte_random(b: &mut Bencher) {
    let aut = $aut(vec!["zbcdef\x00", "zmnopq\x00"]);
    $bench(b, aut, HAYSTACK_RANDOM);
}

#[bench]
fn ac_ten_bytes(b: &mut Bencher) {
    let aut = $aut(vec!["a", "b", "c", "d", "e",
                        "f", "g", "h", "i", "j"]);
    $bench(b, aut, &haystack_same('z'));
}

#[bench]
fn ac_ten_diff_prefix(b: &mut Bencher) {
    let aut = $aut(vec!["abcdef", "bbcdef", "cbcdef", "dbcdef",
                        "ebcdef", "fbcdef", "gbcdef", "hbcdef",
                        "ibcdef", "jbcdef"]);
    $bench(b, aut, &haystack_same('z'));
}

#[bench]
fn ac_ten_one_prefix_byte_every_match(b: &mut Bencher) {
    let aut = $aut(vec!["zacdef", "zbcdef", "zccdef", "zdcdef",
                        "zecdef", "zfcdef", "zgcdef", "zhcdef",
                        "zicdef", "zjcdef"]);
    $bench(b, aut, &haystack_same('z'));
}

#[bench]
fn ac_ten_one_prefix_byte_no_match(b: &mut Bencher) {
    let aut = $aut(vec!["zacdef", "zbcdef", "zccdef", "zdcdef",
                        "zecdef", "zfcdef", "zgcdef", "zhcdef",
                        "zicdef", "zjcdef"]);
    $bench(b, aut, &haystack_same('y'));
}

#[bench]
fn ac_ten_one_prefix_byte_random(b: &mut Bencher) {
    let aut = $aut(vec!["zacdef\x00", "zbcdef\x00", "zccdef\x00",
                        "zdcdef\x00", "zecdef\x00", "zfcdef\x00",
                        "zgcdef\x00", "zhcdef\x00", "zicdef\x00",
                        "zjcdef\x00"]);
    $bench(b, aut, HAYSTACK_RANDOM);
}
        }
    }
}

aut_benches!(dense, AcAutomaton::new, bench_aut_no_match);
aut_benches!(dense_boxed, AcAutomaton::new, bench_box_aut_no_match);
aut_benches!(sparse, AcAutomaton::<&str, Sparse>::with_transitions,
             bench_aut_no_match);
aut_benches!(full, AcAutomaton::new, bench_full_aut_no_match);
aut_benches!(full_overlap, AcAutomaton::new, bench_full_aut_overlapping_no_match);

// A naive multi-pattern search.
// We use this to benchmark *throughput*, so it should never match anything.
fn naive_find(needles: &[String], haystack: &str) -> bool {
    for hi in 0..haystack.len() {
        let rest = &haystack.as_bytes()[hi..];
        for needle in needles {
            let needle = needle.as_bytes();
            if needle.len() > rest.len() {
                continue;
            }
            if needle == &rest[..needle.len()] {
                // should never happen in throughput benchmarks.
                return true;
            }
        }
    }
    false
}

#[bench]
fn naive_one_byte(b: &mut Bencher) {
    bench_naive_no_match(b, vec!["a"], &haystack_same('z'));
}

#[bench]
fn naive_one_prefix_byte_no_match(b: &mut Bencher) {
    bench_naive_no_match(b, vec!["zbc"], &haystack_same('y'));
}

#[bench]
fn naive_one_prefix_byte_every_match(b: &mut Bencher) {
    bench_naive_no_match(b, vec!["zbc"], &haystack_same('z'));
}

#[bench]
fn naive_one_prefix_byte_random(b: &mut Bencher) {
    bench_naive_no_match(b, vec!["zbc\x00"], HAYSTACK_RANDOM);
}

#[bench]
fn naive_two_bytes(b: &mut Bencher) {
    bench_naive_no_match(b, vec!["a", "b"], &haystack_same('z'));
}

#[bench]
fn naive_two_diff_prefix(b: &mut Bencher) {
    bench_naive_no_match(b, vec!["abcdef", "bmnopq"], &haystack_same('z'));
}

#[bench]
fn naive_two_one_prefix_byte_every_match(b: &mut Bencher) {
    bench_naive_no_match(b, vec!["zbcdef", "zmnopq"], &haystack_same('z'));
}

#[bench]
fn naive_two_one_prefix_byte_no_match(b: &mut Bencher) {
    bench_naive_no_match(b, vec!["zbcdef", "zmnopq"], &haystack_same('y'));
}

#[bench]
fn naive_two_one_prefix_byte_random(b: &mut Bencher) {
    bench_naive_no_match(b, vec!["zbcdef\x00", "zmnopq\x00"], HAYSTACK_RANDOM);
}

#[bench]
fn naive_ten_bytes(b: &mut Bencher) {
    let needles = vec!["a", "b", "c", "d", "e",
                       "f", "g", "h", "i", "j"];
    bench_naive_no_match(b, needles, &haystack_same('z'));
}

#[bench]
fn naive_ten_diff_prefix(b: &mut Bencher) {
    let needles = vec!["abcdef", "bbcdef", "cbcdef", "dbcdef",
                       "ebcdef", "fbcdef", "gbcdef", "hbcdef",
                       "ibcdef", "jbcdef"];
    bench_naive_no_match(b, needles, &haystack_same('z'));
}

#[bench]
fn naive_ten_one_prefix_byte_every_match(b: &mut Bencher) {
    let needles = vec!["zacdef", "zbcdef", "zccdef", "zdcdef",
                       "zecdef", "zfcdef", "zgcdef", "zhcdef",
                       "zicdef", "zjcdef"];
    bench_naive_no_match(b, needles, &haystack_same('z'));
}

#[bench]
fn naive_ten_one_prefix_byte_no_match(b: &mut Bencher) {
    let needles = vec!["zacdef", "zbcdef", "zccdef", "zdcdef",
                       "zecdef", "zfcdef", "zgcdef", "zhcdef",
                       "zicdef", "zjcdef"];
    bench_naive_no_match(b, needles, &haystack_same('y'));
}

#[bench]
fn naive_ten_one_prefix_byte_random(b: &mut Bencher) {
    let needles = vec!["zacdef\x00", "zbcdef\x00", "zccdef\x00",
                       "zdcdef\x00", "zecdef\x00", "zfcdef\x00",
                       "zgcdef\x00", "zhcdef\x00", "zicdef\x00",
                       "zjcdef\x00"];
    bench_naive_no_match(b, needles, HAYSTACK_RANDOM);
}


// The organization above is just awful. Let's start over...

mod sherlock {
    use aho_corasick::{Automaton, AcAutomaton};
    use test::Bencher;
    use super::HAYSTACK_SHERLOCK;

    macro_rules! sherlock {
        ($name:ident, $count:expr, $pats:expr) => {
            #[bench]
            fn $name(b: &mut Bencher) {
                let haystack = HAYSTACK_SHERLOCK;
                let aut = AcAutomaton::new($pats).into_full();
                b.bytes = haystack.len() as u64;
                b.iter(|| assert_eq!($count, aut.find(haystack).count()));
            }
        }
    }

    sherlock!(name_alt1, 158, vec!["Sherlock", "Street"]);

    sherlock!(name_alt2, 558, vec!["Sherlock", "Holmes"]);

    sherlock!(name_alt3, 740, vec![
        "Sherlock", "Holmes", "Watson", "Irene", "Adler", "John", "Baker",
    ]);

    sherlock!(name_alt3_nocase, 1764, vec![
        "ADL", "ADl", "AdL", "Adl", "BAK", "BAk", "BAK", "BaK", "Bak", "BaK",
        "HOL", "HOl", "HoL", "Hol", "IRE", "IRe", "IrE", "Ire", "JOH", "JOh",
        "JoH", "Joh", "SHE", "SHe", "ShE", "She", "WAT", "WAt", "WaT", "Wat",
        "aDL", "aDl", "adL", "adl", "bAK", "bAk", "bAK", "baK", "bak", "baK",
        "hOL", "hOl", "hoL", "hol", "iRE", "iRe", "irE", "ire", "jOH", "jOh",
        "joH", "joh", "sHE", "sHe", "shE", "she", "wAT", "wAt", "waT", "wat",
        "ſHE", "ſHe", "ſhE", "ſhe",
    ]);

    sherlock!(name_alt4, 582, vec!["Sher", "Hol"]);

    sherlock!(name_alt4_nocase, 1307, vec![
        "HOL", "HOl", "HoL", "Hol", "SHE", "SHe", "ShE", "She", "hOL", "hOl",
        "hoL", "hol", "sHE", "sHe", "shE", "she", "ſHE", "ſHe", "ſhE", "ſhe",
    ]);

    sherlock!(name_alt5, 639, vec!["Sherlock", "Holmes", "Watson"]);

    sherlock!(name_alt5_nocase, 1442, vec![
        "HOL", "HOl", "HoL", "Hol", "SHE", "SHe", "ShE", "She", "WAT", "WAt",
        "WaT", "Wat", "hOL", "hOl", "hoL", "hol", "sHE", "sHe", "shE", "she",
        "wAT", "wAt", "waT", "wat", "ſHE", "ſHe", "ſhE", "ſhe",
    ]);
}
