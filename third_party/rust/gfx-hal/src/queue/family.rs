//! Queue family and groups.

use crate::{queue::QueueType, Backend};

use std::{any::Any, fmt::Debug};

/// General information about a queue family, available upon adapter discovery.
///
/// Note that a backend can expose multiple queue families with the same properties.
///
/// Can be obtained from an [adapter][crate::adapter::Adapter] through its
/// [`queue_families`][crate::adapter::Adapter::queue_families] field.
pub trait QueueFamily: Debug + Any + Send + Sync {
    /// Returns the type of queues.
    fn queue_type(&self) -> QueueType;
    /// Returns maximum number of queues created from this family.
    fn max_queues(&self) -> usize;
    /// Returns the queue family ID.
    fn id(&self) -> QueueFamilyId;
    /// Returns true if the queue family supports sparse binding
    fn supports_sparse_binding(&self) -> bool;
}

/// Identifier for a queue family of a physical device.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct QueueFamilyId(pub usize);

/// Bare-metal queue group.
///
/// Denotes all queues created from one queue family.
#[derive(Debug)]
pub struct QueueGroup<B: Backend> {
    /// Family index for the queues in this group.
    pub family: QueueFamilyId,
    /// List of queues.
    pub queues: Vec<B::Queue>,
}

impl<B: Backend> QueueGroup<B> {
    /// Create a new, empty queue group for a queue family.
    pub fn new(family: QueueFamilyId) -> Self {
        QueueGroup {
            family,
            queues: Vec::new(),
        }
    }

    /// Add a command queue to the group.
    ///
    /// The queue needs to be created from this queue family.
    pub fn add_queue(&mut self, queue: B::Queue) {
        self.queues.push(queue);
    }
}
