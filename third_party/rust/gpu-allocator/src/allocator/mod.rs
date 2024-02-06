use std::{backtrace::Backtrace, sync::Arc};

use log::*;

use crate::result::*;

pub(crate) mod dedicated_block_allocator;
pub(crate) use dedicated_block_allocator::DedicatedBlockAllocator;

pub(crate) mod free_list_allocator;
pub(crate) use free_list_allocator::FreeListAllocator;

#[derive(PartialEq, Copy, Clone, Debug)]
#[repr(u8)]
pub(crate) enum AllocationType {
    Free,
    Linear,
    NonLinear,
}

impl AllocationType {
    #[cfg(feature = "visualizer")]
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Free => "Free",
            Self::Linear => "Linear",
            Self::NonLinear => "Non-Linear",
        }
    }
}

#[derive(Clone)]
pub(crate) struct AllocationReport {
    pub(crate) name: String,
    pub(crate) size: u64,
    #[cfg(feature = "visualizer")]
    pub(crate) backtrace: Arc<Backtrace>,
}

#[cfg(feature = "visualizer")]
pub(crate) trait SubAllocatorBase: crate::visualizer::SubAllocatorVisualizer {}
#[cfg(not(feature = "visualizer"))]
pub(crate) trait SubAllocatorBase {}

pub(crate) trait SubAllocator: SubAllocatorBase + std::fmt::Debug + Sync + Send {
    fn allocate(
        &mut self,
        size: u64,
        alignment: u64,
        allocation_type: AllocationType,
        granularity: u64,
        name: &str,
        backtrace: Arc<Backtrace>,
    ) -> Result<(u64, std::num::NonZeroU64)>;

    fn free(&mut self, chunk_id: Option<std::num::NonZeroU64>) -> Result<()>;

    fn rename_allocation(
        &mut self,
        chunk_id: Option<std::num::NonZeroU64>,
        name: &str,
    ) -> Result<()>;

    fn report_memory_leaks(
        &self,
        log_level: Level,
        memory_type_index: usize,
        memory_block_index: usize,
    );

    fn report_allocations(&self) -> Vec<AllocationReport>;

    #[must_use]
    fn supports_general_allocations(&self) -> bool;
    #[must_use]
    fn size(&self) -> u64;
    #[must_use]
    fn allocated(&self) -> u64;

    /// Helper function: reports how much memory is available in this suballocator
    #[must_use]
    fn available_memory(&self) -> u64 {
        self.size() - self.allocated()
    }

    /// Helper function: reports if the suballocator is empty (meaning, having no allocations).
    #[must_use]
    fn is_empty(&self) -> bool {
        self.allocated() == 0
    }
}

pub(crate) const VISUALIZER_TABLE_MAX_ENTRY_NAME_LEN: usize = 40;

pub(crate) fn fmt_bytes(mut amount: u64) -> String {
    const SUFFIX: [&str; 5] = ["B", "KB", "MB", "GB", "TB"];

    let mut idx = 0;
    let mut print_amount = amount as f64;
    loop {
        if amount < 1024 {
            return format!("{:.2} {}", print_amount, SUFFIX[idx]);
        }

        print_amount = amount as f64 / 1024.0;
        amount /= 1024;
        idx += 1;
    }
}
