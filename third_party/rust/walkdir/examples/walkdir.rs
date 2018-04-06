extern crate docopt;
extern crate serde;
#[macro_use]
extern crate serde_derive;
extern crate walkdir;

use std::io::{self, Write};

use docopt::Docopt;
use walkdir::WalkDir;

const USAGE: &'static str = "
Usage:
    walkdir [options] [<dir>]

Options:
    -h, --help
    -L, --follow-links   Follow symlinks.
    --min-depth NUM      Minimum depth.
    --max-depth NUM      Maximum depth.
    -n, --fd-max NUM     Maximum open file descriptors. [default: 32]
    --tree               Show output as a tree.
    --sort               Sort the output.
    -q, --ignore-errors  Ignore errors.
    -d, --depth          Show directory's contents before the directory itself.
";

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
struct Args {
    arg_dir: Option<String>,
    flag_follow_links: bool,
    flag_min_depth: Option<usize>,
    flag_max_depth: Option<usize>,
    flag_fd_max: usize,
    flag_tree: bool,
    flag_ignore_errors: bool,
    flag_sort: bool,
    flag_depth: bool,
}

macro_rules! wout { ($($tt:tt)*) => { {writeln!($($tt)*)}.unwrap() } }

fn main() {
    let args: Args = Docopt::new(USAGE)
        .and_then(|d| d.deserialize())
        .unwrap_or_else(|e| e.exit());
    let mind = args.flag_min_depth.unwrap_or(0);
    let maxd = args.flag_max_depth.unwrap_or(::std::usize::MAX);
    let dir = args.arg_dir.clone().unwrap_or(".".to_owned());
    let mut walkdir = WalkDir::new(dir)
        .max_open(args.flag_fd_max)
        .follow_links(args.flag_follow_links)
        .min_depth(mind)
        .max_depth(maxd);
    if args.flag_sort {
        walkdir = walkdir.sort_by(|a,b| a.file_name().cmp(b.file_name()));
    }
    if args.flag_depth {
        walkdir = walkdir.contents_first(true)
    }
    let it = walkdir.into_iter();
    let mut out = io::BufWriter::new(io::stdout());
    let mut eout = io::stderr();
    if args.flag_tree {
        for dent in it {
            match dent {
                Err(err) => {
                    out.flush().unwrap();
                    wout!(eout, "ERROR: {}", err);
                }
                Ok(dent) => {
                    let name = dent.file_name().to_string_lossy();
                    wout!(out, "{}{}", indent(dent.depth()), name);
                }
            }
        }
    } else if args.flag_ignore_errors {
        for dent in it.filter_map(|e| e.ok()) {
            wout!(out, "{}", dent.path().display());
        }
    } else {
        for dent in it {
            match dent {
                Err(err) => {
                    out.flush().unwrap();
                    wout!(eout, "ERROR: {}", err);
                }
                Ok(dent) => {
                    wout!(out, "{}", dent.path().display());
                }
            }
        }
    }
}

fn indent(depth: usize) -> String {
    ::std::iter::repeat(' ').take(2 * depth).collect()
}
