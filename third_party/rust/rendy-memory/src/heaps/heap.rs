use crate::utilization::*;

#[derive(Debug)]
pub(super) struct MemoryHeap {
    size: u64,
    used: u64,
    effective: u64,
}

impl MemoryHeap {
    pub(super) fn new(size: u64) -> Self {
        MemoryHeap {
            size,
            used: 0,
            effective: 0,
        }
    }

    pub(super) fn available(&self) -> u64 {
        if self.used > self.size {
            log::warn!("Heap size exceeded");
            0
        } else {
            self.size - self.used
        }
    }

    pub(super) fn allocated(&mut self, used: u64, effective: u64) {
        self.used += used;
        self.effective += effective;
        debug_assert!(self.used >= self.effective);
    }

    pub(super) fn freed(&mut self, used: u64, effective: u64) {
        self.used -= used;
        self.effective -= effective;
        debug_assert!(self.used >= self.effective);
    }

    pub(super) fn utilization(&self) -> MemoryHeapUtilization {
        MemoryHeapUtilization {
            utilization: MemoryUtilization {
                used: self.used,
                effective: self.effective,
            },
            size: self.size,
        }
    }
}
