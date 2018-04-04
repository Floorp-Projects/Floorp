#[macro_use]
extern crate serde_derive;
extern crate docopt;

use docopt::Docopt;

const USAGE: &'static str = "
Naval Fate.

Usage:
  naval_fate.py ship new <name>...
  naval_fate.py ship <name> move <x> <y> [--speed=<kn>]
  naval_fate.py ship shoot <x> <y>
  naval_fate.py mine (set|remove) <x> <y> [--moored | --drifting]
  naval_fate.py (-h | --help)
  naval_fate.py --version

Options:
  -h --help     Show this screen.
  --version     Show version.
  --speed=<kn>  Speed in knots [default: 10].
  --moored      Moored (anchored) mine.
  --drifting    Drifting mine.
";

#[derive(Debug, Deserialize)]
struct Args {
    flag_speed: isize,
    flag_drifting: bool,
    arg_name: Vec<String>,
    arg_x: Option<isize>,
    arg_y: Option<isize>,
    cmd_ship: bool,
    cmd_mine: bool,
}

fn main() {
    let args: Args = Docopt::new(USAGE)
        .and_then(|d| d.deserialize())
        .unwrap_or_else(|e| e.exit());
    println!("{:?}", args);

    println!("\nSome values:");
    println!("  Speed: {}", args.flag_speed);
    println!("  Drifting? {}", args.flag_drifting);
    println!("  Names: {:?}", args.arg_name);
    println!("  Command 'ship' invoked? {:?}", args.cmd_ship);
}
