//! Universal format specification.
//! Applicable to textures, views, and vertex buffers.
//!
//! For a more detailed description of all the specific format specifiers,
//! please see [the official Vulkan documentation](https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkFormat.html)
//!
//! `gfx-rs` splits a `Format` into two sub-components, a `SurfaceType` and
//! a `ChannelType`.  The `SurfaceType` specifies how the large the channels are,
//! for instance `R32_G32_B32_A32`.  The `ChannelType` specifies how the
//! components are interpreted, for instance `Sfloat` or `Sint`.

pub use external_memory::DrmModifier;

bitflags!(
    /// Bitflags which describe what properties of an image
    /// a format specifies or does not specify.  For example,
    /// the `Rgba8Unorm` format only specifies a `COLOR` aspect,
    /// while `D32SfloatS8Uint` specifies both a depth and stencil
    /// aspect but no color.
    #[derive(Default)]
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Aspects: u8 {
        /// Color aspect.
        const COLOR = 0x1;
        /// Depth aspect.
        const DEPTH = 0x2;
        /// Stencil aspect.
        const STENCIL = 0x4;
    }
);

/// Description of a format.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct FormatDesc {
    /// Total number of bits.
    ///
    /// * Depth/Stencil formats are opaque formats, where the total number of bits is unknown.
    ///   A dummy value is used for these formats instead (sum of depth and stencil bits).
    ///   For copy operations, the number of bits of the corresponding aspect should be used.
    /// * The total number can be larger than the sum of individual format bits
    ///   (`color`, `alpha`, `depth` and `stencil`) for packed formats.
    /// * For compressed formats, this denotes the number of bits per block.
    pub bits: u16,
    /// Dimensions (width, height) of the texel blocks.
    pub dim: (u8, u8),
    /// The format representation depends on the endianness of the platform.
    ///
    /// * On little-endian systems, the actual oreder of components is reverse of what
    ///   a surface type specifies.
    pub packed: bool,
    /// Format aspects
    pub aspects: Aspects,
}

impl FormatDesc {
    /// Check if the format is compressed.
    pub fn is_compressed(&self) -> bool {
        self.dim != (1, 1)
    }
}

/// Description of the bits distribution of a format.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct FormatBits {
    /// Number of color bits (summed for R/G/B).
    ///
    /// For compressed formats, this value is 0.
    pub color: u8,
    /// Number of alpha bits.
    ///
    /// For compressed formats, this value is 0.
    pub alpha: u8,
    /// Number of depth bits
    pub depth: u8,
    /// Number of stencil bits
    pub stencil: u8,
}

/// Format bits configuration with no bits assigned.
pub const BITS_ZERO: FormatBits = FormatBits {
    color: 0,
    alpha: 0,
    depth: 0,
    stencil: 0,
};

/// Source channel in a swizzle configuration. Some may redirect onto
/// different physical channels, some may be hardcoded to 0 or 1.
#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum Component {
    //TODO: add `Identity = 0`?
    /// Hardcoded zero
    Zero = 1,
    /// Hardcoded one
    One = 2,
    /// Red channel
    R = 3,
    /// Green channel
    G = 4,
    /// Blue channel
    B = 5,
    /// Alpha channel.
    A = 6,
}

/// Channel swizzle configuration for the resource views.
/// This specifies a "swizzle" operation which remaps the various
/// channels of a format into a different order.  For example,
/// `Swizzle(Component::B, Component::G, Component::R, Component::A)`
/// will swap `RGBA` formats into `BGRA` formats and back.
///
/// Note: It's not currently mirrored at compile-time,
/// thus providing less safety and convenience.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Swizzle(pub Component, pub Component, pub Component, pub Component);

impl Swizzle {
    /// A trivially non-swizzling configuration; performs no changes.
    pub const NO: Swizzle = Swizzle(Component::R, Component::G, Component::B, Component::A);
}

impl Default for Swizzle {
    fn default() -> Self {
        Self::NO
    }
}

/// Drm format properties
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct DrmFormatProperties {
    /// Drm format modifier.
    pub drm_modifier: DrmModifier,
    /// Number of memory planes each image crated with `drm_modifier` has.
    pub plane_count: u32,
    /// Valid image usage with the `drm_modifier`.
    pub valid_usages: ImageFeature,
}
impl Default for DrmFormatProperties {
    fn default() -> Self {
        Self {
            drm_modifier: DrmModifier::Invalid,
            plane_count: 0,
            valid_usages: ImageFeature::default(),
        }
    }
}

/// Format properties of the physical device.
#[derive(Debug, Default, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Properties {
    /// A bitmask of the features supported when an image with linear tiling is requested.
    /// Linear tiling has a known layout in-memory so data can be copied to and from host
    /// memory.
    pub linear_tiling: ImageFeature,
    /// A bitmask of the features supported when an image with optimal tiling is requested.
    /// Optimal tiling is arranged however the GPU wants; its exact layout is undefined.
    pub optimal_tiling: ImageFeature,
    /// The features supported by buffers.
    pub buffer_features: BufferFeature,
    /// Drm format properties
    pub drm_format_properties: Vec<DrmFormatProperties>,
}

//Note: these are detached from Vulkan!
bitflags!(
    /// Image feature flags.
    #[derive(Default)]
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct ImageFeature: u32 {
        /// Image view can be sampled.
        const SAMPLED = 0x1;
        /// Image can be sampled with a linear sampler or as blit source with linear sampling.
        const SAMPLED_LINEAR = 0x2;
        /// Image can be sampled with a min/max reduction sampler.
        const SAMPLED_MINMAX = 0x4;

        /// Image view can be used as storage image with exclusive read & write access.
        const STORAGE = 0x10;
        /// Image view can be used as storage image with simultaneous read/write access.
        const STORAGE_READ_WRITE = 0x20;
        /// Image view can be used as storage image with atomics.
        const STORAGE_ATOMIC = 0x40;

        /// Image view can be used as color and input attachment.
        const COLOR_ATTACHMENT = 0x100;
        /// Image view can be used as color (with blending) and input attachment.
        const COLOR_ATTACHMENT_BLEND = 0x200;
        /// Image view can be used as depth-stencil and input attachment.
        const DEPTH_STENCIL_ATTACHMENT = 0x400;

        /// Image can be used as source for blit commands.
        const BLIT_SRC = 0x1000;
        /// Image can be used as destination for blit commands.
        const BLIT_DST = 0x2000;
        /// Image can be copied from.
        const TRANSFER_SRC = 0x4000;
        /// Image can be copied to.
        const TRANSFER_DST = 0x8000;
    }
);

bitflags!(
    /// Buffer feature flags.
    #[derive(Default)]
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct BufferFeature: u32 {
        /// Buffer view can be used as uniform texel buffer.
        const UNIFORM_TEXEL = 0x8;
        /// Buffer view can be used as storage texel buffer.
        const STORAGE_TEXEL = 0x10;
        /// Buffer view can be used as storage texel buffer (with atomics).
        const STORAGE_TEXEL_ATOMIC = 0x20;
        /// Image view can be used as vertex buffer.
        const VERTEX = 0x40;
    }
);

/// Type of a surface channel. This is how we interpret the
/// storage allocated with `SurfaceType`.
#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum ChannelType {
    /// Unsigned normalized.
    Unorm,
    /// Signed normalized.
    Snorm,
    /// Unsigned integer.
    Uint,
    /// Signed integer.
    Sint,
    /// Unsigned floating-point.
    Ufloat,
    /// Signed floating-point.
    Sfloat,
    /// Unsigned scaled integer.
    Uscaled,
    /// Signed scaled integer.
    Sscaled,
    /// Unsigned normalized, SRGB non-linear encoded.
    Srgb,
}

macro_rules! surface_types {
    { $($name:ident { $total:expr, $($aspect:ident)|*, $dim:expr $( ,$component:ident : $bits:expr )*} ,)* } => {
        /// Type of the allocated texture surface. It is supposed to only
        /// carry information about the number of bits per each channel.
        /// The actual types are up to the views to decide and interpret.
        /// The actual components are up to the swizzle to define.
        #[repr(u8)]
        #[allow(missing_docs, non_camel_case_types)]
        #[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
        #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
        pub enum SurfaceType {
            $( $name, )*
        }

        impl SurfaceType {
            /// Return the bits for this format.
            pub fn describe_bits(&self) -> FormatBits {
                match *self {
                    $( SurfaceType::$name => FormatBits {
                        $( $component: $bits, )*
                        .. BITS_ZERO
                    }, )*
                }
            }

            /// Return the format descriptor.
            pub fn desc(&self) -> FormatDesc {
                match *self {
                    $( SurfaceType::$name => FormatDesc {
                        bits: $total.min(!$total),
                        dim: $dim,
                        packed: $total > 0x1000,
                        aspects: $(Aspects::$aspect)|*,
                    }, )*
                }
            }
        }
    }
}

// ident { num_bits, aspects, dim, (color, alpha, ..) }
// if the number of bits is given with exclamation (e.g. `!16`), the format is considered packed
surface_types! {
    R4_G4               {  !8, COLOR, (1, 1), color: 8 },
    R4_G4_B4_A4         { !16, COLOR, (1, 1), color: 12, alpha: 4 },
    B4_G4_R4_A4         { !16, COLOR, (1, 1), color: 12, alpha: 4 },
    R5_G6_B5            { !16, COLOR, (1, 1), color: 16 },
    B5_G6_R5            { !16, COLOR, (1, 1), color: 16 },
    R5_G5_B5_A1         { !16, COLOR, (1, 1), color: 15, alpha: 1 },
    B5_G5_R5_A1         { !16, COLOR, (1, 1), color: 15, alpha: 1 },
    A1_R5_G5_B5         { !16, COLOR, (1, 1), color: 15, alpha: 1 },
    R8                  {   8, COLOR, (1, 1), color: 8 },
    R8_G8               {  16, COLOR, (1, 1), color: 16 },
    R8_G8_B8            {  24, COLOR, (1, 1), color: 24 },
    B8_G8_R8            {  24, COLOR, (1, 1), color: 24 },
    R8_G8_B8_A8         {  32, COLOR, (1, 1), color: 24, alpha: 8 },
    B8_G8_R8_A8         {  32, COLOR, (1, 1), color: 24, alpha: 8 },
    A8_B8_G8_R8         { !32, COLOR, (1, 1), color: 24, alpha: 8 },
    A2_R10_G10_B10      { !32, COLOR, (1, 1), color: 30, alpha: 2 },
    A2_B10_G10_R10      { !32, COLOR, (1, 1), color: 30, alpha: 2 },
    R16                 {  16, COLOR, (1, 1), color: 16 },
    R16_G16             {  32, COLOR, (1, 1), color: 32 },
    R16_G16_B16         {  48, COLOR, (1, 1), color: 48 },
    R16_G16_B16_A16     {  64, COLOR, (1, 1), color: 48, alpha: 16 },
    R32                 {  32, COLOR, (1, 1), color: 32 },
    R32_G32             {  64, COLOR, (1, 1), color: 64 },
    R32_G32_B32         {  96, COLOR, (1, 1), color: 96 },
    R32_G32_B32_A32     { 128, COLOR, (1, 1), color: 96, alpha: 32 },
    R64                 {  64, COLOR, (1, 1), color: 64 },
    R64_G64             { 128, COLOR, (1, 1), color: 128 },
    R64_G64_B64         { 192, COLOR, (1, 1), color: 192 },
    R64_G64_B64_A64     { 256, COLOR, (1, 1), color: 192, alpha: 64 },
    B10_G11_R11         { !32, COLOR, (1, 1), color: 32 },
    E5_B9_G9_R9         { !32, COLOR, (1, 1), color: 27 },
    D16                 {  16, DEPTH, (1, 1), depth: 16 },
    X8D24               { !32, DEPTH, (1, 1), depth: 24 },
    D32                 {  32, DEPTH, (1, 1), depth: 32 },
    S8                  {   8, STENCIL, (1, 1), stencil: 8 },
    D16_S8              {  24, DEPTH | STENCIL, (1, 1), depth: 16, stencil: 8 },
    D24_S8              {  32, DEPTH | STENCIL, (1, 1), depth: 24, stencil: 8 },
    D32_S8              {  40, DEPTH | STENCIL, (1, 1), depth: 32, stencil: 8 },
    BC1_RGB             {  64, COLOR, (4, 4) },
    BC1_RGBA            {  64, COLOR, (4, 4) },
    BC2                 { 128, COLOR, (4, 4) },
    BC3                 { 128, COLOR, (4, 4) },
    BC4                 {  64, COLOR, (4, 4) },
    BC5                 { 128, COLOR, (4, 4) },
    BC6                 { 128, COLOR, (4, 4) },
    BC7                 { 128, COLOR, (4, 4) },
    ETC2_R8_G8_B8       {  64, COLOR, (4, 4) },
    ETC2_R8_G8_B8_A1    {  64, COLOR, (4, 4) },
    ETC2_R8_G8_B8_A8    { 128, COLOR, (4, 4) },
    EAC_R11             {  64, COLOR, (4, 4) },
    EAC_R11_G11         { 128, COLOR, (4, 4) },
    ASTC_4x4            { 128, COLOR, (4, 4) },
    ASTC_5x4            { 128, COLOR, (5, 4) },
    ASTC_5x5            { 128, COLOR, (5, 5) },
    ASTC_6x5            { 128, COLOR, (6, 5) },
    ASTC_6x6            { 128, COLOR, (6, 6) },
    ASTC_8x5            { 128, COLOR, (8, 5) },
    ASTC_8x6            { 128, COLOR, (8, 6) },
    ASTC_8x8            { 128, COLOR, (8, 8) },
    ASTC_10x5           { 128, COLOR, (10, 5) },
    ASTC_10x6           { 128, COLOR, (10, 6) },
    ASTC_10x8           { 128, COLOR, (10, 8) },
    ASTC_10x10          { 128, COLOR, (10, 10) },
    ASTC_12x10          { 128, COLOR, (12, 10) },
    ASTC_12x12          { 128, COLOR, (12, 12) },
}

/// Generic run-time base format.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct BaseFormat(pub SurfaceType, pub ChannelType);

/// Conversion trait into `Format`;
pub trait AsFormat {
    /// Associated format.
    const SELF: Format;
}

macro_rules! formats {
    {
        $name:ident = ($surface:ident, $channel:ident),
        $($name_tail:ident = ($surface_tail:ident, $channel_tail:ident),)*
    } => {
        /// A format descriptor that describes the channels present in a
        /// texture or view, how they are laid out, what size they are,
        /// and how the elements of the channels are interpreted (integer,
        /// float, etc.)
        #[allow(missing_docs)]
        #[repr(u32)]
        #[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
        #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
        pub enum Format {
            $name = 1,
            $( $name_tail, )*

            // This serves as safety net for conversion from Vulkan -> HAL,
            // in case Vulkan adds new formats:
            //  1. We can check if a format is out of range
            //  2. We 'ensure' that backend implementations do non-exhaustive matching
            #[doc(hidden)]
            __NumFormats,
        }

        /// Number of formats.
        pub const NUM_FORMATS: usize = Format::__NumFormats as _;

        /// Conversion table from `Format` to `BaseFormat`, excluding `Undefined`.
        pub const BASE_FORMATS: [BaseFormat; NUM_FORMATS-1] = [
              BaseFormat(SurfaceType::$surface, ChannelType::$channel),
            $(BaseFormat(SurfaceType::$surface_tail, ChannelType::$channel_tail), )*
        ];

            /// A struct equivalent to the matching `Format` enum member, which allows
            /// an API to be strongly typed on particular formats.
            #[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
            #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
            pub struct $name;

            impl AsFormat for $name {
                const SELF: Format = Format::$name;
            }

        $(
            /// A struct equivalent to the matching `Format` enum member, which allows
            /// an API to be strongly typed on particular formats.
            #[allow(missing_docs)]
            #[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
            #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
            pub struct $name_tail;

            impl AsFormat for $name_tail {
                const SELF: Format = Format::$name_tail;
            }

        )*
    }
}

// Format order has to match the order exposed by the Vulkan API.
formats! {
    Rg4Unorm = (R4_G4, Unorm),
    Rgba4Unorm = (R4_G4_B4_A4, Unorm),
    Bgra4Unorm = (B4_G4_R4_A4, Unorm),
    R5g6b5Unorm = (R5_G6_B5, Unorm),
    B5g6r5Unorm = (B5_G6_R5, Unorm),
    R5g5b5a1Unorm = (R5_G5_B5_A1, Unorm),
    B5g5r5a1Unorm = (B5_G5_R5_A1, Unorm),
    A1r5g5b5Unorm = (A1_R5_G5_B5, Unorm),
    R8Unorm = (R8, Unorm),
    R8Snorm = (R8, Snorm),
    R8Uscaled = (R8, Uscaled),
    R8Sscaled = (R8, Sscaled),
    R8Uint = (R8, Uint),
    R8Sint = (R8, Sint),
    R8Srgb = (R8, Srgb),
    Rg8Unorm = (R8_G8, Unorm),
    Rg8Snorm = (R8_G8, Snorm),
    Rg8Uscaled = (R8_G8, Uscaled),
    Rg8Sscaled = (R8_G8, Sscaled),
    Rg8Uint = (R8_G8, Uint),
    Rg8Sint = (R8_G8, Sint),
    Rg8Srgb = (R8_G8, Srgb),
    Rgb8Unorm = (R8_G8_B8, Unorm),
    Rgb8Snorm = (R8_G8_B8, Snorm),
    Rgb8Uscaled = (R8_G8_B8, Uscaled),
    Rgb8Sscaled = (R8_G8_B8, Sscaled),
    Rgb8Uint = (R8_G8_B8, Uint),
    Rgb8Sint = (R8_G8_B8, Sint),
    Rgb8Srgb = (R8_G8_B8, Srgb),
    Bgr8Unorm = (B8_G8_R8, Unorm),
    Bgr8Snorm = (B8_G8_R8, Snorm),
    Bgr8Uscaled = (B8_G8_R8, Uscaled),
    Bgr8Sscaled = (B8_G8_R8, Sscaled),
    Bgr8Uint = (B8_G8_R8, Uint),
    Bgr8Sint = (B8_G8_R8, Sint),
    Bgr8Srgb = (B8_G8_R8, Srgb),
    Rgba8Unorm = (R8_G8_B8_A8, Unorm),
    Rgba8Snorm = (R8_G8_B8_A8, Snorm),
    Rgba8Uscaled = (R8_G8_B8_A8, Uscaled),
    Rgba8Sscaled = (R8_G8_B8_A8, Sscaled),
    Rgba8Uint = (R8_G8_B8_A8, Uint),
    Rgba8Sint = (R8_G8_B8_A8, Sint),
    Rgba8Srgb = (R8_G8_B8_A8, Srgb),
    Bgra8Unorm = (B8_G8_R8_A8, Unorm),
    Bgra8Snorm = (B8_G8_R8_A8, Snorm),
    Bgra8Uscaled = (B8_G8_R8_A8, Uscaled),
    Bgra8Sscaled = (B8_G8_R8_A8, Sscaled),
    Bgra8Uint = (B8_G8_R8_A8, Uint),
    Bgra8Sint = (B8_G8_R8_A8, Sint),
    Bgra8Srgb = (B8_G8_R8_A8, Srgb),
    Abgr8Unorm = (A8_B8_G8_R8, Unorm),
    Abgr8Snorm = (A8_B8_G8_R8, Snorm),
    Abgr8Uscaled = (A8_B8_G8_R8, Uscaled),
    Abgr8Sscaled = (A8_B8_G8_R8, Sscaled),
    Abgr8Uint = (A8_B8_G8_R8, Uint),
    Abgr8Sint = (A8_B8_G8_R8, Sint),
    Abgr8Srgb = (A8_B8_G8_R8, Srgb),
    A2r10g10b10Unorm = (A2_R10_G10_B10, Unorm),
    A2r10g10b10Snorm = (A2_R10_G10_B10, Snorm),
    A2r10g10b10Uscaled = (A2_R10_G10_B10, Uscaled),
    A2r10g10b10Sscaled = (A2_R10_G10_B10, Sscaled),
    A2r10g10b10Uint = (A2_R10_G10_B10, Uint),
    A2r10g10b10Sint = (A2_R10_G10_B10, Sint),
    A2b10g10r10Unorm = (A2_B10_G10_R10, Unorm),
    A2b10g10r10Snorm = (A2_B10_G10_R10, Snorm),
    A2b10g10r10Uscaled = (A2_B10_G10_R10, Uscaled),
    A2b10g10r10Sscaled = (A2_B10_G10_R10, Sscaled),
    A2b10g10r10Uint = (A2_B10_G10_R10, Uint),
    A2b10g10r10Sint = (A2_B10_G10_R10, Sint),
    R16Unorm = (R16, Unorm),
    R16Snorm = (R16, Snorm),
    R16Uscaled = (R16, Uscaled),
    R16Sscaled = (R16, Sscaled),
    R16Uint = (R16, Uint),
    R16Sint = (R16, Sint),
    R16Sfloat = (R16, Sfloat),
    Rg16Unorm = (R16_G16, Unorm),
    Rg16Snorm = (R16_G16, Snorm),
    Rg16Uscaled = (R16_G16, Uscaled),
    Rg16Sscaled = (R16_G16, Sscaled),
    Rg16Uint = (R16_G16, Uint),
    Rg16Sint = (R16_G16, Sint),
    Rg16Sfloat = (R16_G16, Sfloat),
    Rgb16Unorm = (R16_G16_B16, Unorm),
    Rgb16Snorm = (R16_G16_B16, Snorm),
    Rgb16Uscaled = (R16_G16_B16, Uscaled),
    Rgb16Sscaled = (R16_G16_B16, Sscaled),
    Rgb16Uint = (R16_G16_B16, Uint),
    Rgb16Sint = (R16_G16_B16, Sint),
    Rgb16Sfloat = (R16_G16_B16, Sfloat),
    Rgba16Unorm = (R16_G16_B16_A16, Unorm),
    Rgba16Snorm = (R16_G16_B16_A16, Snorm),
    Rgba16Uscaled = (R16_G16_B16_A16, Uscaled),
    Rgba16Sscaled = (R16_G16_B16_A16, Sscaled),
    Rgba16Uint = (R16_G16_B16_A16, Uint),
    Rgba16Sint = (R16_G16_B16_A16, Sint),
    Rgba16Sfloat = (R16_G16_B16_A16, Sfloat),
    R32Uint = (R32, Uint),
    R32Sint = (R32, Sint),
    R32Sfloat = (R32, Sfloat),
    Rg32Uint = (R32_G32, Uint),
    Rg32Sint = (R32_G32, Sint),
    Rg32Sfloat = (R32_G32, Sfloat),
    Rgb32Uint = (R32_G32_B32, Uint),
    Rgb32Sint = (R32_G32_B32, Sint),
    Rgb32Sfloat = (R32_G32_B32, Sfloat),
    Rgba32Uint = (R32_G32_B32_A32, Uint),
    Rgba32Sint = (R32_G32_B32_A32, Sint),
    Rgba32Sfloat = (R32_G32_B32_A32, Sfloat),
    R64Uint = (R64, Uint),
    R64Sint = (R64, Sint),
    R64Sfloat = (R64, Sfloat),
    Rg64Uint = (R64_G64, Uint),
    Rg64Sint = (R64_G64, Sint),
    Rg64Sfloat = (R64_G64, Sfloat),
    Rgb64Uint = (R64_G64_B64, Uint),
    Rgb64Sint = (R64_G64_B64, Sint),
    Rgb64Sfloat = (R64_G64_B64, Sfloat),
    Rgba64Uint = (R64_G64_B64_A64, Uint),
    Rgba64Sint = (R64_G64_B64_A64, Sint),
    Rgba64Sfloat = (R64_G64_B64_A64, Sfloat),
    B10g11r11Ufloat = (B10_G11_R11, Ufloat),
    E5b9g9r9Ufloat = (E5_B9_G9_R9, Ufloat),
    D16Unorm = (D16, Unorm),
    X8D24Unorm = (X8D24, Unorm),
    D32Sfloat = (D32, Sfloat),
    S8Uint = (S8, Uint),
    D16UnormS8Uint = (D16_S8, Unorm),
    D24UnormS8Uint = (D24_S8, Unorm),
    D32SfloatS8Uint = (D32_S8, Sfloat),
    Bc1RgbUnorm = (BC1_RGB, Unorm),
    Bc1RgbSrgb = (BC1_RGB, Srgb),
    Bc1RgbaUnorm = (BC1_RGBA, Unorm),
    Bc1RgbaSrgb = (BC1_RGBA, Srgb),
    Bc2Unorm = (BC2, Unorm),
    Bc2Srgb = (BC2, Srgb),
    Bc3Unorm = (BC3, Unorm),
    Bc3Srgb = (BC3, Srgb),
    Bc4Unorm = (BC4, Unorm),
    Bc4Snorm = (BC4, Snorm),
    Bc5Unorm = (BC5, Unorm),
    Bc5Snorm = (BC5, Snorm),
    Bc6hUfloat = (BC6, Ufloat),
    Bc6hSfloat = (BC6, Sfloat),
    Bc7Unorm = (BC7, Unorm),
    Bc7Srgb = (BC7, Srgb),
    Etc2R8g8b8Unorm = (ETC2_R8_G8_B8, Unorm),
    Etc2R8g8b8Srgb = (ETC2_R8_G8_B8, Srgb),
    Etc2R8g8b8a1Unorm = (ETC2_R8_G8_B8_A1, Unorm),
    Etc2R8g8b8a1Srgb = (ETC2_R8_G8_B8_A1, Srgb),
    Etc2R8g8b8a8Unorm = (ETC2_R8_G8_B8_A8, Unorm),
    Etc2R8g8b8a8Srgb = (ETC2_R8_G8_B8_A8, Srgb),
    EacR11Unorm = (EAC_R11, Unorm),
    EacR11Snorm = (EAC_R11, Snorm),
    EacR11g11Unorm = (EAC_R11_G11, Unorm),
    EacR11g11Snorm = (EAC_R11_G11, Snorm),
    Astc4x4Unorm = (ASTC_4x4, Unorm),
    Astc4x4Srgb = (ASTC_4x4, Srgb),
    Astc5x4Unorm = (ASTC_5x4, Unorm),
    Astc5x4Srgb = (ASTC_5x4, Srgb),
    Astc5x5Unorm = (ASTC_5x5, Unorm),
    Astc5x5Srgb = (ASTC_5x5, Srgb),
    Astc6x5Unorm = (ASTC_6x5, Unorm),
    Astc6x5Srgb = (ASTC_6x5, Srgb),
    Astc6x6Unorm = (ASTC_6x6, Unorm),
    Astc6x6Srgb = (ASTC_6x6, Srgb),
    Astc8x5Unorm = (ASTC_8x5, Unorm),
    Astc8x5Srgb = (ASTC_8x5, Srgb),
    Astc8x6Unorm = (ASTC_8x6, Unorm),
    Astc8x6Srgb = (ASTC_8x6, Srgb),
    Astc8x8Unorm = (ASTC_8x8, Unorm),
    Astc8x8Srgb = (ASTC_8x8, Srgb),
    Astc10x5Unorm = (ASTC_10x5, Unorm),
    Astc10x5Srgb = (ASTC_10x5, Srgb),
    Astc10x6Unorm = (ASTC_10x6, Unorm),
    Astc10x6Srgb = (ASTC_10x6, Srgb),
    Astc10x8Unorm = (ASTC_10x8, Unorm),
    Astc10x8Srgb = (ASTC_10x8, Srgb),
    Astc10x10Unorm = (ASTC_10x10, Unorm),
    Astc10x10Srgb = (ASTC_10x10, Srgb),
    Astc12x10Unorm = (ASTC_12x10, Unorm),
    Astc12x10Srgb = (ASTC_12x10, Srgb),
    Astc12x12Unorm = (ASTC_12x12, Unorm),
    Astc12x12Srgb = (ASTC_12x12, Srgb),
}

impl Format {
    /// Get base format.
    ///
    /// Returns `None` if format is `Undefined`.
    pub fn base_format(self) -> BaseFormat {
        assert!(self as usize != 0 && NUM_FORMATS > self as usize);
        BASE_FORMATS[self as usize - 1]
    }

    /// A shortcut to obtain surface format description.
    pub fn surface_desc(&self) -> FormatDesc {
        self.base_format().0.desc()
    }

    /// Returns if the format has a color aspect.
    pub fn is_color(self) -> bool {
        self.surface_desc().aspects.contains(Aspects::COLOR)
    }

    /// Returns if the format has a depth aspect.
    pub fn is_depth(self) -> bool {
        self.surface_desc().aspects.contains(Aspects::DEPTH)
    }

    /// Returns if the format has a stencil aspect.
    pub fn is_stencil(self) -> bool {
        self.surface_desc().aspects.contains(Aspects::STENCIL)
    }
}

// Common vertex attribute formats
impl AsFormat for f32 {
    const SELF: Format = Format::R32Sfloat;
}
impl AsFormat for [f32; 2] {
    const SELF: Format = Format::Rg32Sfloat;
}
impl AsFormat for [f32; 3] {
    const SELF: Format = Format::Rgb32Sfloat;
}
impl AsFormat for [f32; 4] {
    const SELF: Format = Format::Rgba32Sfloat;
}
