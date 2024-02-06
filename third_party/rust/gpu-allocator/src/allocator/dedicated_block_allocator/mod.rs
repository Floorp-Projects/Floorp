#![deny(unsafe_code, clippy::unwrap_used)]

#[cfg(feature = "visualizer")]
pub(crate) mod visualizer;

use std::{backtrace::Backtrace, sync::Arc};

use log::{log, Level};

use super::{AllocationReport, AllocationType, SubAllocator, SubAllocatorBase};
use crate::{AllocationError, Result};

#[derive(Debug)]
pub(crate) struct DedicatedBlockAllocator {
    size: u64,
    allocated: u64,
    /// Only used if [`crate::AllocatorDebugSettings::store_stack_traces`] is [`true`]
    name: Option<String>,
    backtrace: Arc<Backtrace>,
}

impl DedicatedBlockAllocator {
    pub(crate) fn new(size: u64) -> Self {
        Self {
            size,
            allocated: 0,
            name: None,
            backtrace: Arc::new(Backtrace::disabled()),
        }
    }
}

impl SubAllocatorBase for DedicatedBlockAllocator {}
impl SubAllocator for DedicatedBlockAllocator {
    fn allocate(
        &mut self,
        size: u64,
        _alignment: u64,
        _allocation_type: AllocationType,
        _granularity: u64,
        name: &str,
        backtrace: Arc<Backtrace>,
    ) -> Result<(u64, std::num::NonZeroU64)> {
        if self.allocated != 0 {
            return Err(AllocationError::OutOfMemory);
        }

        if self.size != size {
            return Err(AllocationError::Internal(
                "DedicatedBlockAllocator size must match allocation size.".into(),
            ));
        }

        self.allocated = size;
        self.name = Some(name.to_string());
        self.backtrace = backtrace;

        #[allow(clippy::unwrap_used)]
        let dummy_id = std::num::NonZeroU64::new(1).unwrap();
        Ok((0, dummy_id))
    }

    fn free(&mut self, chunk_id: Option<std::num::NonZeroU64>) -> Result<()> {
        if chunk_id != std::num::NonZeroU64::new(1) {
            Err(AllocationError::Internal("Chunk ID must be 1.".into()))
        } else {
            self.allocated = 0;
            Ok(())
        }
    }

    fn rename_allocation(
        &mut self,
        chunk_id: Option<std::num::NonZeroU64>,
        name: &str,
    ) -> Result<()> {
        if chunk_id != std::num::NonZeroU64::new(1) {
            Err(AllocationError::Internal("Chunk ID must be 1.".into()))
        } else {
            self.name = Some(name.into());
            Ok(())
        }
    }

    fn report_memory_leaks(
        &self,
        log_level: Level,
        memory_type_index: usize,
        memory_block_index: usize,
    ) {
        let empty = "".to_string();
        let name = self.name.as_ref().unwrap_or(&empty);

        log!(
            log_level,
            r#"leak detected: {{
    memory type: {}
    memory block: {}
    dedicated allocation: {{
        size: 0x{:x},
        name: {},
        backtrace: {}
    }}
}}"#,
            memory_type_index,
            memory_block_index,
            self.size,
            name,
            self.backtrace
        )
    }

    fn report_allocations(&self) -> Vec<AllocationReport> {
        vec![AllocationReport {
            name: self
                .name
                .clone()
                .unwrap_or_else(|| "<Unnamed Dedicated allocation>".to_owned()),
            size: self.size,
            #[cfg(feature = "visualizer")]
            backtrace: self.backtrace.clone(),
        }]
    }

    fn size(&self) -> u64 {
        self.size
    }

    fn allocated(&self) -> u64 {
        self.allocated
    }

    fn supports_general_allocations(&self) -> bool {
        false
    }
}
