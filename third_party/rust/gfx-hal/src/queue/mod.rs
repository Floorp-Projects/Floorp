//! Command queues.
//!
//! Queues are the execution paths of the graphical processing units. These process
//! submitted commands buffers.
//!
//! There are different types of queues, which can only handle associated command buffers.
//! `CommandQueue<B, C>` has the capability defined by `C`: graphics, compute and transfer.

pub mod family;

use crate::{
    device::OutOfMemory,
    pso,
    window::{PresentError, PresentationSurface, Suboptimal, SwapImageIndex},
    Backend,
};
use std::{any::Any, borrow::Borrow, fmt, iter};

pub use self::family::{QueueFamily, QueueFamilyId, QueueGroup};

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

/// Submission information for a command queue.
#[derive(Debug)]
pub struct Submission<Ic, Iw, Is> {
    /// Command buffers to submit.
    pub command_buffers: Ic,
    /// Semaphores to wait being signalled before submission.
    pub wait_semaphores: Iw,
    /// Semaphores to signal after all command buffers in the submission have finished execution.
    pub signal_semaphores: Is,
}

/// `RawCommandQueue` are abstractions to the internal GPU execution engines.
/// Commands are executed on the the device by submitting command buffers to queues.
pub trait CommandQueue<B: Backend>: fmt::Debug + Any + Send + Sync {
    /// Submit command buffers to queue for execution.
    /// `fence` must be in unsignalled state, and will be signalled after all command buffers in the submission have
    /// finished execution.
    ///
    /// Unsafe because it's not checked that the queue can process the submitted command buffers.
    /// Trying to submit compute commands to a graphics queue will result in undefined behavior.
    /// Each queue implements safer wrappers according to their supported functionalities!
    unsafe fn submit<'a, T, Ic, S, Iw, Is>(
        &mut self,
        submission: Submission<Ic, Iw, Is>,
        fence: Option<&B::Fence>,
    ) where
        T: 'a + Borrow<B::CommandBuffer>,
        Ic: IntoIterator<Item = &'a T>,
        S: 'a + Borrow<B::Semaphore>,
        Iw: IntoIterator<Item = (&'a S, pso::PipelineStage)>,
        Is: IntoIterator<Item = &'a S>;

    /// Simplified version of `submit` that doesn't expect any semaphores.
    unsafe fn submit_without_semaphores<'a, T, Ic>(
        &mut self,
        command_buffers: Ic,
        fence: Option<&B::Fence>,
    ) where
        T: 'a + Borrow<B::CommandBuffer>,
        Ic: IntoIterator<Item = &'a T>,
    {
        let submission = Submission {
            command_buffers,
            wait_semaphores: iter::empty(),
            signal_semaphores: iter::empty(),
        };
        self.submit::<_, _, B::Semaphore, _, _>(submission, fence)
    }

    /// Presents the result of the queue to the given swapchains, after waiting on all the
    /// semaphores given in `wait_semaphores`. A given swapchain must not appear in this
    /// list more than once.
    ///
    /// Unsafe for the same reasons as `submit()`.
    unsafe fn present<'a, W, Is, S, Iw>(
        &mut self,
        swapchains: Is,
        wait_semaphores: Iw,
    ) -> Result<Option<Suboptimal>, PresentError>
    where
        Self: Sized,
        W: 'a + Borrow<B::Swapchain>,
        Is: IntoIterator<Item = (&'a W, SwapImageIndex)>,
        S: 'a + Borrow<B::Semaphore>,
        Iw: IntoIterator<Item = &'a S>;

    /// Simplified version of `present` that doesn't expect any semaphores.
    unsafe fn present_without_semaphores<'a, W, Is>(
        &mut self,
        swapchains: Is,
    ) -> Result<Option<Suboptimal>, PresentError>
    where
        Self: Sized,
        W: 'a + Borrow<B::Swapchain>,
        Is: IntoIterator<Item = (&'a W, SwapImageIndex)>,
    {
        self.present::<_, _, B::Semaphore, _>(swapchains, iter::empty())
    }

    /// Present the a
    unsafe fn present_surface(
        &mut self,
        surface: &mut B::Surface,
        image: <B::Surface as PresentationSurface<B>>::SwapchainImage,
        wait_semaphore: Option<&B::Semaphore>,
    ) -> Result<Option<Suboptimal>, PresentError>;

    /// Wait for the queue to idle.
    fn wait_idle(&self) -> Result<(), OutOfMemory>;
}
