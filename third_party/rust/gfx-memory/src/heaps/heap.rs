use crate::{
    stats::{MemoryHeapUtilization, MemoryUtilization},
    Size,
};

#[derive(Debug)]
pub(super) struct MemoryHeap {
    size: Size,
    used: Size,
    effective: Size,
}

impl MemoryHeap {
    pub(super) fn new(size: Size) -> Self {
        MemoryHeap {
            size,
            used: 0,
            effective: 0,
        }
    }

    pub(super) fn available(&self) -> Size {
        if self.used > self.size {
            log::warn!("Heap size exceeded");
            0
        } else {
            self.size - self.used
        }
    }

    pub(super) fn allocated(&mut self, used: Size, effective: Size) {
        self.used += used;
        self.effective += effective;
        debug_assert!(self.used >= self.effective);
    }

    pub(super) fn freed(&mut self, used: Size, effective: Size) {
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
