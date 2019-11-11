use libc::{c_void, siginfo_t};

extern "C" {
    fn siginfo_si_addr(si: *const siginfo_t) -> *const c_void;
}

pub trait SiginfoExt {
    fn si_addr_ext(&self) -> *const c_void;
}

impl SiginfoExt for siginfo_t {
    fn si_addr_ext(&self) -> *const c_void {
        unsafe { siginfo_si_addr(self as *const siginfo_t) }
    }
}
