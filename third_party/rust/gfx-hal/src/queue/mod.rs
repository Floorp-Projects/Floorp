//! Command queues.
//!
//! Queues are the execution paths of the graphical processing units. These process
//! submitted commands buffers.
//!
//! There are different types of queues, which can only handle associated command buffers.
//! `Queue<B, C>` has the capability defined by `C`: graphics, compute and transfer.

pub mod family;

use crate::{
    device::OutOfMemory,
    pso,
    window::{PresentError, PresentationSurface, Suboptimal},
    Backend,
};
use std::{any::Any, fmt};

pub use self::family::{QueueFamily, QueueFamilyId, QueueGroup};
use crate::memory::{SparseBind, SparseImageBind};

/// The type of the queue, an enum encompassing `queue::Capability`
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum QueueType {
    /// Supports all operations.
    General,
    /// Only supports graphics and transfer operations.
    Graphics,
    /// Only supports compute and transfer operations.
    Compute,
    /// Only supports transfer operations.
    Transfer,
}

impl QueueType {
    /// Returns true if the queue supports graphics operations.
    pub fn supports_graphics(&self) -> bool {
        match *self {
            QueueType::General | QueueType::Graphics => true,
            QueueType::Compute | QueueType::Transfer => false,
        }
    }
    /// Returns true if the queue supports compute operations.
    pub fn supports_compute(&self) -> bool {
        match *self {
            QueueType::General | QueueType::Graphics | QueueType::Compute => true,
            QueueType::Transfer => false,
        }
    }
    /// Returns true if the queue supports transfer operations.
    pub fn supports_transfer(&self) -> bool {
        true
    }
}

/// Scheduling hint for devices about the priority of a queue.  Values range from `0.0` (low) to
/// `1.0` (high).
pub type QueuePriority = f32;

/// Abstraction for an internal GPU execution engine.
///
/// Commands are executed on the the device by submitting
/// [command buffers][crate::command::CommandBuffer].
///
/// Queues can also be used for presenting to a surface
/// (that is, flip the front buffer with the next one in the chain).
pub trait Queue<B: Backend>: fmt::Debug + Any + Send + Sync {
    /// Sparse memory bind operation.
    ///
    /// # Arguments
    ///
    /// * `info` - information about the memory bindings.
    ///
    /// # Safety
    ///
    /// - Defining memory as `None` will cause undefined behaviour when the
    /// tile is read or written from in some hardware.
    /// - The memory regions provided are not checked to be valid and matching
    /// of the sparse resource type.
    /// - If extents are not a multiple of the block size, additional space will be
    /// bound, and accessing memory is unsafe.
    unsafe fn bind_sparse<'a, Iw, Is, Ibi, Ib, Iii, Io, Ii>(
        &mut self,
        _wait_semaphores: Iw,
        _signal_semaphores: Is,
        _buffer_memory_binds: Ib,
        _image_opaque_memory_binds: Io,
        _image_memory_binds: Ii,
        _device: &B::Device,
        _fence: Option<&B::Fence>,
    ) where
        Ibi: Iterator<Item = &'a SparseBind<&'a B::Memory>>,
        Ib: Iterator<Item = (&'a mut B::Buffer, Ibi)>,
        Iii: Iterator<Item = &'a SparseImageBind<&'a B::Memory>>,
        Io: Iterator<Item = (&'a mut B::Image, Ibi)>,
        Ii: Iterator<Item = (&'a mut B::Image, Iii)>,
        Iw: Iterator<Item = &'a B::Semaphore>,
        Is: Iterator<Item = &'a B::Semaphore>,
    {
        unimplemented!()
    }

    /// Submit command buffers to queue for execution.
    ///
    /// # Arguments
    ///
    /// * `command_buffers` - command buffers to submit.
    /// * `wait_semaphores` - semaphores to wait being signalled before submission.
    /// * `signal_semaphores` - semaphores to signal after all command buffers
    ///   in the submission have finished execution.
    /// * `fence` - must be in unsignaled state, and will be signaled after
    ///   all command buffers in the submission have finished execution.
    ///
    /// # Safety
    ///
    /// It's not checked that the queue can process the submitted command buffers.
    ///
    /// For example, trying to submit compute commands to a graphics queue
    /// will result in undefined behavior.
    unsafe fn submit<'a, Ic, Iw, Is>(
        &mut self,
        command_buffers: Ic,
        wait_semaphores: Iw,
        signal_semaphores: Is,
        fence: Option<&mut B::Fence>,
    ) where
        Ic: Iterator<Item = &'a B::CommandBuffer>,
        Iw: Iterator<Item = (&'a B::Semaphore, pso::PipelineStage)>,
        Is: Iterator<Item = &'a B::Semaphore>;

    /// Present a swapchain image directly to a surface, after waiting on `wait_semaphore`.
    ///
    /// # Safety
    ///
    /// Unsafe for the same reasons as [`submit`][Queue::submit].
    /// No checks are performed to verify that this queue supports present operations.
    unsafe fn present(
        &mut self,
        surface: &mut B::Surface,
        image: <B::Surface as PresentationSurface<B>>::SwapchainImage,
        wait_semaphore: Option<&mut B::Semaphore>,
    ) -> Result<Option<Suboptimal>, PresentError>;

    /// Wait for the queue to be idle.
    fn wait_idle(&mut self) -> Result<(), OutOfMemory>;

    /// The amount of nanoseconds that causes a timestamp query value to increment by one.
    fn timestamp_period(&self) -> f32;
}
