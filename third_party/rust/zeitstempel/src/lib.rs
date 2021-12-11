//! zeitstempel is German for "timestamp".
//!
//! ---
//!
//! Time's hard. Correct time is near impossible.
//!
//! This crate has one purpose: give me a timestamp as an integer, coming from a monotonic clock
//! source, include time across suspend/hibernation of the host machine and let me compare it to
//! other timestamps.
//!
//! It becomes the developer's responsibility to only compare timestamps obtained from this clock source.
//! Timestamps are not comparable across operating system reboots.
//!
//! # Why not `std::time::Instant`?
//!
//! [`std::time::Instant`] fulfills some of our requirements:
//!
//! * It's monotonic, guaranteed ([sort of][rustsource]).
//! * It can be compared to other timespans.
//!
//! However:
//!
//! * It can't be serialized.
//! * It's not guaranteed that the clock source it uses contains suspend/hibernation time across all operating systems.
//!
//! [rustsource]: https://doc.rust-lang.org/1.47.0/src/std/time.rs.html#213-237
//!
//! # Example
//!
//! ```
//! # use std::{thread, time::Duration};
//! let start = zeitstempel::now();
//! thread::sleep(Duration::from_millis(2));
//!
//! let diff = Duration::from_nanos(zeitstempel::now() - start);
//! assert!(diff >= Duration::from_millis(2));
//! ```
//!
//! # Supported operating systems
//!
//! We support the following operating systems:
//!
//! * Windows\*
//! * macOS
//! * Linux
//! * Android
//! * iOS
//!
//! For other operating systems there's a fallback to `std::time::Instant`,
//! compared against a process-global fixed reference point.
//! We don't guarantee that measured time includes time the system spends in sleep or hibernation.
//!
//! \* To use native Windows 10 functionality enable the `win10plus` feature. Otherwise it will use the fallback.

#![deny(missing_docs)]
#![deny(broken_intra_doc_links)]

cfg_if::cfg_if! {
    if #[cfg(any(target_os = "macos", target_os = "ios"))] {
        mod mac;
        use mac as sys;
    } else if #[cfg(any(target_os = "linux", target_os = "android"))] {
        mod linux;
        use linux as sys;
    } else if #[cfg(all(windows, feature = "win10plus"))] {
        mod win;
        use win as sys;
    } else {
        mod fallback;
        use fallback as sys;
    }
}

/// Returns a timestamp corresponding to "now".
///
/// It can be compared to other timestamps gathered from this API, as long as the host was not
/// rebooted inbetween.
///
///
/// ## Note
///
/// * The difference between two timestamps will include time the system was in sleep or
///   hibernation.
/// * The difference between two timestamps gathered from this is in nanoseconds.
/// * The clocks on some operating systems, e.g. on Windows, are not nanosecond-precise.
///   The value will still use nanosecond resolution.
pub fn now() -> u64 {
    sys::now_including_suspend()
}

#[cfg(test)]
mod test {
    use super::*;
    use std::thread;
    use std::time::Duration;

    #[test]
    fn order() {
        let ts1 = now();
        thread::sleep(Duration::from_millis(2));
        let ts2 = now();

        assert!(ts1 < ts2);
    }
}
