// Just check that we aren't asked to use an impossible configuration.
fn main() {
    assert!(
        !(cfg!(feature = "use_ring") && cfg!(feature = "use_openssl")),
        "Cannot configure `hawk` with both `use_ring` and `use_openssl`!"
    );
}
