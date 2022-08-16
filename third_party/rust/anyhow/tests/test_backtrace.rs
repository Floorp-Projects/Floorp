// FIXME: needs to be updated to provide_ref/request_ref API.
// https://github.com/rust-lang/rust/pull/99431
#![cfg(any())]

#[rustversion::not(nightly)]
#[ignore]
#[test]
fn test_backtrace() {}

#[rustversion::nightly]
#[test]
fn test_backtrace() {
    use anyhow::anyhow;

    let error = anyhow!("oh no!");
    let _ = error.backtrace();
}
