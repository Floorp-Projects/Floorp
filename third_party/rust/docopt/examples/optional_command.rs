// This example shows how to implement a command with a "catch all."
//
// This requires writing your own impl for `Decodable` because docopt's
// decoder uses `Option<T>` to mean "T may not be present" rather than
// "T may be present but incorrect."

#[macro_use]
extern crate serde_derive;
extern crate serde;
extern crate docopt;

use docopt::Docopt;
use serde::de::{Deserialize, Deserializer, Error, Visitor};
use std::fmt;

// Write the Docopt usage string.
const USAGE: &'static str = "
Rust's package manager

Usage:
    mycli [<command>]

Options:
    -h, --help       Display this message
";

#[derive(Debug, Deserialize)]
struct Args {
    arg_command: Command,
}

struct CommandVisitor;

impl<'de> Visitor<'de> for CommandVisitor {
    type Value = Command;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a string A, B or C")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E>
        where E: Error
    {
        Ok(match s {
               "" => Command::None,
               "A" => Command::A,
               "B" => Command::B,
               "C" => Command::C,
               s => Command::Unknown(s.to_string()),
           })
    }
}

impl<'de> Deserialize<'de> for Command {
    fn deserialize<D>(d: D) -> Result<Command, D::Error>
        where D: Deserializer<'de>
    {
        d.deserialize_str(CommandVisitor)
    }
}

#[derive(Debug)]
enum Command {
    A,
    B,
    C,
    Unknown(String),
    None,
}

fn main() {
    let args: Args = Docopt::new(USAGE)
        .and_then(|d| d.deserialize())
        .unwrap_or_else(|e| e.exit());
    println!("{:?}", args);
}
