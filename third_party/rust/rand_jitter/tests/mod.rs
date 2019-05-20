extern crate rand_jitter;
extern crate rand_core;

use rand_jitter::JitterRng;
#[cfg(feature = "std")]
use rand_core::RngCore;

#[cfg(feature = "std")]
#[test]
fn test_jitter_init() {
    // Because this is a debug build, measurements here are not representive
    // of the final release build.
    // Don't fail this test if initializing `JitterRng` fails because of a
    // bad timer (the timer from the standard library may not have enough
    // accuracy on all platforms).
    match JitterRng::new() {
        Ok(ref mut rng) => {
            // false positives are possible, but extremely unlikely
            assert!(rng.next_u32() | rng.next_u32() != 0);
        },
        Err(_) => {},
    }
}

#[test]
fn test_jitter_bad_timer() {
    fn bad_timer() -> u64 { 0 }
    let mut rng = JitterRng::new_with_timer(bad_timer);
    assert!(rng.test_timer().is_err());
}

