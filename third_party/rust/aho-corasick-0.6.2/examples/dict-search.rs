// This example demonstrates how to use the Aho-Corasick algorithm to rapidly
// scan text for matches in a large dictionary of keywords. This example by
// default reads your system's dictionary (~120,000 words).
extern crate aho_corasick;
extern crate csv;
extern crate docopt;
extern crate memmap;
extern crate rustc_serialize;

use std::error::Error;
use std::fs::File;
use std::io::{self, BufRead, Write};
use std::process;

use aho_corasick::{Automaton, AcAutomaton, Match};
use docopt::Docopt;
use memmap::{Mmap, Protection};

static USAGE: &'static str = "
Usage: dict-search [options] <input>
       dict-search --help

Options:
    -d <path>, --dict <path>   Path to dictionary of keywords to search.
                               [default: /usr/share/dict/words]
    -m <len>, --min-len <len>  The minimum length for a keyword in UTF-8
                               encoded bytes. [default: 5]
    --overlapping              Report overlapping matches.
    -c, --count                Show only the numebr of matches.
    --memory-usage             Show memory usage of automaton.
    --full                     Use fully expanded transition matrix.
                               Warning: may use lots of memory.
    -h, --help                 Show this usage message.
";

#[derive(Clone, Debug, RustcDecodable)]
struct Args {
    arg_input: String,
    flag_dict: String,
    flag_min_len: usize,
    flag_overlapping: bool,
    flag_memory_usage: bool,
    flag_full: bool,
    flag_count: bool,
}

fn main() {
    let args: Args = Docopt::new(USAGE)
                            .and_then(|d| d.decode())
                            .unwrap_or_else(|e| e.exit());
    match run(&args) {
        Ok(()) => {}
        Err(err) => {
            writeln!(&mut io::stderr(), "{}", err).unwrap();
            process::exit(1);
        }
    }
}

fn run(args: &Args) -> Result<(), Box<Error>> {
    let aut = try!(build_automaton(&args.flag_dict, args.flag_min_len));
    if args.flag_memory_usage {
        let (bytes, states) = if args.flag_full {
            let aut = aut.into_full();
            (aut.heap_bytes(), aut.num_states())
        } else {
            (aut.heap_bytes(), aut.num_states())
        };
        println!("{} bytes, {} states", bytes, states);
        return Ok(());
    }

    if args.flag_full {
        let aut = aut.into_full();
        if args.flag_overlapping {
            if args.flag_count {
                let mmap = Mmap::open_path(
                    &args.arg_input, Protection::Read).unwrap();
                let text = unsafe { mmap.as_slice() };
                println!("{}", aut.find_overlapping(text).count());
            } else {
                let rdr = try!(File::open(&args.arg_input));
                try!(write_matches(&aut, aut.stream_find_overlapping(rdr)));
            }
        } else {
            if args.flag_count {
                let mmap = Mmap::open_path(
                    &args.arg_input, Protection::Read).unwrap();
                let text = unsafe { mmap.as_slice() };
                println!("{}", aut.find(text).count());
            } else {
                let rdr = try!(File::open(&args.arg_input));
                try!(write_matches(&aut, aut.stream_find(rdr)));
            }
        }
    } else {
        if args.flag_overlapping {
            if args.flag_count {
                let mmap = Mmap::open_path(
                    &args.arg_input, Protection::Read).unwrap();
                let text = unsafe { mmap.as_slice() };
                println!("{}", aut.find_overlapping(text).count());
            } else {
                let rdr = try!(File::open(&args.arg_input));
                try!(write_matches(&aut, aut.stream_find_overlapping(rdr)));
            }
        } else {
            if args.flag_count {
                let mmap = Mmap::open_path(
                    &args.arg_input, Protection::Read).unwrap();
                let text = unsafe { mmap.as_slice() };
                println!("{}", aut.find(text).count());
            } else {
                let rdr = try!(File::open(&args.arg_input));
                try!(write_matches(&aut, aut.stream_find(rdr)));
            }
        }
    }
    Ok(())
}

fn write_matches<A, I>(aut: &A, it: I) -> Result<(), Box<Error>>
        where A: Automaton<String>, I: Iterator<Item=io::Result<Match>> {
    let mut wtr = csv::Writer::from_writer(io::stdout());
    try!(wtr.write(["pattern", "start", "end"].iter()));
    for m in it {
        let m = try!(m);
        try!(wtr.write([
            aut.pattern(m.pati),
            &m.start.to_string(),
            &m.end.to_string(),
        ].iter()));
    }
    try!(wtr.flush());
    Ok(())
}

fn build_automaton(
    dict_path: &str,
    min_len: usize,
) -> Result<AcAutomaton<String>, Box<Error>> {
    let buf = io::BufReader::new(try!(File::open(dict_path)));
    let mut lines = Vec::with_capacity(1 << 10);
    for line in buf.lines() {
        let line = try!(line);
        if line.len() >= min_len {
            lines.push(line);
        }
    }
    Ok(AcAutomaton::with_transitions(lines))
}
