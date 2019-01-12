Docopt for Rust with automatic type based decoding (i.e., data validation).
This implementation conforms to the
[official description of Docopt](http://docopt.org/) and
[passes its test suite](https://github.com/docopt/docopt/pull/201).

[![Build status](https://api.travis-ci.org/docopt/docopt.rs.svg)](https://travis-ci.org/docopt/docopt.rs)
[![](http://meritbadge.herokuapp.com/docopt)](https://crates.io/crates/docopt)

Dual-licensed under MIT or the [UNLICENSE](http://unlicense.org).


### Current status

Fully functional but the design of the API is up for debate. **I am seeking
feedback**.


### Documentation

<https://docs.rs/docopt>


### Installation

This crate is fully compatible with Cargo. Just add it to your `Cargo.toml`:

```toml
[dependencies]
docopt = "0.8"
serde = "1.0" # if you're using `derive(Deserialize)`
serde_derive = "1.0" # if you're using `derive(Deserialize)`
```

If you want to use the macro, then add `docopt_macros = "0.8"` instead.
Note that the **`docopt!` macro only works on a nightly Rust compiler** because
it is a compiler plugin.


### Quick example

Here is a full working example. Notice that you can specify the types of each
of the named values in the Docopt usage string. Values will be automatically
converted to those types (or an error will be reported).

```rust
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
    arg_x: Option<i32>,
    arg_y: Option<i32>,
    cmd_ship: bool,
    cmd_mine: bool,
}

fn main() {
    let args: Args = Docopt::new(USAGE)
                            .and_then(|d| d.deserialize())
                            .unwrap_or_else(|e| e.exit());
    println!("{:?}", args);
}
```

Here is the same example, but with the use of the `docopt!` macro, which will
*generate a struct for you*. Note that this uses a compiler plugin, so it only
works on a **nightly Rust compiler**:

```rust
#![feature(plugin)]
#![plugin(docopt_macros)]

#[macro_use]
extern crate serde_derive;
extern crate docopt;

use docopt::Docopt;

docopt!(Args derive Debug, "
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
");

fn main() {
    let args: Args = Args::docopt().deserialize().unwrap_or_else(|e| e.exit());
    println!("{:?}", args);
}
```

The `Args` struct has one static method defined for it: `docopt`. The method
returns a normal `Docopt` value, which can be used to set configuration
options, `argv` and parse or decode command line arguments.


### Struct field name mapping

The field names of the struct map like this:

```
-g            => flag_g
--group       => flag_group
--group <arg> => flag_group
FILE          => arg_FILE
<file>        => arg_file
build         => cmd_build
```


### Data validation example

Here's another example that shows how to specify the types of your arguments:

```rust
#![feature(plugin)]
#![plugin(docopt_macros)]

#[macro_use]
extern crate serde_derive;

extern crate docopt;

docopt!(Args, "Usage: add <x> <y>", arg_x: i32, arg_y: i32);

fn main() {
    let args: Args = Args::docopt().deserialize().unwrap_or_else(|e| e.exit());
    println!("x: {}, y: {}", args.arg_x, args.arg_y);
}
```

In this example, specific type annotations were added. They will be
automatically inserted into the generated struct. You can override as many (or
as few) fields as you want. If you don't specify a type, then one of `bool`,
`u64`, `String` or `Vec<String>` will be chosen depending on the type of
argument. In this case, both `arg_x` and `arg_y` would have been `String`.

If any value cannot be decoded into a value with the right type, then an error
will be shown to the user.

And of course, you don't need the macro to do this. You can do the same thing
with a manually written struct too.


### Modeling `rustc`

Here's a selected subset for some of `rustc`'s options. This also shows how to
restrict values to a list of choices via an `enum` type and demonstrates more
Docopt features.

```rust
#![feature(plugin)]
#![plugin(docopt_macros)]

#[macro_use]
extern crate serde_derive;
extern crate serde;

extern crate docopt;

use serde::de;

docopt!(Args derive Debug, "
Usage: rustc [options] [--cfg SPEC... -L PATH...] INPUT
       rustc (--help | --version)

Options:
    -h, --help         Show this message.
    --version          Show the version of rustc.
    --cfg SPEC         Configure the compilation environment.
    -L PATH            Add a directory to the library search path.
    --emit TYPE        Configure the output that rustc will produce.
                       Valid values: asm, ir, bc, obj, link.
    --opt-level LEVEL  Optimize with possible levels 0-3.
", flag_opt_level: Option<OptLevel>, flag_emit: Option<Emit>);

#[derive(Deserialize, Debug)]
enum Emit { Asm, Ir, Bc, Obj, Link }

#[derive(Debug)]
enum OptLevel { Zero, One, Two, Three }

impl<'de> de::Deserialize<'de> for OptLevel {
    fn deserialize<D>(deserializer: D) -> Result<OptLevel, D::Error>
        where D: de::Deserializer<'de>
    {
        let level = match u8::deserialize(deserializer)? {
            0 => OptLevel::Zero,
            1 => OptLevel::One,
            2 => OptLevel::Two,
            3 => OptLevel::Three,
            n => {
                let value = de::Unexpected::Unsigned(n as u64);
                let msg = "expected an integer between 0 and 3";
                return Err(de::Error::invalid_value(value, &msg));
            }
        };
        Ok(level)
    }
}

fn main() {
    let args: Args = Args::docopt().deserialize().unwrap_or_else(|e| e.exit());
    println!("{:?}", args);
}
```

### Viewing the generated struct

Generating a struct is pretty magical, but if you want, you can look at it by
expanding all macros. Say you wrote the above example for `Usage: add <x> <y>`
into a file called `add.rs`. Then running:

```bash
rustc -L path/containing/docopt/lib -Z unstable-options --pretty=expanded add.rs
```

Will show all macros expanded. The `path/containing/docopt/lib` is usually
`target/debug/deps` or `target/release/deps` in a cargo project. In the generated code, you should be
able to find the generated struct:

```rust
struct Args {
    pub arg_x: int,
    pub arg_y: int,
}
```


### Traditional Docopt API

The reference implementation of Docopt returns a Python dictionary with names
like `<arg>` or `--flag`. If you prefer this access pattern, then you can use
`docopt::ArgvMap`. The disadvantage is that you have to do all of your type
conversion manually. Here's the canonical Docopt example with a hash table:

```rust
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

fn main() {
    let args = Docopt::new(USAGE)
                      .and_then(|dopt| dopt.parse())
                      .unwrap_or_else(|e| e.exit());
    println!("{:?}", args);

    // You can conveniently access values with `get_{bool,count,str,vec}`
    // functions. If the key doesn't exist (or if, e.g., you use `get_str` on
    // a switch), then a sensible default value is returned.
    println!("\nSome values:");
    println!("  Speed: {}", args.get_str("--speed"));
    println!("  Drifting? {}", args.get_bool("--drifting"));
    println!("  Names: {:?}", args.get_vec("<name>"));
}
```

### Tab completion support

This particular implementation bundles a command called `docopt-wordlist` that
can be used to automate tab completion. This repository also collects some
basic completion support for various shells (currently only bash) in the
`completions` directory.

You can use them to setup tab completion on your system. It should work with
any program that uses Docopt (or rather, any program that outputs usage
messages that look like Docopt). For example, to get tab completion support for
Cargo, you'll have to install `docopt-wordlist` and add some voodoo to your
`$HOME/.bash_completion` file (this may vary for other shells).

Here it is step by step:

```bash
# Download and build `docopt-wordlist` (as part of the Docopt package)
$ git clone git://github.com/docopt/docopt.rs
$ cd docopt.rs
$ cargo build --release

# Now setup tab completion (for bash)
$ echo "DOCOPT_WORDLIST_BIN=\"$(pwd)/target/release/docopt-wordlist\"" >> $HOME/.bash_completion
$ echo "source \"$(pwd)/completions/docopt-wordlist.bash\"" >> $HOME/.bash_completion
$ echo "complete -F _docopt_wordlist_commands cargo" >> $HOME/.bash_completion
```

My [CSV toolkit](https://github.com/BurntSushi/xsv) is supported too:

```bash
# shameless plug...
$ echo "complete -F _docopt_wordlist_commands xsv" >> $HOME/.bash_completion
```

Note that this is emphatically a first pass. There are several improvements
that I'd like to make:

1. Take context into account when completing. For example, it should be
   possible to only show completions that can lead to a valid Docopt match.
   This may be hard. (i.e., It may require restructuring Docopt's internals.)
2. Support more shells. (I'll happily accept pull requests on this one. I doubt
   I'll venture outside of bash any time soon.)
3. Make tab completion support more seamless. The way it works right now is
   pretty hacky by intermingling file/directory completion.
