use clap::Parser;

#[derive(Parser)]
#[clap(name = "cargo")]
#[clap(bin_name = "cargo")]
enum Cargo {
    ExampleDerive(ExampleDerive),
}

#[derive(clap::Args)]
#[clap(author, version, about, long_about = None)]
struct ExampleDerive {
    #[clap(long, parse(from_os_str))]
    manifest_path: Option<std::path::PathBuf>,
}

fn main() {
    let Cargo::ExampleDerive(args) = Cargo::parse();
    println!("{:?}", args.manifest_path);
}
