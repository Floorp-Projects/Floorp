// The Computer Language Benchmarks Game
// http://benchmarksgame.alioth.debian.org/
//
// regex-dna program contributed by the Rust Project Developers
// contributed by BurntSushi
// contributed by TeXitoi
// converted from regex-dna program
// contributed by Matt Brubeck

extern crate regex;

use std::fs::File;
use std::io::{self, Read};
use std::sync::Arc;
use std::thread;

use regex::bytes::Regex;

macro_rules! regex { ($re:expr) => { Regex::new($re).unwrap() } }

/// Read the input into memory.
fn read() -> io::Result<Vec<u8>> {
    // Pre-allocate a buffer based on the input file size.
    let mut stdin = File::open("/dev/stdin")?;
    let size = stdin.metadata()?.len() as usize;
    let mut buf = Vec::with_capacity(size + 1);

    stdin.read_to_end(&mut buf)?;
    Ok(buf)
}

/// Perform a replacement into the given buffer.
fn replace(
    re: &Regex,
    haystack: &[u8],
    replacement: &[u8],
    dst: &mut Vec<u8>,
) {
    dst.clear();
    let mut last_match = 0;
    for m in re.find_iter(haystack) {
        dst.extend_from_slice(&haystack[last_match..m.start()]);
        dst.extend_from_slice(replacement);
        last_match = m.end();
    }
    dst.extend_from_slice(&haystack[last_match..]);
}

fn main() {
    let mut seq2 = read().unwrap();
    let ilen = seq2.len();

    // Remove headers and newlines.
    let squash = regex!(r"(?m)^>.*$|\n");
    let seq = Arc::new(squash.replace_all(&seq2, &b""[..]).into_owned());
    let clen = seq.len();

    // Search for occurrences of the following patterns:
    let variants = vec![
        regex!("agggtaaa|tttaccct"),
        regex!("[cgt]gggtaaa|tttaccc[acg]"),
        regex!("a[act]ggtaaa|tttacc[agt]t"),
        regex!("ag[act]gtaaa|tttac[agt]ct"),
        regex!("agg[act]taaa|ttta[agt]cct"),
        regex!("aggg[acg]aaa|ttt[cgt]ccct"),
        regex!("agggt[cgt]aa|tt[acg]accct"),
        regex!("agggta[cgt]a|t[acg]taccct"),
        regex!("agggtaa[cgt]|[acg]ttaccct"),
    ];

    // Count each pattern in parallel.  Use an Arc (atomic reference-counted
    // pointer) to share the sequence between threads without copying it.
    let mut counts = vec![];
    for variant in variants {
        let seq = seq.clone();
        let restr = variant.to_string();
        let future = thread::spawn(move || variant.find_iter(&seq).count());
        counts.push((restr, future));
    }
    // Print the counts.
    for (variant, count) in counts {
        println!("{} {}", variant, count.join().unwrap());
    }
    let mut seq1 = Arc::try_unwrap(seq).unwrap();

    // Replace the following patterns, one at a time:
    let substs = vec![
        (regex!(r"tHa[Nt]"), &b"<4>"[..]),
        (regex!(r"aND|caN|Ha[DS]|WaS"), &b"<3>"[..]),
        (regex!(r"a[NSt]|BY"), &b"<2>"[..]),
        (regex!(r"<[^>]*>"), &b"|"[..]),
        (regex!(r"\|[^|][^|]*\|"), &b"-"[..]),
    ];

    // Perform the replacements in sequence:
    for (re, replacement) in substs {
        replace(&re, &seq1, replacement, &mut seq2);
        ::std::mem::swap(&mut seq1, &mut seq2);
    }

    // Print the final results:
    println!("\n{}\n{}\n{}", ilen, clen, seq1.len());
}
