use crate::errors::ThreadInfoError;
use nix::errno::Errno;
use nix::sys::ptrace;
use nix::unistd;
use std::convert::TryInto;
use std::io::{self, BufRead};
use std::path;

type Result<T> = std::result::Result<T, ThreadInfoError>;

pub type Pid = i32;

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[path = "thread_info_x86.rs"]
mod imp;
#[cfg(target_arch = "arm")]
#[path = "thread_info_arm.rs"]
mod imp;
#[cfg(target_arch = "aarch64")]
#[path = "thread_info_aarch64.rs"]
mod imp;
#[cfg(target_arch = "mips")]
#[path = "thread_info_mips.rs"]
mod imp;

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
pub type ThreadInfo = imp::ThreadInfoX86;
#[cfg(target_arch = "arm")]
pub type ThreadInfo = imp::ThreadInfoArm;
#[cfg(target_arch = "aarch64")]
pub type ThreadInfo = imp::ThreadInfoAarch64;
#[cfg(target_arch = "mips")]
pub type ThreadInfo = imp::ThreadInfoMips;

#[derive(Debug)]
#[allow(non_camel_case_types)]
enum NT_Elf {
    NT_NONE = 0,
    NT_PRSTATUS = 1,
    NT_PRFPREG = 2,
    //NT_PRPSINFO = 3,
    //NT_TASKSTRUCT = 4,
    //NT_AUXV = 6,
}

pub fn to_u128(slice: &[u32]) -> Vec<u128> {
    let mut res = Vec::new();
    for chunk in slice.chunks_exact(4) {
        let value = u128::from_ne_bytes(
            chunk
                .iter()
                .map(|x| x.to_ne_bytes().to_vec())
                .flatten()
                .collect::<Vec<_>>()
                .as_slice()
                .try_into() // Make slice into fixed size array
                .unwrap(), // Which has to work as we know the numbers work out
        );
        res.push(value)
    }
    res
}

trait CommonThreadInfo {
    fn get_ppid_and_tgid(tid: Pid) -> Result<(Pid, Pid)> {
        let mut ppid = -1;
        let mut tgid = -1;

        let status_path = path::PathBuf::from(format!("/proc/{}/status", tid));
        let status_file = std::fs::File::open(status_path)?;
        for line in io::BufReader::new(status_file).lines() {
            let l = line?;
            let start = l
                .get(0..6)
                .ok_or(ThreadInfoError::InvalidProcStatusFile(tid, l.clone()))?;
            match start {
                "Tgid:\t" => {
                    tgid = l
                        .get(6..)
                        .ok_or(ThreadInfoError::InvalidProcStatusFile(tid, l.clone()))?
                        .parse::<Pid>()?
                }
                "PPid:\t" => {
                    ppid = l
                        .get(6..)
                        .ok_or(ThreadInfoError::InvalidProcStatusFile(tid, l.clone()))?
                        .parse::<Pid>()?
                }
                _ => continue,
            }
        }
        if ppid == -1 || tgid == -1 {
            return Err(ThreadInfoError::InvalidPid(
                format!("/proc/{}/status", tid),
                ppid,
                tgid,
            ));
        }
        Ok((ppid, tgid))
    }

    /// SLIGHTLY MODIFIED COPY FROM CRATE nix
    /// Function for ptrace requests that return values from the data field.
    /// Some ptrace get requests populate structs or larger elements than `c_long`
    /// and therefore use the data field to return values. This function handles these
    /// requests.
    fn ptrace_get_data<T>(
        request: ptrace::Request,
        flag: Option<NT_Elf>,
        pid: nix::unistd::Pid,
    ) -> Result<T> {
        let mut data = std::mem::MaybeUninit::uninit();
        let res = unsafe {
            libc::ptrace(
                request as ptrace::RequestType,
                libc::pid_t::from(pid),
                flag.unwrap_or(NT_Elf::NT_NONE),
                data.as_mut_ptr() as *const _ as *const libc::c_void,
            )
        };
        Errno::result(res)?;
        Ok(unsafe { data.assume_init() })
    }

    /// SLIGHTLY MODIFIED COPY FROM CRATE nix
    /// Function for ptrace requests that return values from the data field.
    /// Some ptrace get requests populate structs or larger elements than `c_long`
    /// and therefore use the data field to return values. This function handles these
    /// requests.
    fn ptrace_get_data_via_io<T>(
        request: ptrace::Request,
        flag: Option<NT_Elf>,
        pid: nix::unistd::Pid,
    ) -> Result<T> {
        let mut data = std::mem::MaybeUninit::uninit();
        let io = libc::iovec {
            iov_base: data.as_mut_ptr() as *mut libc::c_void,
            iov_len: std::mem::size_of::<T>(),
        };
        let res = unsafe {
            libc::ptrace(
                request as ptrace::RequestType,
                libc::pid_t::from(pid),
                flag.unwrap_or(NT_Elf::NT_NONE),
                &io as *const _ as *const libc::c_void,
            )
        };
        Errno::result(res)?;
        Ok(unsafe { data.assume_init() })
    }

    /// COPY FROM CRATE nix BECAUSE ITS NOT PUBLIC
    fn ptrace_peek(
        request: ptrace::Request,
        pid: unistd::Pid,
        addr: ptrace::AddressType,
        data: *mut libc::c_void,
    ) -> nix::Result<libc::c_long> {
        let ret = unsafe {
            Errno::clear();
            libc::ptrace(
                request as ptrace::RequestType,
                libc::pid_t::from(pid),
                addr,
                data,
            )
        };
        match Errno::result(ret) {
            Ok(..) | Err(nix::Error::Sys(Errno::UnknownErrno)) => Ok(ret),
            err @ Err(..) => err,
        }
    }
}
impl ThreadInfo {
    pub fn create(pid: Pid, tid: Pid) -> std::result::Result<Self, ThreadInfoError> {
        Self::create_impl(pid, tid)
    }
}
