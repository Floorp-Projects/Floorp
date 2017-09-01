// The Computer Language Benchmarks Game
// http://benchmarksgame.alioth.debian.org/
//
// regex-dna program contributed by the Rust Project Developers
// contributed by BurntSushi
// contributed by TeXitoi
// converted from regex-dna program
// contributed by Matt Brubeck

extern crate regex;

use std::borrow::Cow;
use std::fs::File;
use std::io::{self, Read};
use std::sync::Arc;
use std::thread;

macro_rules! regex { ($re:expr) => { ::regex::bytes::Regex::new($re).unwrap() } }

/// Read the input into memory.
fn read() -> io::Result<Vec<u8>> {
    // Pre-allocate a buffer based on the input file size.
    let mut stdin = File::open("/dev/stdin")?;
    let size = stdin.metadata()?.len() as usize;
    let mut buf = Vec::with_capacity(size + 1);

    stdin.read_to_end(&mut buf)?;
    Ok(buf)
}

fn main() {
    let mut seq = read().unwrap();
    let ilen = seq.len();

    // Remove headers and newlines.
    seq = regex!(r"(?m)^>.*$|\n").replace_all(&seq, &b""[..]).into_owned();
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
    let seq_arc = Arc::new(seq);
    let mut counts = vec![];
    for variant in variants {
        let seq = seq_arc.clone();
        let restr = variant.to_string();
        let future = thread::spawn(move || variant.find_iter(&seq).count());
        counts.push((restr, future));
    }

    // Replace the following patterns, one at a time:
    let substs = vec![
        (regex!("tHa[Nt]"), &b"<4>"[..]),
        (regex!("aND|caN|Ha[DS]|WaS"), &b"<3>"[..]),
        (regex!("a[NSt]|BY"), &b"<2>"[..]),
        (regex!("<[^>]*>"), &b"|"[..]),
        (regex!("\\|[^|][^|]*\\|"), &b"-"[..]),
    ];

    // Use Cow here to avoid one extra copy of the sequence, by borrowing from
    // the Arc during the first iteration.
    let mut seq = Cow::Borrowed(&seq_arc[..]);

    // Perform the replacements in sequence:
    for (re, replacement) in substs {
        seq = Cow::Owned(re.replace_all(&seq, replacement).into_owned());
    }

    // Print the results:
    for (variant, count) in counts {
        println!("{} {}", variant, count.join().unwrap());
    }
    println!("\n{}\n{}\n{}", ilen, clen, seq.len());
}
