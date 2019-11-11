use std::mem;
// use std::os::unix::io::RawFd;
use std::option::Option;
use libc::{self, c_int, c_void};
use {Error, Result};
use errno::Errno;
use ::unistd::Pid;

// For some functions taking with a parameter of type CloneFlags,
// only a subset of these flags have an effect.
libc_bitflags!{
    pub struct CloneFlags: c_int {
        CLONE_VM;
        CLONE_FS;
        CLONE_FILES;
        CLONE_SIGHAND;
        CLONE_PTRACE;
        CLONE_VFORK;
        CLONE_PARENT;
        CLONE_THREAD;
        CLONE_NEWNS;
        CLONE_SYSVSEM;
        CLONE_SETTLS;
        CLONE_PARENT_SETTID;
        CLONE_CHILD_CLEARTID;
        CLONE_DETACHED;
        CLONE_UNTRACED;
        CLONE_CHILD_SETTID;
        CLONE_NEWCGROUP;
        CLONE_NEWUTS;
        CLONE_NEWIPC;
        CLONE_NEWUSER;
        CLONE_NEWPID;
        CLONE_NEWNET;
        CLONE_IO;
    }
}

pub type CloneCb<'a> = Box<FnMut() -> isize + 'a>;

#[repr(C)]
#[derive(Clone, Copy)]
#[allow(missing_debug_implementations)]
pub struct CpuSet {
    cpu_set: libc::cpu_set_t,
}

impl CpuSet {
    pub fn new() -> CpuSet {
        CpuSet { cpu_set: unsafe { mem::zeroed() } }
    }

    pub fn is_set(&self, field: usize) -> Result<bool> {
        if field >= 8 * mem::size_of::<libc::cpu_set_t>() {
            Err(Error::Sys(Errno::EINVAL))
        } else {
            Ok(unsafe { libc::CPU_ISSET(field, &self.cpu_set) })
        }
    }

    pub fn set(&mut self, field: usize) -> Result<()> {
        if field >= 8 * mem::size_of::<libc::cpu_set_t>() {
            Err(Error::Sys(Errno::EINVAL))
        } else {
            Ok(unsafe { libc::CPU_SET(field, &mut self.cpu_set) })
        }
    }

    pub fn unset(&mut self, field: usize) -> Result<()> {
        if field >= 8 * mem::size_of::<libc::cpu_set_t>() {
            Err(Error::Sys(Errno::EINVAL))
        } else {
            Ok(unsafe { libc::CPU_CLR(field, &mut self.cpu_set) })
        }
    }
}

pub fn sched_setaffinity(pid: Pid, cpuset: &CpuSet) -> Result<()> {
    let res = unsafe {
        libc::sched_setaffinity(pid.into(),
                                mem::size_of::<CpuSet>() as libc::size_t,
                                &cpuset.cpu_set)
    };

    Errno::result(res).map(drop)
}

pub fn clone(mut cb: CloneCb,
             stack: &mut [u8],
             flags: CloneFlags,
             signal: Option<c_int>)
             -> Result<Pid> {
    extern "C" fn callback(data: *mut CloneCb) -> c_int {
        let cb: &mut CloneCb = unsafe { &mut *data };
        (*cb)() as c_int
    }

    let res = unsafe {
        let combined = flags.bits() | signal.unwrap_or(0);
        let ptr = stack.as_mut_ptr().offset(stack.len() as isize);
        let ptr_aligned = ptr.offset((ptr as usize % 16) as isize * -1);
        libc::clone(mem::transmute(callback as extern "C" fn(*mut Box<::std::ops::FnMut() -> isize>) -> i32),
                   ptr_aligned as *mut c_void,
                   combined,
                   &mut cb as *mut _ as *mut c_void)
    };

    Errno::result(res).map(Pid::from_raw)
}

pub fn unshare(flags: CloneFlags) -> Result<()> {
    let res = unsafe { libc::unshare(flags.bits()) };

    Errno::result(res).map(drop)
}

// pub fn setns(fd: RawFd, nstype: CloneFlags) -> Result<()> {
//     let res = unsafe { libc::setns(fd, nstype.bits()) };

//     Errno::result(res).map(drop)
// }
