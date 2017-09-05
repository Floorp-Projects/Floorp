// The Computer Language Benchmarks Game
// http://benchmarksgame.alioth.debian.org/
//
// regex-dna program contributed by the Rust Project Developers
// contributed by BurntSushi
// contributed by TeXitoi
// converted from regex-dna program

extern crate regex;

use std::io::{self, Read};
use std::sync::Arc;
use std::thread;

macro_rules! regex { ($re:expr) => { ::regex::bytes::Regex::new($re).unwrap() } }

fn main() {
    let squash = regex!("(?m)^>.*$|\n");
    let variants = Arc::new(vec![
        regex!("agggtaaa|tttaccct"),
        regex!("[cgt]gggtaaa|tttaccc[acg]"),
        regex!("a[act]ggtaaa|tttacc[agt]t"),
        regex!("ag[act]gtaaa|tttac[agt]ct"),
        regex!("agg[act]taaa|ttta[agt]cct"),
        regex!("aggg[acg]aaa|ttt[cgt]ccct"),
        regex!("agggt[cgt]aa|tt[acg]accct"),
        regex!("agggta[cgt]a|t[acg]taccct"),
        regex!("agggtaa[cgt]|[acg]ttaccct"),
    ]);
    let substs = Arc::new(vec![
        (regex!("tHa[Nt]"), &b"<4>"[..]),
        (regex!("aND|caN|Ha[DS]|WaS"), &b"<3>"[..]),
        (regex!("a[NSt]|BY"), &b"<2>"[..]),
        (regex!("<[^>]*>"), &b"|"[..]),
        (regex!("\\|[^|][^|]*\\|"), &b"-"[..]),
    ]);

    let mut seq = Vec::with_capacity(51 * (1 << 20));
    io::stdin().read_to_end(&mut seq).unwrap();
    let ilen = seq.len();

    let seq = Arc::new(squash.replace_all(&seq, &b""[..]).into_owned());
    let clen = seq.len();
    let mut workers = vec![];
    let njobs = 4;
    let per = variants.len() / njobs;
    for i in 0..njobs {
        let seq = seq.clone();
        let variants = variants.clone();
        let substs = substs.clone();
        workers.push(thread::spawn(move || {
            let (start, end) = (i * per, i * per + per);
            let mut var_counts = vec![];
            for variant in &variants[start..end] {
                var_counts.push((
                    variant.to_string(),
                    variant.find_iter(&seq).count(),
                ));
            }
            let chunk_len = seq.len() / njobs;
            let mut chunk = substs[0].0.replace_all(
                &seq[i * chunk_len..i * chunk_len + chunk_len],
                substs[0].1,
            ).into_owned();
            for &(ref re, ref replacement) in substs.iter().skip(1) {
                chunk = re.replace_all(&chunk, &**replacement).into_owned();
            }
            (var_counts, chunk.len())
        }));
    }
    let last_var = variants[8].find_iter(&seq).count();
    let mut seq_len = 0;
    for worker in workers {
        let (var_counts, chunk_len) = worker.join().unwrap();
        seq_len += chunk_len;
        for (variant, count) in var_counts {
            println!("{} {}", variant, count);
        }
    }
    println!("{} {}", variants[8].to_string(), last_var);
    println!("\n{}\n{}\n{}", ilen, clen, seq_len);
}
