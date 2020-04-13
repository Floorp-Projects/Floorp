use crate::mapping::MappedRange;
use hal::memory as m;

/// Block that owns a `Segment` of the `Memory`.
/// Implementor must ensure that there can't be any other blocks
/// with overlapping range (either through type system or safety notes for unsafe functions).
/// Provides access to safe memory range mapping.
pub trait Block<B: hal::Backend> {
    /// Get memory properties of the block.
    fn properties(&self) -> m::Properties;

    /// Get raw memory object.
    fn memory(&self) -> &B::Memory;

    /// Get memory segment owned by this block.
    fn segment(&self) -> m::Segment;

    /// Get mapping for the block segment.
    /// Memory writes to the region performed by device become available for the host.
    fn map<'a>(
        &'a mut self,
        device: &B::Device,
        segment: m::Segment,
    ) -> Result<MappedRange<'a, B>, hal::device::MapError>;
}
