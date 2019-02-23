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
docopt = "1"
serde = "1.0" # if you're using `derive(Deserialize)`
serde_derive = "1.0" # if you're using `derive(Deserialize)`
```


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
