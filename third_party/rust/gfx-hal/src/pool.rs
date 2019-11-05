//! Command pools

use crate::command::Level;
use crate::Backend;

use smallvec::SmallVec;
use std::any::Any;
use std::fmt;

bitflags!(
    /// Command pool creation flags.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct CommandPoolCreateFlags: u8 {
        /// Indicates short-lived command buffers.
        /// Memory optimization hint for implementations.
        const TRANSIENT = 0x1;
        /// Allow command buffers to be reset individually.
        const RESET_INDIVIDUAL = 0x2;
    }
);

/// The allocated command buffers are associated with the creating command queue.
pub trait CommandPool<B: Backend>: fmt::Debug + Any + Send + Sync {
    /// Reset the command pool and the corresponding command buffers.
    ///
    /// # Synchronization: You may _not_ free the pool if a command buffer is still in use (pool memory still in use)
    unsafe fn reset(&mut self, release_resources: bool);

    /// Allocate a single command buffers from the pool.
    unsafe fn allocate_one(&mut self, level: Level) -> B::CommandBuffer {
        self.allocate_vec(1, level).pop().unwrap()
    }

    /// Allocate new command buffers from the pool.
    unsafe fn allocate_vec(&mut self, num: usize, level: Level) -> SmallVec<[B::CommandBuffer; 1]> {
        (0 .. num).map(|_| self.allocate_one(level)).collect()
    }

    /// Free command buffers which are allocated from this pool.
    unsafe fn free<I>(&mut self, buffers: I)
    where
        I: IntoIterator<Item = B::CommandBuffer>;
}
