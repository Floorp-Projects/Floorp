use libc::{self, SI_LOAD_SHIFT};
use std::{cmp, mem};
use std::time::Duration;

use Result;
use errno::Errno;

/// System info structure returned by `sysinfo`.
#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
pub struct SysInfo(libc::sysinfo);

impl SysInfo {
    /// Returns the load average tuple.
    ///
    /// The returned values represent the load average over time intervals of
    /// 1, 5, and 15 minutes, respectively.
    pub fn load_average(&self) -> (f64, f64, f64) {
        (
            self.0.loads[0] as f64 / (1 << SI_LOAD_SHIFT) as f64,
            self.0.loads[1] as f64 / (1 << SI_LOAD_SHIFT) as f64,
            self.0.loads[2] as f64 / (1 << SI_LOAD_SHIFT) as f64,
        )
    }

    /// Returns the time since system boot.
    pub fn uptime(&self) -> Duration {
        // Truncate negative values to 0
        Duration::from_secs(cmp::max(self.0.uptime, 0) as u64)
    }

    /// Current number of processes.
    pub fn process_count(&self) -> u16 {
        self.0.procs
    }

    /// Returns the amount of swap memory in Bytes.
    pub fn swap_total(&self) -> u64 {
        self.scale_mem(self.0.totalswap)
    }

    /// Returns the amount of unused swap memory in Bytes.
    pub fn swap_free(&self) -> u64 {
        self.scale_mem(self.0.freeswap)
    }

    /// Returns the total amount of installed RAM in Bytes.
    pub fn ram_total(&self) -> u64 {
        self.scale_mem(self.0.totalram)
    }

    /// Returns the amount of completely unused RAM in Bytes.
    ///
    /// "Unused" in this context means that the RAM in neither actively used by
    /// programs, nor by the operating system as disk cache or buffer. It is
    /// "wasted" RAM since it currently serves no purpose.
    pub fn ram_unused(&self) -> u64 {
        self.scale_mem(self.0.freeram)
    }

    fn scale_mem(&self, units: libc::c_ulong) -> u64 {
        units as u64 * self.0.mem_unit as u64
    }
}

/// Returns system information.
///
/// [See `sysinfo(2)`](http://man7.org/linux/man-pages/man2/sysinfo.2.html).
pub fn sysinfo() -> Result<SysInfo> {
    let mut info: libc::sysinfo = unsafe { mem::uninitialized() };
    let res = unsafe { libc::sysinfo(&mut info) };
    Errno::result(res).map(|_| SysInfo(info))
}
