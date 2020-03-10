set RUST_BACKTRACE=1
cargo test --verbose --features %CLANG_VERSION% -- --nocapture
