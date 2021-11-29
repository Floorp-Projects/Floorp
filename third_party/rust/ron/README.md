# Rusty Object Notation

[![Build Status](https://travis-ci.org/ron-rs/ron.svg?branch=master)](https://travis-ci.org/ron-rs/ron)
[![Crates.io](https://img.shields.io/crates/v/ron.svg)](https://crates.io/crates/ron)
[![MSRV](https://img.shields.io/badge/MSRV-1.36.0-orange)](https://github.com/ron-rs/ron)
[![Docs](https://docs.rs/ron/badge.svg)](https://docs.rs/ron)
[![Matrix](https://img.shields.io/matrix/ron-rs:matrix.org.svg)](https://matrix.to/#/#ron-rs:matrix.org)

RON is a simple readable data serialization format that looks similar to Rust syntax.
It's designed to support all of [Serde's data model](https://serde.rs/data-model.html), so
structs, enums, tuples, arrays, generic maps, and primitive values.

## Example

```rust
GameConfig( // optional struct name
    window_size: (800, 600),
    window_title: "PAC-MAN",
    fullscreen: false,
    
    mouse_sensitivity: 1.4,
    key_bindings: {
        "up": Up,
        "down": Down,
        "left": Left,
        "right": Right,
        
        // Uncomment to enable WASD controls
        /*
        "W": Up,
        "A": Down,
        "S": Left,
        "D": Right,
        */
    },
    
    difficulty_options: (
        start_difficulty: Easy,
        adaptive: false,
    ),
)
```

## Why RON?

### Example in JSON

```json
{
   "materials": {
        "metal": {
            "reflectivity": 1.0
        },
        "plastic": {
            "reflectivity": 0.5
        }
   },
   "entities": [
        {
            "name": "hero",
            "material": "metal"
        },
        {
            "name": "monster",
            "material": "plastic"
        }
   ]
}
```

Notice these issues:
  1. Struct and maps are the same
     - random order of exported fields
       - annoying and inconvenient for reading
       - doesn't work well with version control
     - quoted field names
       - too verbose
     - no support for enums
  2. No trailing comma allowed
  3. No comments allowed

### Same example in RON

```rust
Scene( // class name is optional
    materials: { // this is a map
        "metal": (
            reflectivity: 1.0,
        ),
        "plastic": (
            reflectivity: 0.5,
        ),
    },
    entities: [ // this is an array
        (
            name: "hero",
            material: "metal",
        ),
        (
            name: "monster",
            material: "plastic",
        ),
    ],
)
```

The new format uses `(`..`)` brackets for *heterogeneous* structures (classes),
while preserving the `{`..`}` for maps, and `[`..`]` for *homogeneous* structures (arrays).
This distinction allows us to solve the biggest problem with JSON.

Here are the general rules to parse the heterogeneous structures:

| class is named? | fields are named? | what is it?               | example             |
| --------------- | ------------------| ------------------------- | ------------------- |
| no              | no                | tuple                     | `(a, b)`            |
| yes/no          | no                | tuple struct              | `Name(a, b)`        |
| yes             | no                | enum value                | `Variant(a, b)`     |
| yes/no          | yes               | struct                    | `(f1: a, f2: b,)`   |

### Specification

There is a very basic, work in progress specification available on
[the wiki page](https://github.com/ron-rs/ron/wiki/Specification).
A more formal and complete grammar is available [here](docs/grammar.md).

### Appendix

Why not XML?
  - too verbose
  - unclear how to treat attributes vs contents

Why not YAML?
  - significant white-space
  - specification is too big

Why not TOML?
  - alien syntax
  - absolute paths are not scalable

Why not XXX?
  - if you know a better format, tell me!

## Tooling

IntelliJ: https://github.com/ron-rs/intellij-ron

VS Code: https://github.com/a5huynh/vscode-ron

Sublime Text: https://packagecontrol.io/packages/RON

Atom: https://atom.io/packages/language-ron

Vim: https://github.com/ron-rs/ron.vim

EMACS: https://chiselapp.com/user/Hutzdog/repository/ron-mode/home

## License

RON is dual-licensed under Apache-2.0 and MIT.

