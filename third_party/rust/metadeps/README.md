metadeps lets you write `pkg-config` dependencies in `Cargo.toml` metadata,
rather than programmatically in `build.rs`.  This makes those dependencies
declarative, so other tools can read them as well.

# Usage

In your `Cargo.toml`, add the following to your `[build-dependencies]`:

```toml
metadeps = "1.1"
```

Then, to declare a dependency on `testlib >= 1.2`, and a conditional dependency
on `testdata >= 4.5`, add the following section:

```toml
[package.metadata.pkg-config]
testlib = "1.2"
testdata = { version = "4.5", feature = "use-testdata" }
```

In your `build.rs`, add:

```rust
extern crate metadeps;

fn main() {
    metadeps::probe().unwrap();
}
```
