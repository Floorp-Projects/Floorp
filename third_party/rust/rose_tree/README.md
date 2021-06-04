# rose_tree [![Build Status](https://travis-ci.org/mitchmindtree/rose_tree-rs.svg?branch=master)](https://travis-ci.org/mitchmindtree/rose_tree-rs) [![Crates.io](https://img.shields.io/crates/v/rose_tree.svg)](https://crates.io/crates/rose_tree) [![Crates.io](https://img.shields.io/crates/l/rose_tree.svg)](https://github.com/mitchmindtree/rose_tree-rs/blob/master/LICENSE-MIT)



An implementation of the [**rose tree** (aka **multi-way tree**) data structure](https://en.wikipedia.org/wiki/Rose_tree) for Rust.

An indexable tree data structure with a variable and unbounded number of branches per node.

It is Implemented on top of [petgraph](https://github.com/bluss/petulant-avenger-graphlibrary)'s [Graph](http://bluss.github.io/petulant-avenger-graphlibrary/doc/petgraph/graph/struct.Graph.html) data structure and attempts to follow similar conventions where suitable.


Me offering you this crate
--------------------------

                 _.-"""""'.
               .;__        `\
               /   `\        |
               ;a/ a `'. _   |
     ,_        |/_       _)  /  .-.-.
    {(}`\      \.___,     \.'   |   |
     '--''-.(   \_  _     /      \ /
           .-\_ _."-.... ;_       `   .-.-.
            _/ '--.        \          |   |
          ."\    _/\  ,    |           \ /
         /   \_.'  /'./    ;            `
         \__.' '-./   '   /
        __/       `\     /
      .'  ``""--..__\___/
     /                 |
     |    ,            |
      \    ';_        /
       \     \`'-...-'
        \     \    |      __
         \     \   /-----;  '.
       .--\_.-"\         |    \
      /         |._______|\    \
      \_____,__/           '.__|



Documentation
-------------

[API documentation here!](http://mitchmindtree.github.io/rose_tree-rs/rose_tree)


Usage
-----

Please see the [tests directory](https://github.com/mitchmindtree/rose_tree-rs/tree/master/tests) for some basic usage examples.

Use rose_tree in your project by adding it to your Cargo.toml dependencies like so:

```toml
[dependencies]
rose_tree = "*"
```

and then adding

```rust
extern crate rose_tree;
```

to your lib.rs.


License
-------

Dual-licensed to be compatible with the petgraph and Rust projects.

Licensed under the Apache License, Version 2.0 http://www.apache.org/licenses/LICENSE-2.0 or the MIT license http://opensource.org/licenses/MIT, at your option. This file may not be copied, modified, or distributed except according to those terms.


[Beautiful ascii source](http://www.chris.com/ascii/joan/www.geocities.com/SoHo/7373/flowers.html).
