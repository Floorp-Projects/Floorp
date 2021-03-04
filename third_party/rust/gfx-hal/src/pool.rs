//! Command pools

use crate::{command::Level, Backend, PseudoVec};

use std::{any::Any, fmt};

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
    /// # Arguments
    ///
    /// * `release_resources` - if `true`, this command pool will recycle all the
    ///   resources it own and give them back to the system.
    ///
    /// # Synchronization
    ///
    /// You may _not_ free the pool if a command buffer allocated from it
    /// is still in use (pool memory still in use).
    unsafe fn reset(&mut self, release_resources: bool);

    /// Allocate a single [command buffer][crate::command::CommandBuffer] from the pool.
    ///
    /// # Arguments
    ///
    /// * `level` - whether this command buffer is primary or secondary.
    unsafe fn allocate_one(&mut self, level: Level) -> B::CommandBuffer {
        let mut result = PseudoVec(None);
        self.allocate(1, level, &mut result);
        result.0.unwrap()
    }

    /// Allocate new [command buffers][crate::command::CommandBuffer] from the pool.
    ///
    /// # Arguments
    ///
    /// * `num` - how many buffers to return
    /// * `level` - whether to allocate primary or secondary command buffers.
    /// * `list` - an extendable list of command buffers into which to allocate.
    unsafe fn allocate<E>(&mut self, num: usize, level: Level, list: &mut E)
    where
        E: Extend<B::CommandBuffer>,
    {
        list.extend((0..num).map(|_| self.allocate_one(level)));
    }

    /// Free [command buffers][crate::command::CommandBuffer] allocated from this pool.
    unsafe fn free<I>(&mut self, buffers: I)
    where
        I: Iterator<Item = B::CommandBuffer>;
}
