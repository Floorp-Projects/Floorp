/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use camino::Utf8PathBuf;
use clap::{Parser, Subcommand};

// Structs to help our cmdline parsing. Note that docstrings below form part
// of the "help" output.

/// Scaffolding and bindings generator for Rust
#[derive(Parser)]
#[clap(name = "uniffi-bindgen")]
#[clap(version = clap::crate_version!())]
#[clap(propagate_version = true)]
struct Cli {
    #[clap(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Generate foreign language bindings
    Generate {
        /// Foreign language(s) for which to build bindings.
        #[clap(long, short, possible_values = &["kotlin", "python", "swift", "ruby"])]
        language: Vec<String>,

        /// Directory in which to write generated files. Default is same folder as .udl file.
        #[clap(long, short)]
        out_dir: Option<Utf8PathBuf>,

        /// Do not try to format the generated bindings.
        #[clap(long, short)]
        no_format: bool,

        /// Path to the optional uniffi config file. If not provided, uniffi-bindgen will try to guess it from the UDL's file location.
        #[clap(long, short)]
        config: Option<Utf8PathBuf>,

        /// Extract proc-macro metadata from a native lib (cdylib or staticlib) for this crate.
        #[clap(long)]
        lib_file: Option<Utf8PathBuf>,

        /// Path to the UDL file.
        udl_file: Utf8PathBuf,
    },

    /// Generate Rust scaffolding code
    Scaffolding {
        /// Directory in which to write generated files. Default is same folder as .udl file.
        #[clap(long, short)]
        out_dir: Option<Utf8PathBuf>,

        /// Path to the optional uniffi config file. If not provided, uniffi-bindgen will try to guess it from the UDL's file location.
        #[clap(long, short)]
        config: Option<Utf8PathBuf>,

        /// Do not try to format the generated bindings.
        #[clap(long, short)]
        no_format: bool,

        /// Path to the UDL file.
        udl_file: Utf8PathBuf,
    },

    /// Print the JSON representation of the interface from a dynamic library
    PrintJson {
        /// Path to the library file (.so, .dll, .dylib, or .a)
        path: Utf8PathBuf,
    },
}

pub fn run_main() -> anyhow::Result<()> {
    let cli = Cli::parse();
    match &cli.command {
        Commands::Generate {
            language,
            out_dir,
            no_format,
            config,
            lib_file,
            udl_file,
        } => uniffi_bindgen::generate_bindings(
            udl_file,
            config.as_deref(),
            language.iter().map(String::as_str).collect(),
            out_dir.as_deref(),
            lib_file.as_deref(),
            !no_format,
        ),
        Commands::Scaffolding {
            out_dir,
            config,
            no_format,
            udl_file,
        } => uniffi_bindgen::generate_component_scaffolding(
            udl_file,
            config.as_deref(),
            out_dir.as_deref(),
            !no_format,
        ),
        Commands::PrintJson { path } => uniffi_bindgen::print_json(path),
    }?;
    Ok(())
}
