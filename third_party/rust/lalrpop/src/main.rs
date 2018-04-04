extern crate docopt;
extern crate lalrpop;
#[macro_use]
extern crate serde_derive;
extern crate serde;

use docopt::Docopt;
use lalrpop::Configuration;
use std::env;
use std::io::{self, Write};
use std::process;

static VERSION: &'static str = env!("CARGO_PKG_VERSION");

fn main() {
    main1().unwrap();
}

fn main1() -> io::Result<()> {
    let mut stderr = std::io::stderr();
    let mut stdout = std::io::stdout();

    let args: Args = Docopt::new(USAGE)
        .and_then(|d| d.argv(env::args()).deserialize())
        .unwrap_or_else(|e| e.exit());

    if args.flag_version {
        try!(writeln!(stdout, "{}", VERSION));
        process::exit(0);
    }

    let mut config = Configuration::new();

    match args.flag_level.unwrap_or(LevelFlag::Info) {
        LevelFlag::Quiet => config.log_quiet(),
        LevelFlag::Info => config.log_info(),
        LevelFlag::Verbose => config.log_verbose(),
        LevelFlag::Debug => config.log_debug(),
    };

    if args.flag_force {
        config.force_build(true);
    }

    if args.flag_color {
        config.always_use_colors();
    }

    if args.flag_comments {
        config.emit_comments(true);
    }

    if args.flag_report {
        config.emit_report(true);
    }

    if args.arg_inputs.len() == 0 {
        try!(writeln!(
            stderr,
            "Error: no input files specified! Try --help for help."
        ));
        process::exit(1);
    }

    for arg in args.arg_inputs {
        match config.process_file(&arg) {
            Ok(()) => {}
            Err(err) => {
                try!(writeln!(
                    stderr,
                    "Error encountered processing `{}`: {}",
                    arg, err
                ));
                process::exit(1);
            }
        }
    }

    Ok(())
}

const USAGE: &'static str = "
Usage: lalrpop [options] <inputs>...
       lalrpop --help
       lalrpop (-V | --version)

Options:
    -V, --version        Print version.
    -l, --level LEVEL    Set the debug level. (Default: info)
                         Valid values: quiet, info, verbose, debug.
    -f, --force          Force execution, even if the .lalrpop file is older than the .rs file.
    -c, --color          Force colorful output, even if this is not a TTY.
    --comments           Enable comments in the generated code.
    --report             Generate report files.
";

#[derive(Debug, Deserialize)]
struct Args {
    arg_inputs: Vec<String>,
    flag_level: Option<LevelFlag>,
    flag_force: bool,
    flag_color: bool,
    flag_comments: bool,
    flag_report: bool,
    flag_version: bool,
}

#[derive(Debug, Deserialize)]
enum LevelFlag {
    Quiet,
    Info,
    Verbose,
    Debug,
}

#[cfg(test)]
mod test {
    use docopt::Docopt;
    use super::USAGE;
    use super::Args;

    #[test]
    fn test_usage_help() {
        let argv = || vec!["lalrpop", "--help"];
        let _: Args = Docopt::new(USAGE)
            .and_then(|d| d.help(false).argv(argv().into_iter()).deserialize())
            .unwrap();
    }

    #[test]
    fn test_usage_version() {
        let argv = || vec!["lalrpop", "--version"];
        let _: Args = Docopt::new(USAGE)
            .and_then(|d| d.argv(argv().into_iter()).deserialize())
            .unwrap();
    }

    #[test]
    fn test_usage_single_input() {
        let argv = || vec!["lalrpop", "file.lalrpop"];
        let _: Args = Docopt::new(USAGE)
            .and_then(|d| d.argv(argv().into_iter()).deserialize())
            .unwrap();
    }

    #[test]
    fn test_usage_multiple_inputs() {
        let argv = || vec!["lalrpop", "file.lalrpop", "../file2.lalrpop"];
        let _: Args = Docopt::new(USAGE)
            .and_then(|d| d.argv(argv().into_iter()).deserialize())
            .unwrap();
    }
}
