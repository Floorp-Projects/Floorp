use std::convert::TryInto;
use std::time::Instant;

use once_cell::sync::Lazy;

static INIT_TIME: Lazy<Instant> = Lazy::new(Instant::now);

pub fn now_including_suspend() -> u64 {
    // For Windows:
    // Instead of relying on figuring out the underlying functions,
    // we can rely on the fact that `Instant::now` maps to [QueryPerformanceCounter] on Windows,
    // so by comparing it to another arbitrary timestamp we will get a duration that will include
    // suspend time.
    // If we use that as a timestamp we can compare later timestamps to it and that will also
    // include suspend time.
    //
    // [QueryPerformanceCounter]: https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter
    //
    // For other operating systems:
    // This fallback is not used on Linux, where it maps to `CLOCK_MONOTONIC`, which does NOT
    // include suspend time. But we don't use it there, so no problem.
    //
    // This fallback is not used on macOS, where it maps to `mach_absolute_time`, which does NOT
    // include suspend time. But we don't use it there, so no problem.
    //
    // For other operating systems we make no guarantees, other than that we won't panic.
    let now = Instant::now();
    now.checked_duration_since(*INIT_TIME)
        .and_then(|diff| diff.as_nanos().try_into().ok())
        .unwrap_or(0)
}
