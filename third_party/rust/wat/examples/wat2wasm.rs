use anyhow::Context;
use std::env;

fn main() -> anyhow::Result<()> {
    // Use the `getopts` crate to parse the `-o` option as well as `-h`
    let program = env::args().nth(0).unwrap();
    let mut opts = getopts::Options::new();
    opts.optopt("o", "", "set output file name", "NAME");
    opts.optflag("h", "help", "print this help menu");
    let matches = opts.parse(env::args_os().skip(1))?;
    if matches.opt_present("h") {
        return Ok(print_usage(&program, opts));
    }
    let input = match matches.free.len() {
        0 => {
            print_usage(&program, opts);
            std::process::exit(1);
        }
        1 => &matches.free[0],
        _ => anyhow::bail!("more than one input file specified on command line"),
    };

    // Using `wast`, parse this input file into a wasm binary...
    let binary = wat::parse_file(&input)?;

    // ... and if requested, write out that file!
    if let Some(output) = matches.opt_str("o") {
        std::fs::write(&output, binary).context(format!("failed to write: {}", output))?;
    }
    Ok(())
}

fn print_usage(program: &str, opts: getopts::Options) {
    let brief = format!("Usage: {} FILE [options]", program);
    print!("{}", opts.usage(&brief));
}
