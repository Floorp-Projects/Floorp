use crate::errors::ThreadInfoError;
use nix::{errno::Errno, sys::ptrace, unistd};
use std::{
    io::{self, BufRead},
    path,
};

type Result<T> = std::result::Result<T, ThreadInfoError>;

pub type Pid = i32;

cfg_if::cfg_if! {
    if #[cfg(any(target_arch = "x86", target_arch = "x86_64"))] {
        mod x86;
        pub type ThreadInfo = x86::ThreadInfoX86;
    } else if #[cfg(target_arch = "arm")] {
        mod arm;
        pub type ThreadInfo = arm::ThreadInfoArm;
    } else if #[cfg(target_arch = "aarch64")] {
        mod aarch64;
        pub type ThreadInfo = aarch64::ThreadInfoAarch64;
    } else if #[cfg(target_arch = "mips")] {
        mod mips;
        pub type ThreadInfo = mips::ThreadInfoMips;
    }
}

#[derive(Debug)]
#[allow(non_camel_case_types, dead_code)]
enum NT_Elf {
    NT_NONE = 0,
    NT_PRSTATUS = 1,
    NT_PRFPREGSET = 2,
    //NT_PRPSINFO = 3,
    //NT_TASKSTRUCT = 4,
    //NT_AUXV = 6,
    NT_ARM_VFP = 0x400, // ARM VFP/NEON registers
}

#[inline]
pub fn to_u128(slice: &[u32]) -> &[u128] {
    unsafe { std::slice::from_raw_parts(slice.as_ptr().cast(), slice.len().saturating_div(4)) }
}

#[inline]
pub fn copy_registers(dst: &mut [u128], src: &[u128]) {
    let to_copy = std::cmp::min(dst.len(), src.len());
    dst[..to_copy].copy_from_slice(&src[..to_copy]);
}

#[inline]
pub fn copy_u32_registers(dst: &mut [u128], src: &[u32]) {
    copy_registers(dst, to_u128(src));
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
                .ok_or_else(|| ThreadInfoError::InvalidProcStatusFile(tid, l.clone()))?;
            match start {
                "Tgid:\t" => {
                    tgid = l
                        .get(6..)
                        .ok_or_else(|| ThreadInfoError::InvalidProcStatusFile(tid, l.clone()))?
                        .parse::<Pid>()?;
                }
                "PPid:\t" => {
                    ppid = l
                        .get(6..)
                        .ok_or_else(|| ThreadInfoError::InvalidProcStatusFile(tid, l.clone()))?
                        .parse::<Pid>()?;
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
        request: ptrace::RequestType,
        flag: Option<NT_Elf>,
        pid: nix::unistd::Pid,
    ) -> Result<T> {
        let mut data = std::mem::MaybeUninit::uninit();
        let res = unsafe {
            libc::ptrace(
                request,
                libc::pid_t::from(pid),
                flag.unwrap_or(NT_Elf::NT_NONE),
                data.as_mut_ptr(),
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
        request: ptrace::RequestType,
        flag: Option<NT_Elf>,
        pid: nix::unistd::Pid,
    ) -> Result<T> {
        let mut data = std::mem::MaybeUninit::<T>::uninit();
        let io = libc::iovec {
            iov_base: data.as_mut_ptr().cast(),
            iov_len: std::mem::size_of::<T>(),
        };
        let res = unsafe {
            libc::ptrace(
                request,
                libc::pid_t::from(pid),
                flag.unwrap_or(NT_Elf::NT_NONE),
                &io as *const _,
            )
        };
        Errno::result(res)?;
        Ok(unsafe { data.assume_init() })
    }

    /// COPY FROM CRATE nix BECAUSE ITS NOT PUBLIC
    fn ptrace_peek(
        request: ptrace::RequestType,
        pid: unistd::Pid,
        addr: ptrace::AddressType,
        data: *mut libc::c_void,
    ) -> nix::Result<libc::c_long> {
        let ret = unsafe {
            Errno::clear();
            libc::ptrace(request, libc::pid_t::from(pid), addr, data)
        };
        match Errno::result(ret) {
            Ok(..) | Err(Errno::UnknownErrno) => Ok(ret),
            err @ Err(..) => err,
        }
    }
}
impl ThreadInfo {
    pub fn create(pid: Pid, tid: Pid) -> std::result::Result<Self, ThreadInfoError> {
        Self::create_impl(pid, tid)
    }
}
