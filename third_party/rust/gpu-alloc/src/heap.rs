#[derive(Debug)]
pub(crate) struct Heap {
    size: u64,
    used: u64,
    allocated: u128,
    deallocated: u128,
}

impl Heap {
    pub(crate) fn new(size: u64) -> Self {
        Heap {
            size,
            used: 0,
            allocated: 0,
            deallocated: 0,
        }
    }

    pub(crate) fn size(&mut self) -> u64 {
        self.size
    }

    pub(crate) fn alloc(&mut self, size: u64) {
        self.used += size;
        self.allocated += u128::from(size);
    }

    pub(crate) fn dealloc(&mut self, size: u64) {
        self.used -= size;
        self.deallocated += u128::from(size);
    }
}
