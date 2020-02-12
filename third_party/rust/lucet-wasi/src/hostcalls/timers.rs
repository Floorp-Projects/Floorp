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

    // Referring these 3 sources
    //https://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x
    //https://stackoverflow.com/questions/11680461/monotonic-clock-on-osx
    //https://gist.github.com/lifthrasiir/393ffb3e9900709fa2e3ae2a540b635f

    static mut CONVERSION_FACTOR : f64 = 0.0;
    static INIT : Once = Once::new();

    unsafe fn get_cached_conversion_factor() -> f64 {
        unsafe {
            INIT.call_once(|| {
                let mut timebase = MaybeUninit::<mach_timebase_info_data_t>::uninit();
                mach_timebase_info(timebase.as_mut_ptr());
                let timebase = unsafe { timebase.assume_init() };
            
                let numer_d : f64 = timebase.numer as f64;
                let denom_d : f64 = timebase.denom as f64;
            
                CONVERSION_FACTOR = numer_d / denom_d;
            });
        }
        CONVERSION_FACTOR
    }

    pub unsafe fn clock_gettime_helper(clock_id: libc::clockid_t, tp: *mut libc::timespec) -> c_int {
        if !(clock_id == libc::CLOCK_REALTIME || clock_id == libc::CLOCK_MONOTONIC) {
            (*libc::__error()) = libc::EINVAL;
            return -1;
        }

        if clock_id == libc::CLOCK_REALTIME {
            let mut micro = MaybeUninit::<libc::timeval>::uninit();
            libc::gettimeofday(micro.as_mut_ptr(), core::ptr::null_mut());
            let micro = unsafe { micro.assume_init() };
      
            (*tp).tv_sec = micro.tv_sec;
            (*tp).tv_nsec = i64::from(micro.tv_usec) * 1000;
            return 0;
        } else {
            let time : u64 = mach_absolute_time();
            let time_d : f64 = time as f64;
            let conv : f64 = get_cached_conversion_factor();
            let nseconds : f64 = time_d * conv;
            let seconds : f64 = nseconds / 1e9;
            (*tp).tv_sec = seconds as i64;
            (*tp).tv_nsec = nseconds as i64;
            return 0;
        }
    }

    pub unsafe fn clock_getres_helper(clock_id: libc::clockid_t, res: *mut libc::timespec) -> c_int {
        if !(clock_id == libc::CLOCK_REALTIME || clock_id == libc::CLOCK_MONOTONIC) {
            (*libc::__error()) = libc::EINVAL;
            return -1;
        }

        (*res).tv_sec = 0 as i64;
        (*res).tv_nsec = 
            if clock_id == libc::CLOCK_REALTIME {
                1000 as i64
            } else {
                1 as i64
            };
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