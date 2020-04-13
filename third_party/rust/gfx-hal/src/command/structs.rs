use crate::{buffer, image};

use std::ops::Range;

/// Specifies a source region and a destination
/// region in a buffer for copying.  All values
/// are in units of bytes.
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct BufferCopy {
    /// Buffer region source offset.
    pub src: buffer::Offset,
    /// Buffer region destination offset.
    pub dst: buffer::Offset,
    /// Region size.
    pub size: buffer::Offset,
}

/// Bundles together all the parameters needed to copy data from one `Image`
/// to another.
#[derive(Clone, Debug)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct ImageCopy {
    /// The image subresource to copy from.
    pub src_subresource: image::SubresourceLayers,
    /// The source offset.
    pub src_offset: image::Offset,
    /// The image subresource to copy to.
    pub dst_subresource: image::SubresourceLayers,
    /// The destination offset.
    pub dst_offset: image::Offset,
    /// The extent of the region to copy.
    pub extent: image::Extent,
}

/// Bundles together all the parameters needed to copy a buffer
/// to an image or vice-versa.
#[derive(Clone, Debug)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct BufferImageCopy {
    /// Buffer offset in bytes.
    pub buffer_offset: buffer::Offset,
    /// Width of a buffer 'row' in texels.
    pub buffer_width: u32,
    /// Height of a buffer 'image slice' in texels.
    pub buffer_height: u32,
    /// The image subresource.
    pub image_layers: image::SubresourceLayers,
    /// The offset of the portion of the image to copy.
    pub image_offset: image::Offset,
    /// Size of the portion of the image to copy.
    pub image_extent: image::Extent,
}

/// Parameters for an image resolve operation,
/// where a multi-sampled image is copied into a single-sampled
/// image.
#[derive(Clone, Debug)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct ImageResolve {
    /// Source image and layers.
    pub src_subresource: image::SubresourceLayers,
    /// Source image offset.
    pub src_offset: image::Offset,
    /// Destination image and layers.
    pub dst_subresource: image::SubresourceLayers,
    /// Destination image offset.
    pub dst_offset: image::Offset,
    /// Image extent.
    pub extent: image::Extent,
}

/// Parameters for an image blit operation, where a portion of one image
/// is copied into another, possibly with scaling and filtering.
#[derive(Clone, Debug)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct ImageBlit {
    /// Source image and layers.
    pub src_subresource: image::SubresourceLayers,
    /// Source image bounds.
    pub src_bounds: Range<image::Offset>,
    /// Destination image and layers.
    pub dst_subresource: image::SubresourceLayers,
    /// Destination image bounds.
    pub dst_bounds: Range<image::Offset>,
}
