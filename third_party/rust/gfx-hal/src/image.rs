//! Image related structures.
//!
//! An image is a block of GPU memory representing a grid of texels.

use crate::{
    buffer::Offset as RawOffset,
    device, format,
    pso::{Comparison, Rect},
};
use std::{f32, hash, ops::Range};

/// Dimension size.
pub type Size = u32;
/// Number of MSAA samples.
pub type NumSamples = u8;
/// Image layer.
pub type Layer = u16;
/// Image mipmap level.
pub type Level = u8;
/// Maximum accessible mipmap level of an image.
pub const MAX_LEVEL: Level = 15;
/// A texel coordinate in an image.
pub type TexelCoordinate = i32;

/// Describes the size of an image, which may be up to three dimensional.
#[derive(Clone, Copy, Debug, Default, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Extent {
    /// Image width
    pub width: Size,
    /// Image height
    pub height: Size,
    /// Image depth.
    pub depth: Size,
}

impl Extent {
    /// Return true if one of the dimensions is zero.
    pub fn is_empty(&self) -> bool {
        self.width == 0 || self.height == 0 || self.depth == 0
    }
    /// Get the extent at a particular mipmap level.
    pub fn at_level(&self, level: Level) -> Self {
        Extent {
            width: 1.max(self.width >> level),
            height: 1.max(self.height >> level),
            depth: 1.max(self.depth >> level),
        }
    }
    /// Get a rectangle for the full area of extent.
    pub fn rect(&self) -> Rect {
        Rect {
            x: 0,
            y: 0,
            w: self.width as i16,
            h: self.height as i16,
        }
    }
}

/// An offset into an `Image` used for image-to-image
/// copy operations.  All offsets are in texels, and
/// specifying offsets other than 0 for dimensions
/// that do not exist is undefined behavior -- for
/// example, specifying a `z` offset of `1` in a
/// two-dimensional image.
#[derive(Clone, Copy, Debug, Default, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Offset {
    /// X offset.
    pub x: TexelCoordinate,
    /// Y offset.
    pub y: TexelCoordinate,
    /// Z offset.
    pub z: TexelCoordinate,
}

impl Offset {
    /// Zero offset shortcut.
    pub const ZERO: Self = Offset { x: 0, y: 0, z: 0 };

    /// Convert the offset into 2-sided bounds given the extent.
    pub fn into_bounds(self, extent: &Extent) -> Range<Offset> {
        let end = Offset {
            x: self.x + extent.width as i32,
            y: self.y + extent.height as i32,
            z: self.z + extent.depth as i32,
        };
        self..end
    }
}

/// Image tiling modes.
#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum Tiling {
    /// Optimal tiling for GPU memory access. Implementation-dependent.
    Optimal = 0,
    /// Optimal for CPU read/write. Texels are laid out in row-major order,
    /// possibly with some padding on each row.
    Linear = 1,
}

/// Pure image object creation error.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
pub enum CreationError {
    /// Out of either host or device memory.
    #[error(transparent)]
    OutOfMemory(#[from] device::OutOfMemory),
    /// The format is not supported by the device.
    #[error("Unsupported format: {0:?}")]
    Format(format::Format),
    /// The kind doesn't support a particular operation.
    #[error("Specified kind doesn't support particular operation")]
    Kind,
    /// Failed to map a given multisampled kind to the device.
    #[error("Specified format doesn't support specified sampling {0:}")]
    Samples(NumSamples),
    /// Unsupported size in one of the dimensions.
    #[error("Unsupported size in one of the dimensions {0:}")]
    Size(Size),
    /// The given data has a different size than the target image slice.
    #[error("The given data has a different size ({0:}) than the target image slice")]
    Data(usize),
    /// The mentioned usage mode is not supported
    #[error("Unsupported usage: {0:?}")]
    Usage(Usage),
}

/// Error creating an `ImageView`.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
pub enum ViewCreationError {
    /// Out of either Host or Device memory
    #[error(transparent)]
    OutOfMemory(#[from] device::OutOfMemory),
    /// The required usage flag is not present in the image.
    #[error("Specified usage flags are not present in the image {0:?}")]
    Usage(Usage),
    /// Selected mip level doesn't exist.
    #[error("Selected level doesn't exist in the image {0:}")]
    Level(Level),
    /// Selected array layer doesn't exist.
    #[error(transparent)]
    Layer(#[from] LayerError),
    /// An incompatible format was requested for the view.
    #[error("Incompatible format: {0:?}")]
    BadFormat(format::Format),
    /// An incompatible view kind was requested for the view.
    #[error("Incompatible kind: {0:?}")]
    BadKind(ViewKind),
    /// The backend refused for some reason.
    #[error("Implementation specific error occurred")]
    Unsupported,
}

/// An error associated with selected image layer.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
pub enum LayerError {
    /// The source image kind doesn't support array slices.
    #[error("Source image doesn't support view kind {0:?}")]
    NotExpected(Kind),
    /// Selected layers are outside of the provided range.
    #[error("Selected layers are out of bounds")]
    OutOfBounds,
}

/// How to [filter](https://en.wikipedia.org/wiki/Texture_filtering) the
/// image when sampling. They correspond to increasing levels of quality,
/// but also cost.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum Filter {
    /// Selects a single texel from the current mip level and uses its value.
    ///
    /// Mip filtering selects the filtered value from one level.
    Nearest,
    /// Selects multiple texels and calculates the value via multivariate interpolation.
    ///     * 1D: Linear interpolation
    ///     * 2D/Cube: Bilinear interpolation
    ///     * 3D: Trilinear interpolation
    Linear,
}

/// The face of a cube image to do an operation on.
#[allow(missing_docs)]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum CubeFace {
    PosX,
    NegX,
    PosY,
    NegY,
    PosZ,
    NegZ,
}

/// A constant array of cube faces in the order they map to the hardware.
pub const CUBE_FACES: [CubeFace; 6] = [
    CubeFace::PosX,
    CubeFace::NegX,
    CubeFace::PosY,
    CubeFace::NegY,
    CubeFace::PosZ,
    CubeFace::NegZ,
];

/// Specifies the dimensionality of an image to be allocated,
/// along with the number of mipmap layers and MSAA samples
/// if applicable.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum Kind {
    /// A single one-dimensional row of texels.
    D1(Size, Layer),
    /// Two-dimensional image.
    D2(Size, Size, Layer, NumSamples),
    /// Volumetric image.
    D3(Size, Size, Size),
}

impl Kind {
    /// Get the image extent.
    pub fn extent(&self) -> Extent {
        match *self {
            Kind::D1(width, _) => Extent {
                width,
                height: 1,
                depth: 1,
            },
            Kind::D2(width, height, _, _) => Extent {
                width,
                height,
                depth: 1,
            },
            Kind::D3(width, height, depth) => Extent {
                width,
                height,
                depth,
            },
        }
    }

    /// Get the extent of a particular mipmap level.
    pub fn level_extent(&self, level: Level) -> Extent {
        use std::cmp::{max, min};
        // must be at least 1
        let map = |val| max(min(val, 1), val >> min(level, MAX_LEVEL));
        match *self {
            Kind::D1(w, _) => Extent {
                width: map(w),
                height: 1,
                depth: 1,
            },
            Kind::D2(w, h, _, _) => Extent {
                width: map(w),
                height: map(h),
                depth: 1,
            },
            Kind::D3(w, h, d) => Extent {
                width: map(w),
                height: map(h),
                depth: map(d),
            },
        }
    }

    /// Count the number of mipmap levels.
    pub fn compute_num_levels(&self) -> Level {
        use std::cmp::max;
        match *self {
            Kind::D2(_, _, _, s) if s > 1 => {
                // anti-aliased images can't have mipmaps
                1
            }
            _ => {
                let extent = self.extent();
                let dominant = max(max(extent.width, extent.height), extent.depth);
                (1..).find(|level| dominant >> level == 0).unwrap()
            }
        }
    }

    /// Return the number of layers in an array type.
    ///
    /// Each cube face counts as separate layer.
    pub fn num_layers(&self) -> Layer {
        match *self {
            Kind::D1(_, a) | Kind::D2(_, _, a, _) => a,
            Kind::D3(..) => 1,
        }
    }

    /// Return the number of MSAA samples for the kind.
    pub fn num_samples(&self) -> NumSamples {
        match *self {
            Kind::D1(..) => 1,
            Kind::D2(_, _, _, s) => s,
            Kind::D3(..) => 1,
        }
    }
}

/// Specifies the kind/dimensionality of an image view.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum ViewKind {
    /// A single one-dimensional row of texels.
    D1,
    /// An array of rows of texels. Equivalent to `D2` except that texels
    /// in different rows are not sampled, so filtering will be constrained
    /// to a single row of texels at a time.
    D1Array,
    /// A traditional 2D image, with rows arranged contiguously.
    D2,
    /// An array of 2D images. Equivalent to `D3` except that texels in
    /// a different depth level are not sampled.
    D2Array,
    /// A volume image, with each 2D layer arranged contiguously.
    D3,
    /// A set of 6 2D images, one for each face of a cube.
    Cube,
    /// An array of Cube images.
    CubeArray,
}

bitflags!(
    /// Capabilities to create views into an image.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    #[derive(Default)]
    pub struct ViewCapabilities: u32 {
        /// Support creation of views with different formats.
        const MUTABLE_FORMAT = 0x0000_0008;
        /// Support creation of `Cube` and `CubeArray` kinds of views.
        const KIND_CUBE      = 0x0000_0010;
        /// Support creation of `D2Array` kind of view.
        const KIND_2D_ARRAY  = 0x0000_0020;
    }
);

bitflags!(
    /// TODO: Find out if TRANSIENT_ATTACHMENT + INPUT_ATTACHMENT
    /// are applicable on backends other than Vulkan. --AP
    /// Image usage flags
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Usage: u32 {
        /// The image is used as a transfer source.
        const TRANSFER_SRC = 0x1;
        /// The image is used as a transfer destination.
        const TRANSFER_DST = 0x2;
        /// The image is a [sampled image](https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#descriptorsets-sampledimage)
        const SAMPLED = 0x4;
        /// The image is a [storage image](https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#descriptorsets-storageimage)
        const STORAGE = 0x8;
        /// The image is used as a color attachment -- that is, color input to a rendering pass.
        const COLOR_ATTACHMENT = 0x10;
        /// The image is used as a depth attachment.
        const DEPTH_STENCIL_ATTACHMENT = 0x20;
        ///
        const TRANSIENT_ATTACHMENT = 0x40;
        ///
        const INPUT_ATTACHMENT = 0x80;

    }
);

impl Usage {
    /// Returns true if this image can be used in transfer operations.
    pub fn can_transfer(&self) -> bool {
        self.intersects(Usage::TRANSFER_SRC | Usage::TRANSFER_DST)
    }

    /// Returns true if this image can be used as a target.
    pub fn can_target(&self) -> bool {
        self.intersects(Usage::COLOR_ATTACHMENT | Usage::DEPTH_STENCIL_ATTACHMENT)
    }
}

/// Specifies how image coordinates outside the range `[0, 1]` are handled.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum WrapMode {
    /// Tile the image, that is, sample the coordinate modulo `1.0`, so
    /// addressing the image beyond an edge will "wrap" back from the
    /// other edge.
    Tile,
    /// Mirror the image. Like tile, but uses abs(coord) before the modulo.
    Mirror,
    /// Clamp the image to the value at `0.0` or `1.0` respectively.
    Clamp,
    /// Use border color.
    Border,
    /// Mirror once and clamp to edge otherwise.
    ///
    /// Only valid if `Features::SAMPLER_MIRROR_CLAMP_EDGE` is enabled.
    MirrorClamp,
}

/// Specifies how the image texels in the filter kernel are reduced to a single value.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum ReductionMode {
    ///
    WeightedAverage,
    ///
    /// Only valid if `Features::SAMPLER_FILTER_MINMAX` is enabled.
    Minimum,
    ///
    /// Only valid if `Features::SAMPLER_FILTER_MINMAX` is enabled.
    Maximum,
}

/// A wrapper for the LOD level of an image. Needed so that we can
/// implement Eq and Hash for it.
#[derive(Clone, Copy, Debug, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Lod(pub f32);

impl Lod {
    /// Possible LOD range.
    pub const RANGE: Range<Self> = Lod(f32::MIN)..Lod(f32::MAX);
}

impl Eq for Lod {}
impl hash::Hash for Lod {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.0.to_bits().hash(state)
    }
}

/// A wrapper for an RGBA color with 8 bits per texel, encoded as a u32.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct PackedColor(pub u32);

impl From<[f32; 4]> for PackedColor {
    fn from(c: [f32; 4]) -> PackedColor {
        PackedColor(
            c.iter()
                .rev()
                .fold(0, |u, &c| (u << 8) + (c * 255.0) as u32),
        )
    }
}

impl Into<[f32; 4]> for PackedColor {
    fn into(self) -> [f32; 4] {
        let mut out = [0.0; 4];
        for (i, channel) in out.iter_mut().enumerate() {
            let byte = (self.0 >> (i << 3)) & 0xFF;
            *channel = byte as f32 / 255.0;
        }
        out
    }
}

/// The border color for `WrapMode::Border` wrap mode.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum BorderColor {
    ///
    TransparentBlack,
    ///
    OpaqueBlack,
    ///
    OpaqueWhite,
}

impl Into<[f32; 4]> for BorderColor {
    fn into(self) -> [f32; 4] {
        match self {
            BorderColor::TransparentBlack => [0.0, 0.0, 0.0, 0.0],
            BorderColor::OpaqueBlack => [0.0, 0.0, 0.0, 1.0],
            BorderColor::OpaqueWhite => [1.0, 1.0, 1.0, 1.0],
        }
    }
}

/// Specifies how to sample from an image.  These are all the parameters
/// available that alter how the GPU goes from a coordinate in an image
/// to producing an actual value from the texture, including filtering/
/// scaling, wrap mode, etc.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct SamplerDesc {
    /// Minification filter method to use.
    pub min_filter: Filter,
    /// Magnification filter method to use.
    pub mag_filter: Filter,
    /// Mip filter method to use.
    pub mip_filter: Filter,
    /// Reduction mode over the filter.
    pub reduction_mode: ReductionMode,
    /// Wrapping mode for each of the U, V, and W axis (S, T, and R in OpenGL
    /// speak).
    pub wrap_mode: (WrapMode, WrapMode, WrapMode),
    /// This bias is added to every computed mipmap level (N + lod_bias). For
    /// example, if it would select mipmap level 2 and lod_bias is 1, it will
    /// use mipmap level 3.
    pub lod_bias: Lod,
    /// This range is used to clamp LOD level used for sampling.
    pub lod_range: Range<Lod>,
    /// Comparison mode, used primary for a shadow map.
    pub comparison: Option<Comparison>,
    /// Border color is used when one of the wrap modes is set to border.
    pub border: BorderColor,
    /// Specifies whether the texture coordinates are normalized.
    pub normalized: bool,
    /// Anisotropic filtering.
    ///
    /// Can be `Some(_)` only if `Features::SAMPLER_ANISOTROPY` is enabled.
    pub anisotropy_clamp: Option<u8>,
}

impl SamplerDesc {
    /// Create a new sampler description with a given filter method for all filtering operations
    /// and a wrapping mode, using no LOD modifications.
    pub fn new(filter: Filter, wrap: WrapMode) -> Self {
        SamplerDesc {
            min_filter: filter,
            mag_filter: filter,
            mip_filter: filter,
            reduction_mode: ReductionMode::WeightedAverage,
            wrap_mode: (wrap, wrap, wrap),
            lod_bias: Lod(0.0),
            lod_range: Lod::RANGE.clone(),
            comparison: None,
            border: BorderColor::TransparentBlack,
            normalized: true,
            anisotropy_clamp: None,
        }
    }
}

/// Specifies options for how memory for an image is arranged.
/// These are hints to the GPU driver and may or may not have actual
/// performance effects, but describe constraints on how the data
/// may be used that a program *must* obey. They do not specify
/// how channel values or such are laid out in memory; the actual
/// image data is considered opaque.
///
/// Details may be found in [the Vulkan spec](https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#resources-image-layouts)
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum Layout {
    /// General purpose, no restrictions on usage.
    General,
    /// Must only be used as a color attachment in a framebuffer.
    ColorAttachmentOptimal,
    /// Must only be used as a depth attachment in a framebuffer.
    DepthStencilAttachmentOptimal,
    /// Must only be used as a depth attachment in a framebuffer,
    /// or as a read-only depth or stencil buffer in a shader.
    DepthStencilReadOnlyOptimal,
    /// Must only be used as a read-only image in a shader.
    ShaderReadOnlyOptimal,
    /// Must only be used as the source for a transfer command.
    TransferSrcOptimal,
    /// Must only be used as the destination for a transfer command.
    TransferDstOptimal,
    /// No layout, does not support device access.  Only valid as a
    /// source layout when transforming data to a specific destination
    /// layout or initializing data.  Does NOT guarentee that the contents
    /// of the source buffer are preserved.
    Undefined,
    /// Like `Undefined`, but does guarentee that the contents of the source
    /// buffer are preserved.
    Preinitialized,
    /// The layout that an image must be in to be presented to the display.
    Present,
}

impl Default for Layout {
    fn default() -> Self {
        Self::General
    }
}

bitflags!(
    /// Bitflags to describe how memory in an image or buffer can be accessed.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Access: u32 {
        /// Read access to an input attachment from within a fragment shader.
        const INPUT_ATTACHMENT_READ = 0x10;
        /// Read-only state for SRV access, or combine with `SHADER_WRITE` to have r/w access to UAV.
        const SHADER_READ = 0x20;
        /// Writeable state for UAV access.
        /// Combine with `SHADER_READ` to have r/w access to UAV.
        const SHADER_WRITE = 0x40;
        /// Read state but can only be combined with `COLOR_ATTACHMENT_WRITE`.
        const COLOR_ATTACHMENT_READ = 0x80;
        /// Write-only state but can be combined with `COLOR_ATTACHMENT_READ`.
        const COLOR_ATTACHMENT_WRITE = 0x100;
        /// Read access to a depth/stencil attachment in a depth or stencil operation.
        const DEPTH_STENCIL_ATTACHMENT_READ = 0x200;
        /// Write access to a depth/stencil attachment in a depth or stencil operation.
        const DEPTH_STENCIL_ATTACHMENT_WRITE = 0x400;
        /// Read access to the buffer in a copy operation.
        const TRANSFER_READ = 0x800;
        /// Write access to the buffer in a copy operation.
        const TRANSFER_WRITE = 0x1000;
        /// Read access for raw memory to be accessed by the host system (ie, CPU).
        const HOST_READ = 0x2000;
        /// Write access for raw memory to be accessed by the host system.
        const HOST_WRITE = 0x4000;
        /// Read access for memory to be accessed by a non-specific entity.  This may
        /// be the host system, or it may be something undefined or specified by an
        /// extension.
        const MEMORY_READ = 0x8000;
        /// Write access for memory to be accessed by a non-specific entity.
        const MEMORY_WRITE = 0x10000;
    }
);

/// Image state, combining access methods and the image's layout.
pub type State = (Access, Layout);

/// Selector of a concrete subresource in an image.
#[derive(Clone, Copy, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Subresource {
    /// Included aspects: color/depth/stencil
    pub aspects: format::Aspects,
    /// Selected mipmap level
    pub level: Level,
    /// Selected array level
    pub layer: Layer,
}

/// A subset of resource layers contained within an image's level.
#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct SubresourceLayers {
    /// Included aspects: color/depth/stencil
    pub aspects: format::Aspects,
    /// Selected mipmap level
    pub level: Level,
    /// Included array levels
    pub layers: Range<Layer>,
}

/// A subset of resources contained within an image.
#[derive(Clone, Debug, Default, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct SubresourceRange {
    /// Included aspects: color/depth/stencil
    pub aspects: format::Aspects,
    /// First mipmap level in this subresource
    pub level_start: Level,
    /// Number of sequential levels in this subresource.
    ///
    /// A value of `None` indicates the subresource contains
    /// all of the remaining levels.
    pub level_count: Option<Level>,
    /// First layer in this subresource
    pub layer_start: Layer,
    /// Number of sequential layers in this subresource.
    ///
    /// A value of `None` indicates the subresource contains
    /// all of the remaining layers.
    pub layer_count: Option<Layer>,
}

impl From<SubresourceLayers> for SubresourceRange {
    fn from(sub: SubresourceLayers) -> Self {
        SubresourceRange {
            aspects: sub.aspects,
            level_start: sub.level,
            level_count: Some(1),
            layer_start: sub.layers.start,
            layer_count: Some(sub.layers.end - sub.layers.start),
        }
    }
}

impl SubresourceRange {
    /// Resolve the concrete level count based on the total number of layers in an image.
    pub fn resolve_level_count(&self, total: Level) -> Level {
        self.level_count.unwrap_or(total - self.level_start)
    }

    /// Resolve the concrete layer count based on the total number of layer in an image.
    pub fn resolve_layer_count(&self, total: Layer) -> Layer {
        self.layer_count.unwrap_or(total - self.layer_start)
    }
}

/// Image format properties.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct FormatProperties {
    /// Maximum extent.
    pub max_extent: Extent,
    /// Max number of mipmap levels.
    pub max_levels: Level,
    /// Max number of array layers.
    pub max_layers: Layer,
    /// Bit mask of supported sample counts.
    pub sample_count_mask: NumSamples,
    /// Maximum size of the resource in bytes.
    pub max_resource_size: usize,
}

/// Footprint of a subresource in memory.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct SubresourceFootprint {
    /// Byte slice occupied by the subresource.
    pub slice: Range<RawOffset>,
    /// Byte distance between rows.
    pub row_pitch: RawOffset,
    /// Byte distance between array layers.
    pub array_pitch: RawOffset,
    /// Byte distance between depth slices.
    pub depth_pitch: RawOffset,
}

/// The type of tile to check for with `get_tile_size`.
#[derive(Debug)]
pub enum TileKind {
    /// A volume or 3D image tile kind.
    Volume,
    /// A flat or 2D image tile kind, with the number of samples for MSAA.
    Flat(NumSamples),
}

// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#sparsememory-standard-shapes
// https://docs.microsoft.com/en-us/windows/win32/direct3d11/texture2d-and-texture2darray-subresource-tiling
/// Tile or block size for sparse binding
pub fn get_tile_size(tile_kind: TileKind, texel_bits: u16) -> (u16, u16, u16) {
    match tile_kind {
        TileKind::Flat(samples) => {
            let sizes = match texel_bits {
                8 => (256, 256, 1),
                16 => (256, 128, 1),
                32 => (128, 128, 1),
                64 => (128, 64, 1),
                128 => (64, 64, 1),
                _ => unimplemented!(),
            };
            match samples {
                1 => sizes,
                2 => (sizes.0 / 2, sizes.1, 1),
                4 => (sizes.0 / 2, sizes.1 / 2, 1),
                8 => (sizes.0 / 4, sizes.1 / 2, 1),
                16 => (sizes.0 / 4, sizes.1 / 4, 1),
                _ => unimplemented!(),
            }
        }
        TileKind::Volume => match texel_bits {
            8 => (64, 32, 32),
            16 => (32, 32, 32),
            32 => (32, 32, 16),
            64 => (32, 16, 16),
            128 => (16, 16, 16),
            _ => unimplemented!(),
        },
    }
}

/// Description of a framebuffer attachment.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct FramebufferAttachment {
    /// Usage that an image is created with.
    pub usage: Usage,
    /// View capabilities that an image is created with.
    pub view_caps: ViewCapabilities,
    //TODO: make this a list
    /// The image view format.
    pub format: format::Format,
}
