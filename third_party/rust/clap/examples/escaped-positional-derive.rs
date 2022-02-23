// Note: this requires the `derive` feature

use clap::Parser;

#[derive(Parser)]
#[clap(author, version, about, long_about = None)]
struct Cli {
    #[clap(short = 'f')]
    eff: bool,

    #[clap(short = 'p', value_name = "PEAR")]
    pea: Option<String>,

    #[clap(last = true)]
    slop: Vec<String>,
}

fn main() {
    let args = Cli::parse();

    // This is what will happen with `myprog -f -p=bob -- sloppy slop slop`...
    println!("-f used: {:?}", args.eff); // -f used: true
    println!("-p's value: {:?}", args.pea); // -p's value: Some("bob")
    println!("'slops' values: {:?}", args.slop); // 'slops' values: Some(["sloppy", "slop", "slop"])

    // Continued program logic goes here...
}
