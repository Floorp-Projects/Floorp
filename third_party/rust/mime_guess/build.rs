extern crate phf_codegen;
extern crate unicase;

use phf_codegen::Map as PhfMap;

use unicase::UniCase;

use std::fs::File;
use std::io::prelude::*;
use std::io::BufWriter;
use std::path::Path;
use std::env;

use std::collections::BTreeMap;

use mime_types::MIME_TYPES;

#[path="src/mime_types.rs"]
mod mime_types;

fn main() {
    let out_dir = env::var("OUT_DIR").unwrap();
    let dest_path = Path::new(&out_dir).join("mime_types_generated.rs");
    let mut outfile = BufWriter::new(File::create(dest_path).unwrap());

    build_forward_map(&mut outfile);

    build_rev_map(&mut outfile);
}

// Build forward mappings (ext -> mime type)
fn build_forward_map<W: Write>(out: &mut W) {
    write!(out, "static MIME_TYPES: phf::Map<UniCase<&'static str>, &'static str> = ").unwrap();
    let mut forward_map = PhfMap::new();

    for &(key, val) in MIME_TYPES {
        forward_map.entry(UniCase(key), &format!("{:?}", val));
    }

    forward_map.build(out).unwrap();

    writeln!(out, ";").unwrap();
}


// Build reverse mappings (mime type -> ext)
fn build_rev_map<W: Write>(out: &mut W) {
    // First, collect all the mime type -> ext mappings)
    let mut dyn_map = BTreeMap::new();

    for &(key, val) in MIME_TYPES {
        let (top, sub) = split_mime(val);
        dyn_map.entry(UniCase(top))
            .or_insert_with(BTreeMap::new)
            .entry(UniCase(sub))
            .or_insert_with(Vec::new)
            .push(key);
    }

    write!(out, "static REV_MAPPINGS: phf::Map<UniCase<&'static str>, TopLevelExts> = ").unwrap();

    let mut rev_map = PhfMap::new();

    let mut exts = Vec::new();

    for (top, subs) in dyn_map {
        let top_start = exts.len();

        let mut sub_map = PhfMap::new();

        for(sub, sub_exts) in subs {
            let sub_start = exts.len();
            exts.extend(sub_exts);
            let sub_end = exts.len();

            sub_map.entry(sub, &format!("({}, {})", sub_start, sub_end));
        }

        let top_end = exts.len();

        let mut subs_bytes = Vec::<u8>::new();
        sub_map.build(&mut subs_bytes).unwrap();

        let subs_str = ::std::str::from_utf8(&subs_bytes).unwrap();

        rev_map.entry(
            top,
            &format!("TopLevelExts {{ start: {}, end: {}, subs: {} }}", top_start, top_end, subs_str)
        );
    }

    rev_map.build(out).unwrap();
    writeln!(out, ";").unwrap();

    writeln!(out, "const EXTS: &'static [&'static str] = &{:?};", exts).unwrap();
}

fn split_mime(mime: &str) -> (&str, &str) {
    let split_idx = mime.find('/').unwrap();
    (&mime[..split_idx], &mime[split_idx + 1 ..])
}
