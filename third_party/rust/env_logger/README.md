env_logger [![Build Status](https://travis-ci.org/sebasmagri/env_logger.svg?branch=master)](https://travis-ci.org/sebasmagri/env_logger)
==========

Implements a logger that can be configured via an environment variable.

* [`env_logger` documentation](https://docs.rs/env_logger)

## Usage

### In libraries

`env_logger` makes sense when used in executables (binary projects). Libraries should use the [`log`](https://doc.rust-lang.org/log) crate instead.

### In executables

It must be added along with `log` to the project dependencies:

```toml
[dependencies]
log = "0.4.0"
env_logger = "0.5.6"
```

`env_logger` must be initialized as early as possible in the project. After it's initialized, you can use the `log` macros to do actual logging.

```rust
#[macro_use]
extern crate log;
extern crate env_logger;

fn main() {
    env_logger::init();

    info!("starting up");

    // ...
}
```

Then when running the executable, specify a value for the `RUST_LOG`
environment variable that corresponds with the log messages you want to show.

```bash
$ RUST_LOG=info ./main
INFO: 2017-11-09T02:12:24Z: main: starting up
```

### In tests

Tests can use the `env_logger` crate to see log messages generated during that test:

```toml
[dependencies]
log = "0.4.0"

[dev-dependencies]
env_logger = "0.5.6"
```

```rust
#[macro_use]
extern crate log;

fn add_one(num: i32) -> i32 {
    info!("add_one called with {}", num);
    num + 1
}

#[cfg(test)]
mod tests {
    use super::*;
    extern crate env_logger;

    #[test]
    fn it_adds_one() {
        let _ = env_logger::try_init();
        info!("can log from the test too");
        assert_eq!(3, add_one(2));
    }

    #[test]
    fn it_handles_negative_numbers() {
        let _ = env_logger::try_init();
        info!("logging from another test");
        assert_eq!(-7, add_one(-8));
    }
}
```

Assuming the module under test is called `my_lib`, running the tests with the
`RUST_LOG` filtering to info messages from this module looks like:

```bash
$ RUST_LOG=my_lib=info cargo test
     Running target/debug/my_lib-...

running 2 tests
INFO: 2017-11-09T02:12:24Z: my_lib::tests: logging from another test
INFO: 2017-11-09T02:12:24Z: my_lib: add_one called with -8
test tests::it_handles_negative_numbers ... ok
INFO: 2017-11-09T02:12:24Z: my_lib::tests: can log from the test too
INFO: 2017-11-09T02:12:24Z: my_lib: add_one called with 2
test tests::it_adds_one ... ok

test result: ok. 2 passed; 0 failed; 0 ignored; 0 measured
```

Note that `env_logger::try_init()` needs to be called in each test in which you
want to enable logging. Additionally, the default behavior of tests to
run in parallel means that logging output may be interleaved with test output.
Either run tests in a single thread by specifying `RUST_TEST_THREADS=1` or by
running one test by specifying its name as an argument to the test binaries as
directed by the `cargo test` help docs:

```bash
$ RUST_LOG=my_lib=info cargo test it_adds_one
     Running target/debug/my_lib-...

running 1 test
INFO: 2017-11-09T02:12:24Z: my_lib::tests: can log from the test too
INFO: 2017-11-09T02:12:24Z: my_lib: add_one called with 2
test tests::it_adds_one ... ok

test result: ok. 1 passed; 0 failed; 0 ignored; 0 measured
```

## Configuring log target

By default, `env_logger` logs to stderr. If you want to log to stdout instead,
you can use the `Builder` to change the log target:

```rust
use std::env;
use env_logger::{Builder, Target};

let mut builder = Builder::new();
builder.target(Target::Stdout);
if env::var("RUST_LOG").is_ok() {
    builder.parse(&env::var("RUST_LOG").unwrap());
}
builder.init();
```
