use nix::libc::{self, c_int, c_char};

#[cfg(not(target_os = "macos"))]
mod notmac
{
    use super::*;

    pub unsafe fn clock_gettime_helper(clock_id: libc::clockid_t, tp: *mut libc::timespec) -> c_int {
        libc::clock_gettime(clock_id, tp)
    }

    pub unsafe fn clock_getres_helper(clock_id: libc::clockid_t, res: *mut libc::timespec) -> c_int {
        libc::clock_getres(clock_id, res)
    }

    pub unsafe fn futimens_helper(fd: c_int, times: *const libc::timespec) -> c_int {
        libc::futimens(fd, times)
    }

    pub unsafe fn utimensat_helper(dirfd: c_int, path: *const c_char, times: *const libc::timespec, flag: c_int) -> c_int {
        libc::utimensat(dirfd, path, times, flag)
    }
}

#[cfg(target_os = "macos")]
mod mac
{
    use super::*;
    use std::mem::MaybeUninit;
    use std::sync::Once;

    use mach::mach_time::*;

    // From here:
    // https://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x/21352348#21352348

    static INIT : Once = Once::new();

    // conversion factor
    static mut TIMEBASE_NUMER : u64 = 0;
    static mut TIMEBASE_DENOM : u64 = 0;
    // nanoseconds since 1-Jan-1970 to init()
    static mut INITTIME_SEC : u64 = 0;
    static mut INITTIME_NANOSEC : u64 = 0;
    // ticks since boot to init()
    static mut INITCLOCK : u64 = 0;
    static mut INITSUCCESS: bool = false;

    unsafe fn compute_time_cached_values() -> bool {
        INIT.call_once(|| {
            {
                let mut timebase = MaybeUninit::<mach_timebase_info_data_t>::uninit();
                let ret = mach_timebase_info(timebase.as_mut_ptr());
                if ret != mach::kern_return::KERN_SUCCESS {
                    return;
                }
                let timebase = unsafe { timebase.assume_init() };

                TIMEBASE_NUMER = timebase.numer as u64;
                TIMEBASE_DENOM = timebase.denom as u64;
            }

            {
                let mut micro = MaybeUninit::<libc::timeval>::uninit();
                let ret = libc::gettimeofday(micro.as_mut_ptr(), core::ptr::null_mut());
                if ret != 0 {
                    return;
                }
                let micro = unsafe { micro.assume_init() };
                INITTIME_SEC = micro.tv_sec as u64;
                INITTIME_NANOSEC = micro.tv_usec as u64 * 1000;
            }

            {
                INITCLOCK = mach_absolute_time();
            }

            INITSUCCESS = true;
        });
        return INITSUCCESS;
    }

    pub unsafe fn clock_gettime_helper(_clock_id: libc::clockid_t, tp: *mut libc::timespec) -> c_int {
        if !compute_time_cached_values() {
            return -1;
        }

        let clock: u64 = mach_absolute_time() - INITCLOCK;
        let nano: u64 = clock * TIMEBASE_NUMER / TIMEBASE_DENOM;

        let billion: u64 = 1000000000;

        let mut out_sec: u64 = INITTIME_SEC + (nano / billion);
        let mut out_nsec: u64 = INITTIME_NANOSEC + (nano % billion);

        out_sec += out_nsec / billion;
        out_nsec = out_nsec % billion;

        (*tp).tv_sec = out_sec as i64;
        (*tp).tv_nsec = out_nsec as i64;
        return 0;
    }

    pub unsafe fn clock_getres_helper(_clock_id: libc::clockid_t, res: *mut libc::timespec) -> c_int {
        (*res).tv_sec = 0 as i64;
        (*res).tv_nsec = 1 as i64;
        return 0;
    }

    pub unsafe fn futimens_helper(_fd: c_int, _times: *const libc::timespec) -> c_int {
        panic!("futimens not implemented");
    }

    pub unsafe fn utimensat_helper(_dirfd: c_int, _path: *const c_char, _times: *const libc::timespec, _flag: c_int) -> c_int {
        panic!("utimensat not implemented");
    }
}

#[cfg(not(target_os = "macos"))]
pub use notmac::*;

#[cfg(target_os = "macos")]
pub use mac::*;