const NS_PER_S: u64 = 1_000_000_000;

fn timespec_to_ns(ts: libc::timespec) -> u64 {
    (ts.tv_sec as u64) * NS_PER_S + (ts.tv_nsec as u64)
}

/// The time from a clock that cannot be set
/// and represents monotonic time since some unspecified starting point,
/// that also includes any time that the system is suspended.
///
/// See [`clock_gettime`].
///
/// [`clock_gettime`]: https://manpages.debian.org/buster/manpages-dev/clock_gettime.3.en.html
pub fn now_including_suspend() -> u64 {
    let mut ts = libc::timespec {
        tv_sec: 0,
        tv_nsec: 0,
    };
    unsafe {
        libc::clock_gettime(libc::CLOCK_BOOTTIME, &mut ts);
    }

    timespec_to_ns(ts)
}
