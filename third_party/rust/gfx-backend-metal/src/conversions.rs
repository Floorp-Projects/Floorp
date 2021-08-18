use hal;

use crate::PrivateCapabilities;

use hal::{
    format::{Format, Properties, Swizzle},
    image, pass, pso,
    pso::{Comparison, StencilOp},
    IndexType,
};
use metal::*;
use std::num::NonZeroU32;

impl PrivateCapabilities {
    pub fn map_format(&self, format: Format) -> Option<MTLPixelFormat> {
        use self::hal::format::Format as f;
        use metal::MTLPixelFormat::*;
        Some(match format {
            f::R5g6b5Unorm if self.format_b5 => B5G6R5Unorm,
            f::R5g5b5a1Unorm if self.format_b5 => A1BGR5Unorm,
            f::A1r5g5b5Unorm if self.format_b5 => BGR5A1Unorm,
            f::Rgba4Unorm if self.format_b5 => ABGR4Unorm,
            f::R8Srgb if self.format_min_srgb_channels <= 1 => R8Unorm_sRGB,
            f::Rg8Srgb if self.format_min_srgb_channels <= 2 => RG8Unorm_sRGB,
            f::Rgba8Srgb if self.format_min_srgb_channels <= 4 => RGBA8Unorm_sRGB,
            f::Bgra8Srgb if self.format_min_srgb_channels <= 4 => BGRA8Unorm_sRGB,
            f::D16Unorm if self.format_depth16unorm => Depth16Unorm,
            f::D24UnormS8Uint if self.format_depth24_stencil8 => Depth24Unorm_Stencil8,
            f::D32Sfloat => Depth32Float,
            f::D32SfloatS8Uint => Depth32Float_Stencil8,
            f::R8Unorm => R8Unorm,
            f::R8Snorm => R8Snorm,
            f::R8Uint => R8Uint,
            f::R8Sint => R8Sint,
            f::Rg8Unorm => RG8Unorm,
            f::Rg8Snorm => RG8Snorm,
            f::Rg8Uint => RG8Uint,
            f::Rg8Sint => RG8Sint,
            f::Rgba8Unorm => RGBA8Unorm,
            f::Rgba8Snorm => RGBA8Snorm,
            f::Rgba8Uint => RGBA8Uint,
            f::Rgba8Sint => RGBA8Sint,
            f::Bgra8Unorm => BGRA8Unorm,
            f::R16Unorm => R16Unorm,
            f::R16Snorm => R16Snorm,
            f::R16Uint => R16Uint,
            f::R16Sint => R16Sint,
            f::R16Sfloat => R16Float,
            f::Rg16Unorm => RG16Unorm,
            f::Rg16Snorm => RG16Snorm,
            f::Rg16Uint => RG16Uint,
            f::Rg16Sint => RG16Sint,
            f::Rg16Sfloat => RG16Float,
            f::Rgba16Unorm => RGBA16Unorm,
            f::Rgba16Snorm => RGBA16Snorm,
            f::Rgba16Uint => RGBA16Uint,
            f::Rgba16Sint => RGBA16Sint,
            f::Rgba16Sfloat => RGBA16Float,
            f::A2r10g10b10Unorm => BGR10A2Unorm,
            f::A2b10g10r10Unorm => RGB10A2Unorm,
            f::B10g11r11Ufloat => RG11B10Float,
            f::E5b9g9r9Ufloat => RGB9E5Float,
            f::R32Uint => R32Uint,
            f::R32Sint => R32Sint,
            f::R32Sfloat => R32Float,
            f::Rg32Uint => RG32Uint,
            f::Rg32Sint => RG32Sint,
            f::Rg32Sfloat => RG32Float,
            f::Rgba32Uint => RGBA32Uint,
            f::Rgba32Sint => RGBA32Sint,
            f::Rgba32Sfloat => RGBA32Float,
            f::Bc1RgbaUnorm if self.format_bc => BC1_RGBA,
            f::Bc1RgbaSrgb if self.format_bc => BC1_RGBA_sRGB,
            f::Bc1RgbUnorm if self.format_bc => BC1_RGBA, //TODO?
            f::Bc1RgbSrgb if self.format_bc => BC1_RGBA_sRGB, //TODO?
            f::Bc2Unorm if self.format_bc => BC2_RGBA,
            f::Bc2Srgb if self.format_bc => BC2_RGBA_sRGB,
            f::Bc3Unorm if self.format_bc => BC3_RGBA,
            f::Bc3Srgb if self.format_bc => BC3_RGBA_sRGB,
            f::Bc4Unorm if self.format_bc => BC4_RUnorm,
            f::Bc4Snorm if self.format_bc => BC4_RSnorm,
            f::Bc5Unorm if self.format_bc => BC5_RGUnorm,
            f::Bc5Snorm if self.format_bc => BC5_RGSnorm,
            f::Bc6hUfloat if self.format_bc => BC6H_RGBUfloat,
            f::Bc6hSfloat if self.format_bc => BC6H_RGBFloat,
            f::Bc7Unorm if self.format_bc => BC7_RGBAUnorm,
            f::Bc7Srgb if self.format_bc => BC7_RGBAUnorm_sRGB,
            f::EacR11Unorm if self.format_eac_etc => EAC_R11Unorm,
            f::EacR11Snorm if self.format_eac_etc => EAC_R11Snorm,
            f::EacR11g11Unorm if self.format_eac_etc => EAC_RG11Unorm,
            f::EacR11g11Snorm if self.format_eac_etc => EAC_RG11Snorm,
            f::Etc2R8g8b8Unorm if self.format_eac_etc => ETC2_RGB8,
            f::Etc2R8g8b8Srgb if self.format_eac_etc => ETC2_RGB8_sRGB,
            f::Etc2R8g8b8a1Unorm if self.format_eac_etc => ETC2_RGB8A1,
            f::Etc2R8g8b8a1Srgb if self.format_eac_etc => ETC2_RGB8A1_sRGB,
            f::Astc4x4Unorm if self.format_astc => ASTC_4x4_LDR,
            f::Astc4x4Srgb if self.format_astc => ASTC_4x4_sRGB,
            f::Astc5x4Unorm if self.format_astc => ASTC_5x4_LDR,
            f::Astc5x4Srgb if self.format_astc => ASTC_5x4_sRGB,
            f::Astc5x5Unorm if self.format_astc => ASTC_5x5_LDR,
            f::Astc5x5Srgb if self.format_astc => ASTC_5x5_sRGB,
            f::Astc6x5Unorm if self.format_astc => ASTC_6x5_LDR,
            f::Astc6x5Srgb if self.format_astc => ASTC_6x5_sRGB,
            f::Astc6x6Unorm if self.format_astc => ASTC_6x6_LDR,
            f::Astc6x6Srgb if self.format_astc => ASTC_6x6_sRGB,
            f::Astc8x5Unorm if self.format_astc => ASTC_8x5_LDR,
            f::Astc8x5Srgb if self.format_astc => ASTC_8x5_sRGB,
            f::Astc8x6Unorm if self.format_astc => ASTC_8x6_LDR,
            f::Astc8x6Srgb if self.format_astc => ASTC_8x6_sRGB,
            f::Astc8x8Unorm if self.format_astc => ASTC_8x8_LDR,
            f::Astc8x8Srgb if self.format_astc => ASTC_8x8_sRGB,
            f::Astc10x5Unorm if self.format_astc => ASTC_10x5_LDR,
            f::Astc10x5Srgb if self.format_astc => ASTC_10x5_sRGB,
            f::Astc10x6Unorm if self.format_astc => ASTC_10x6_LDR,
            f::Astc10x6Srgb if self.format_astc => ASTC_10x6_sRGB,
            f::Astc10x8Unorm if self.format_astc => ASTC_10x8_LDR,
            f::Astc10x8Srgb if self.format_astc => ASTC_10x8_sRGB,
            f::Astc10x10Unorm if self.format_astc => ASTC_10x10_LDR,
            f::Astc10x10Srgb if self.format_astc => ASTC_10x10_sRGB,
            f::Astc12x10Unorm if self.format_astc => ASTC_12x10_LDR,
            f::Astc12x10Srgb if self.format_astc => ASTC_12x10_sRGB,
            f::Astc12x12Unorm if self.format_astc => ASTC_12x12_LDR,
            f::Astc12x12Srgb if self.format_astc => ASTC_12x12_sRGB,
            // Not supported:
            // a8Unorm
            // agbr4Unorm
            // pvrtc_rgb_2bpp
            // pvrtc_rgb_2bpp_srgb
            // pvrtc_rgb_4bpp
            // pvrtc_rgb_4bpp_srgb
            // pvrtc_rgba_2bpp
            // pvrtc_rgba_2bpp_srgb
            // pvrtc_rgba_4bpp
            // pvrtc_rgba_4bpp_srgb
            // eac_rgba8
            // eac_rgba8_srgb
            // gbgr422
            // bgrg422
            // stencil8 (float-version)
            // x32_stencil8 (float-version)
            // x24_stencil8 (float-version)
            // bgra10_xr
            // bgra10_xr_srgb
            // bgr10_xr
            // bgr10_xr_srgb
            _ => return None,
        })
    }

    pub fn map_format_with_swizzle(
        &self,
        format: Format,
        swizzle: Swizzle,
    ) -> Option<MTLPixelFormat> {
        use self::hal::format::{Component::*, Format::*};
        use metal::MTLPixelFormat as Pf;
        match (format, swizzle) {
            (R8Unorm, Swizzle(Zero, Zero, Zero, R)) => Some(Pf::A8Unorm),
            (Rgba8Unorm, Swizzle(B, G, R, A)) => Some(Pf::BGRA8Unorm),
            (Bgra8Unorm, Swizzle(B, G, R, A)) => Some(Pf::RGBA8Unorm),
            (Bgra8Srgb, Swizzle(B, G, R, A)) => Some(Pf::RGBA8Unorm_sRGB),
            (B5g6r5Unorm, Swizzle(B, G, R, A)) if self.format_b5 => Some(Pf::B5G6R5Unorm),
            _ => {
                let bits = format.base_format().0.describe_bits();
                if swizzle != Swizzle::NO && !(bits.alpha == 0 && swizzle == Swizzle(R, G, B, One))
                {
                    error!("Unsupported swizzle {:?} for format {:?}", swizzle, format);
                }
                self.map_format(format)
            }
        }
    }

    pub fn map_format_properties(&self, format: Format) -> Properties {
        use self::hal::format::{BufferFeature as Bf, ImageFeature as If};
        use metal::MTLPixelFormat::*;

        // Affected formats documented at:
        // https://developer.apple.com/documentation/metal/mtlreadwritetexturetier/mtlreadwritetexturetier1?language=objc
        // https://developer.apple.com/documentation/metal/mtlreadwritetexturetier/mtlreadwritetexturetier2?language=objc
        let (read_write_tier1_if, read_write_tier2_if) = match self.read_write_texture_tier {
            MTLReadWriteTextureTier::TierNone => (If::empty(), If::empty()),
            MTLReadWriteTextureTier::Tier1 => (If::STORAGE_READ_WRITE, If::empty()),
            MTLReadWriteTextureTier::Tier2 => (If::STORAGE_READ_WRITE, If::STORAGE_READ_WRITE),
        };

        let mtl_format = match self.map_format(format) {
            Some(mtl_format) => mtl_format,
            None => {
                return Properties {
                    buffer_features: if map_vertex_format(format).is_some() {
                        Bf::VERTEX
                    } else {
                        Bf::empty()
                    },
                    optimal_tiling: If::empty(),
                    linear_tiling: If::empty(),
                }
            }
        };
        let extra_optimal = match mtl_format {
            A8Unorm => If::SAMPLED_LINEAR,
            R8Unorm => {
                read_write_tier2_if
                    | If::SAMPLED_LINEAR
                    | If::STORAGE
                    | If::COLOR_ATTACHMENT
                    | If::COLOR_ATTACHMENT_BLEND
            }
            R8Unorm_sRGB if self.format_any8_unorm_srgb_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            R8Unorm_sRGB if self.format_any8_unorm_srgb_no_write => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            R8Snorm if self.format_any8_snorm_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            R8Uint => read_write_tier2_if | If::STORAGE | If::COLOR_ATTACHMENT,
            R8Sint => read_write_tier2_if | If::STORAGE | If::COLOR_ATTACHMENT,
            R16Unorm if self.format_r16_norm_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            R16Snorm if self.format_r16_norm_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            R16Uint => read_write_tier2_if | If::STORAGE | If::COLOR_ATTACHMENT,
            R16Sint => read_write_tier2_if | If::STORAGE | If::COLOR_ATTACHMENT,
            R16Float => {
                read_write_tier2_if
                    | If::SAMPLED_LINEAR
                    | If::STORAGE
                    | If::COLOR_ATTACHMENT
                    | If::COLOR_ATTACHMENT_BLEND
            }
            RG8Unorm => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RG8Unorm_sRGB if self.format_any8_unorm_srgb_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RG8Unorm_sRGB if self.format_any8_unorm_srgb_no_write => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RG8Snorm if self.format_any8_snorm_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RG8Uint => If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT,
            RG8Sint => If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT,
            B5G6R5Unorm if self.format_b5 => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            A1BGR5Unorm if self.format_b5 => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            ABGR4Unorm if self.format_b5 => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            BGR5A1Unorm if self.format_b5 => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            R32Uint if self.format_r32_all => {
                read_write_tier1_if | If::STORAGE | If::COLOR_ATTACHMENT
            }
            R32Uint if self.format_r32_no_write => If::COLOR_ATTACHMENT,
            R32Sint if self.format_r32_all => {
                read_write_tier1_if | If::STORAGE | If::COLOR_ATTACHMENT
            }
            R32Sint if self.format_r32_no_write => If::COLOR_ATTACHMENT,
            R32Float if self.format_r32float_no_write_no_filter => {
                If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            R32Float if self.format_r32float_no_filter => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            R32Float if self.format_r32float_all => {
                read_write_tier1_if
                    | If::SAMPLED_LINEAR
                    | If::STORAGE
                    | If::COLOR_ATTACHMENT
                    | If::COLOR_ATTACHMENT_BLEND
            }
            RG16Unorm => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RG16Snorm => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RG16Float => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGBA8Unorm => {
                read_write_tier2_if
                    | If::SAMPLED_LINEAR
                    | If::STORAGE
                    | If::COLOR_ATTACHMENT
                    | If::COLOR_ATTACHMENT_BLEND
            }
            RGBA8Unorm_sRGB if self.format_rgba8_srgb_no_write => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGBA8Unorm_sRGB if self.format_rgba8_srgb_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGBA8Snorm => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGBA8Uint => read_write_tier2_if | If::STORAGE | If::COLOR_ATTACHMENT,
            RGBA8Sint => read_write_tier2_if | If::STORAGE | If::COLOR_ATTACHMENT,
            BGRA8Unorm => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            BGRA8Unorm_sRGB if self.format_rgba8_srgb_no_write => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            BGRA8Unorm_sRGB if self.format_rgba8_srgb_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGB10A2Unorm if self.format_rgb10a2_unorm_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGB10A2Unorm if self.format_rgb10a2_unorm_no_write => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGB10A2Uint if self.format_rgb10a2_uint_color => If::COLOR_ATTACHMENT,
            RGB10A2Uint if self.format_rgb10a2_uint_color_write => {
                If::STORAGE | If::COLOR_ATTACHMENT
            }
            RG11B10Float if self.format_rg11b10_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RG11B10Float if self.format_rg11b10_no_write => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGB9E5Float if self.format_rgb9e5_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGB9E5Float if self.format_rgb9e5_filter_only => If::SAMPLED_LINEAR,
            RGB9E5Float if self.format_rgb9e5_no_write => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RG32Uint if self.format_rg32_color => If::COLOR_ATTACHMENT,
            RG32Sint if self.format_rg32_color => If::COLOR_ATTACHMENT,
            RG32Uint if self.format_rg32_color_write => If::COLOR_ATTACHMENT | If::STORAGE,
            RG32Sint if self.format_rg32_color_write => If::COLOR_ATTACHMENT | If::STORAGE,
            RG32Float if self.format_rg32float_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RG32Float if self.format_rg32float_color_blend => {
                If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RG32Float if self.format_rg32float_no_filter => {
                If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGBA16Unorm => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGBA16Snorm => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            RGBA16Uint => read_write_tier2_if | If::STORAGE | If::COLOR_ATTACHMENT,
            RGBA16Sint => read_write_tier2_if | If::STORAGE | If::COLOR_ATTACHMENT,
            RGBA16Float => {
                read_write_tier2_if
                    | If::SAMPLED_LINEAR
                    | If::STORAGE
                    | If::COLOR_ATTACHMENT
                    | If::COLOR_ATTACHMENT_BLEND
            }
            RGBA32Uint if self.format_rgba32int_color => If::COLOR_ATTACHMENT,
            RGBA32Uint if self.format_rgba32int_color_write => {
                read_write_tier2_if | If::COLOR_ATTACHMENT | If::STORAGE
            }
            RGBA32Sint if self.format_rgba32int_color => If::COLOR_ATTACHMENT,
            RGBA32Sint if self.format_rgba32int_color_write => {
                read_write_tier2_if | If::COLOR_ATTACHMENT | If::STORAGE
            }
            RGBA32Float if self.format_rgba32float_all => {
                read_write_tier2_if
                    | If::SAMPLED_LINEAR
                    | If::STORAGE
                    | If::COLOR_ATTACHMENT
                    | If::COLOR_ATTACHMENT_BLEND
            }
            RGBA32Float if self.format_rgba32float_color => If::COLOR_ATTACHMENT,
            RGBA32Float if self.format_rgba32float_color_write => {
                read_write_tier2_if | If::COLOR_ATTACHMENT | If::STORAGE
            }
            EAC_R11Unorm if self.format_eac_etc => If::SAMPLED_LINEAR,
            EAC_R11Snorm if self.format_eac_etc => If::SAMPLED_LINEAR,
            EAC_RG11Unorm if self.format_eac_etc => If::SAMPLED_LINEAR,
            EAC_RG11Snorm if self.format_eac_etc => If::SAMPLED_LINEAR,
            ETC2_RGB8 if self.format_eac_etc => If::SAMPLED_LINEAR,
            ETC2_RGB8_sRGB if self.format_eac_etc => If::SAMPLED_LINEAR,
            ETC2_RGB8A1 if self.format_eac_etc => If::SAMPLED_LINEAR,
            ETC2_RGB8A1_sRGB if self.format_eac_etc => If::SAMPLED_LINEAR,
            ASTC_4x4_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_4x4_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_5x4_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_5x4_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_5x5_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_5x5_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_6x5_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_6x5_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_6x6_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_6x6_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_8x5_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_8x5_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_8x6_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_8x6_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_8x8_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_8x8_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_10x5_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_10x5_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_10x6_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_10x6_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_10x8_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_10x8_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_10x10_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_10x10_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_12x10_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_12x10_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_12x12_LDR if self.format_astc => If::SAMPLED_LINEAR,
            ASTC_12x12_sRGB if self.format_astc => If::SAMPLED_LINEAR,
            BC1_RGBA if self.format_bc => If::SAMPLED_LINEAR,
            BC1_RGBA_sRGB if self.format_bc => If::SAMPLED_LINEAR,
            BC2_RGBA if self.format_bc => If::SAMPLED_LINEAR,
            BC2_RGBA_sRGB if self.format_bc => If::SAMPLED_LINEAR,
            BC3_RGBA if self.format_bc => If::SAMPLED_LINEAR,
            BC3_RGBA_sRGB if self.format_bc => If::SAMPLED_LINEAR,
            BC4_RUnorm if self.format_bc => If::SAMPLED_LINEAR,
            BC4_RSnorm if self.format_bc => If::SAMPLED_LINEAR,
            BC5_RGUnorm if self.format_bc => If::SAMPLED_LINEAR,
            BC5_RGSnorm if self.format_bc => If::SAMPLED_LINEAR,
            BC6H_RGBUfloat if self.format_bc => If::SAMPLED_LINEAR,
            BC6H_RGBFloat if self.format_bc => If::SAMPLED_LINEAR,
            BC7_RGBAUnorm if self.format_bc => If::SAMPLED_LINEAR,
            BC7_RGBAUnorm_sRGB if self.format_bc => If::SAMPLED_LINEAR,
            Depth16Unorm if self.format_depth16unorm => {
                If::DEPTH_STENCIL_ATTACHMENT | If::SAMPLED_LINEAR
            }
            Depth32Float if self.format_depth32float_filter => {
                If::DEPTH_STENCIL_ATTACHMENT | If::SAMPLED_LINEAR
            }
            Depth32Float if self.format_depth32float_none => If::DEPTH_STENCIL_ATTACHMENT,
            Stencil8 => If::empty(),
            Depth24Unorm_Stencil8 if self.format_depth24_stencil8 => If::DEPTH_STENCIL_ATTACHMENT,
            Depth32Float_Stencil8 if self.format_depth32_stencil8_filter => {
                If::DEPTH_STENCIL_ATTACHMENT | If::SAMPLED_LINEAR
            }
            Depth32Float_Stencil8 if self.format_depth32_stencil8_none => {
                If::DEPTH_STENCIL_ATTACHMENT
            }
            BGR10A2Unorm if self.format_bgr10a2_all => {
                If::SAMPLED_LINEAR | If::STORAGE | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            BGR10A2Unorm if self.format_bgr10a2_no_write => {
                If::SAMPLED_LINEAR | If::COLOR_ATTACHMENT | If::COLOR_ATTACHMENT_BLEND
            }
            _ => If::empty(),
        };

        Properties {
            linear_tiling: If::TRANSFER_SRC | If::TRANSFER_DST,
            optimal_tiling: If::SAMPLED
                | If::BLIT_SRC
                | If::BLIT_DST
                | If::TRANSFER_SRC
                | If::TRANSFER_DST
                | extra_optimal,
            buffer_features: Bf::all(),
        }
    }
}

pub fn map_load_operation(operation: pass::AttachmentLoadOp) -> MTLLoadAction {
    use self::pass::AttachmentLoadOp::*;

    match operation {
        Load => MTLLoadAction::Load,
        Clear => MTLLoadAction::Clear,
        DontCare => MTLLoadAction::DontCare,
    }
}

pub fn map_store_operation(operation: pass::AttachmentStoreOp) -> MTLStoreAction {
    use self::pass::AttachmentStoreOp::*;

    match operation {
        Store => MTLStoreAction::Store,
        DontCare => MTLStoreAction::DontCare,
    }
}

pub fn map_resolved_store_operation(operation: pass::AttachmentStoreOp) -> MTLStoreAction {
    use self::pass::AttachmentStoreOp::*;

    match operation {
        Store => MTLStoreAction::StoreAndMultisampleResolve,
        DontCare => MTLStoreAction::MultisampleResolve,
    }
}

pub fn map_write_mask(mask: pso::ColorMask) -> MTLColorWriteMask {
    let mut mtl_mask = MTLColorWriteMask::empty();

    if mask.contains(pso::ColorMask::RED) {
        mtl_mask |= MTLColorWriteMask::Red;
    }
    if mask.contains(pso::ColorMask::GREEN) {
        mtl_mask |= MTLColorWriteMask::Green;
    }
    if mask.contains(pso::ColorMask::BLUE) {
        mtl_mask |= MTLColorWriteMask::Blue;
    }
    if mask.contains(pso::ColorMask::ALPHA) {
        mtl_mask |= MTLColorWriteMask::Alpha;
    }

    mtl_mask
}

fn map_factor(factor: pso::Factor) -> MTLBlendFactor {
    use self::hal::pso::Factor::*;

    match factor {
        Zero => MTLBlendFactor::Zero,
        One => MTLBlendFactor::One,
        SrcColor => MTLBlendFactor::SourceColor,
        OneMinusSrcColor => MTLBlendFactor::OneMinusSourceColor,
        DstColor => MTLBlendFactor::DestinationColor,
        OneMinusDstColor => MTLBlendFactor::OneMinusDestinationColor,
        SrcAlpha => MTLBlendFactor::SourceAlpha,
        OneMinusSrcAlpha => MTLBlendFactor::OneMinusSourceAlpha,
        DstAlpha => MTLBlendFactor::DestinationAlpha,
        OneMinusDstAlpha => MTLBlendFactor::OneMinusDestinationAlpha,
        ConstColor => MTLBlendFactor::BlendColor,
        OneMinusConstColor => MTLBlendFactor::OneMinusBlendColor,
        ConstAlpha => MTLBlendFactor::BlendAlpha,
        OneMinusConstAlpha => MTLBlendFactor::OneMinusBlendAlpha,
        SrcAlphaSaturate => MTLBlendFactor::SourceAlphaSaturated,
        Src1Color => MTLBlendFactor::Source1Color,
        OneMinusSrc1Color => MTLBlendFactor::OneMinusSource1Color,
        Src1Alpha => MTLBlendFactor::Source1Alpha,
        OneMinusSrc1Alpha => MTLBlendFactor::OneMinusSource1Alpha,
    }
}

pub fn map_blend_op(
    operation: pso::BlendOp,
) -> (MTLBlendOperation, MTLBlendFactor, MTLBlendFactor) {
    use self::hal::pso::BlendOp::*;

    match operation {
        Add { src, dst } => (MTLBlendOperation::Add, map_factor(src), map_factor(dst)),
        Sub { src, dst } => (
            MTLBlendOperation::Subtract,
            map_factor(src),
            map_factor(dst),
        ),
        RevSub { src, dst } => (
            MTLBlendOperation::ReverseSubtract,
            map_factor(src),
            map_factor(dst),
        ),
        Min => (
            MTLBlendOperation::Min,
            MTLBlendFactor::Zero,
            MTLBlendFactor::Zero,
        ),
        Max => (
            MTLBlendOperation::Max,
            MTLBlendFactor::Zero,
            MTLBlendFactor::Zero,
        ),
    }
}

pub fn map_vertex_format(format: Format) -> Option<MTLVertexFormat> {
    use self::hal::format::Format as f;
    use metal::MTLVertexFormat::*;
    Some(match format {
        f::R8Unorm => UCharNormalized,
        f::R8Snorm => CharNormalized,
        f::R8Uint => UChar,
        f::R8Sint => Char,
        f::Rg8Unorm => UChar2Normalized,
        f::Rg8Snorm => Char2Normalized,
        f::Rg8Uint => UChar2,
        f::Rg8Sint => Char2,
        f::Rgb8Unorm => UChar3Normalized,
        f::Rgb8Snorm => Char3Normalized,
        f::Rgb8Uint => UChar3,
        f::Rgb8Sint => Char3,
        f::Rgba8Unorm => UChar4Normalized,
        f::Rgba8Snorm => Char4Normalized,
        f::Rgba8Uint => UChar4,
        f::Rgba8Sint => Char4,
        f::Bgra8Unorm => UChar4Normalized_BGRA,
        f::R16Unorm => UShortNormalized,
        f::R16Snorm => ShortNormalized,
        f::R16Uint => UShort,
        f::R16Sint => Short,
        f::R16Sfloat => Half,
        f::Rg16Unorm => UShort2Normalized,
        f::Rg16Snorm => Short2Normalized,
        f::Rg16Uint => UShort2,
        f::Rg16Sint => Short2,
        f::Rg16Sfloat => Half2,
        f::Rgb16Unorm => UShort3Normalized,
        f::Rgb16Snorm => Short3Normalized,
        f::Rgb16Uint => UShort3,
        f::Rgb16Sint => Short3,
        f::Rgb16Sfloat => Half3,
        f::Rgba16Unorm => UShort4Normalized,
        f::Rgba16Snorm => Short4Normalized,
        f::Rgba16Uint => UShort4,
        f::Rgba16Sint => Short4,
        f::Rgba16Sfloat => Half4,
        f::R32Uint => UInt,
        f::R32Sint => Int,
        f::R32Sfloat => Float,
        f::Rg32Uint => UInt2,
        f::Rg32Sint => Int2,
        f::Rg32Sfloat => Float2,
        f::Rgb32Uint => UInt3,
        f::Rgb32Sint => Int3,
        f::Rgb32Sfloat => Float3,
        f::Rgba32Uint => UInt4,
        f::Rgba32Sint => Int4,
        f::Rgba32Sfloat => Float4,
        _ => return None,
    })
}

pub fn resource_options_from_storage_and_cache(
    storage: MTLStorageMode,
    cache: MTLCPUCacheMode,
) -> MTLResourceOptions {
    MTLResourceOptions::from_bits(
        ((storage as u64) << MTLResourceStorageModeShift)
            | ((cache as u64) << MTLResourceCPUCacheModeShift),
    )
    .unwrap()
}

pub fn map_texture_usage(
    usage: image::Usage,
    tiling: image::Tiling,
    view_caps: image::ViewCapabilities,
) -> MTLTextureUsage {
    use self::hal::image::Usage as U;

    let mut texture_usage = MTLTextureUsage::Unknown;
    // We have to view the texture with a different format
    // in `clear_image` and `copy_image` destinations.
    if view_caps.contains(image::ViewCapabilities::MUTABLE_FORMAT)
        || usage.contains(U::TRANSFER_DST)
    {
        texture_usage |= MTLTextureUsage::PixelFormatView;
    }

    if usage.intersects(U::COLOR_ATTACHMENT | U::DEPTH_STENCIL_ATTACHMENT) {
        texture_usage |= MTLTextureUsage::RenderTarget;
    }
    if usage.intersects(U::SAMPLED | U::INPUT_ATTACHMENT) {
        texture_usage |= MTLTextureUsage::ShaderRead;
    }
    if usage.intersects(U::STORAGE) {
        texture_usage |= MTLTextureUsage::ShaderRead | MTLTextureUsage::ShaderWrite;
    }

    // Note: for blitting, we do actual rendering, so we add more flags for TRANSFER_* usage
    if usage.contains(U::TRANSFER_DST) && tiling == image::Tiling::Optimal {
        texture_usage |= MTLTextureUsage::RenderTarget;
    }
    if usage.contains(U::TRANSFER_SRC) {
        texture_usage |= MTLTextureUsage::ShaderRead;
    }

    texture_usage
}

pub fn map_texture_type(view_kind: image::ViewKind) -> MTLTextureType {
    use self::hal::image::ViewKind as Vk;
    match view_kind {
        Vk::D1 => MTLTextureType::D1,
        Vk::D1Array => MTLTextureType::D1Array,
        Vk::D2 => MTLTextureType::D2,
        Vk::D2Array => MTLTextureType::D2Array,
        Vk::D3 => MTLTextureType::D3,
        Vk::Cube => MTLTextureType::Cube,
        Vk::CubeArray => MTLTextureType::CubeArray,
    }
}

pub fn _map_index_type(index_type: IndexType) -> MTLIndexType {
    match index_type {
        IndexType::U16 => MTLIndexType::UInt16,
        IndexType::U32 => MTLIndexType::UInt32,
    }
}

pub fn map_compare_function(fun: Comparison) -> MTLCompareFunction {
    match fun {
        Comparison::Never => MTLCompareFunction::Never,
        Comparison::Less => MTLCompareFunction::Less,
        Comparison::LessEqual => MTLCompareFunction::LessEqual,
        Comparison::Equal => MTLCompareFunction::Equal,
        Comparison::GreaterEqual => MTLCompareFunction::GreaterEqual,
        Comparison::Greater => MTLCompareFunction::Greater,
        Comparison::NotEqual => MTLCompareFunction::NotEqual,
        Comparison::Always => MTLCompareFunction::Always,
    }
}

pub fn map_filter(filter: image::Filter) -> MTLSamplerMinMagFilter {
    match filter {
        image::Filter::Nearest => MTLSamplerMinMagFilter::Nearest,
        image::Filter::Linear => MTLSamplerMinMagFilter::Linear,
    }
}

pub fn map_wrap_mode(wrap: image::WrapMode) -> MTLSamplerAddressMode {
    match wrap {
        image::WrapMode::Tile => MTLSamplerAddressMode::Repeat,
        image::WrapMode::Mirror => MTLSamplerAddressMode::MirrorRepeat,
        image::WrapMode::Clamp => MTLSamplerAddressMode::ClampToEdge,
        image::WrapMode::Border => MTLSamplerAddressMode::ClampToBorderColor,
        image::WrapMode::MirrorClamp => MTLSamplerAddressMode::MirrorClampToEdge,
    }
}

pub fn map_border_color(border_color: image::BorderColor) -> MTLSamplerBorderColor {
    match border_color {
        image::BorderColor::TransparentBlack => MTLSamplerBorderColor::TransparentBlack,
        image::BorderColor::OpaqueBlack => MTLSamplerBorderColor::OpaqueBlack,
        image::BorderColor::OpaqueWhite => MTLSamplerBorderColor::OpaqueWhite,
    }
}

pub fn map_extent(extent: image::Extent) -> MTLSize {
    MTLSize {
        width: extent.width as _,
        height: extent.height as _,
        depth: extent.depth as _,
    }
}

pub fn map_offset(offset: image::Offset) -> MTLOrigin {
    MTLOrigin {
        x: offset.x as _,
        y: offset.y as _,
        z: offset.z as _,
    }
}

pub fn map_stencil_op(op: StencilOp) -> MTLStencilOperation {
    match op {
        StencilOp::Keep => MTLStencilOperation::Keep,
        StencilOp::Zero => MTLStencilOperation::Zero,
        StencilOp::Replace => MTLStencilOperation::Replace,
        StencilOp::IncrementClamp => MTLStencilOperation::IncrementClamp,
        StencilOp::IncrementWrap => MTLStencilOperation::IncrementWrap,
        StencilOp::DecrementClamp => MTLStencilOperation::DecrementClamp,
        StencilOp::DecrementWrap => MTLStencilOperation::DecrementWrap,
        StencilOp::Invert => MTLStencilOperation::Invert,
    }
}

pub fn map_winding(face: pso::FrontFace) -> MTLWinding {
    match face {
        pso::FrontFace::Clockwise => MTLWinding::Clockwise,
        pso::FrontFace::CounterClockwise => MTLWinding::CounterClockwise,
    }
}

pub fn map_polygon_mode(mode: pso::PolygonMode) -> MTLTriangleFillMode {
    match mode {
        pso::PolygonMode::Point => {
            warn!("Unable to fill with points");
            MTLTriangleFillMode::Lines
        }
        pso::PolygonMode::Line => MTLTriangleFillMode::Lines,
        pso::PolygonMode::Fill => MTLTriangleFillMode::Fill,
    }
}

pub fn map_cull_face(face: pso::Face) -> Option<MTLCullMode> {
    match face {
        pso::Face::NONE => Some(MTLCullMode::None),
        pso::Face::FRONT => Some(MTLCullMode::Front),
        pso::Face::BACK => Some(MTLCullMode::Back),
        _ => None,
    }
}

#[cfg(feature = "cross")]
pub fn map_naga_stage_to_cross(stage: naga::ShaderStage) -> spirv_cross::spirv::ExecutionModel {
    use spirv_cross::spirv::ExecutionModel as Em;
    match stage {
        naga::ShaderStage::Vertex => Em::Vertex,
        naga::ShaderStage::Fragment => Em::Fragment,
        naga::ShaderStage::Compute => Em::GlCompute,
    }
}

#[cfg(feature = "cross")]
pub fn map_sampler_data_to_cross(info: &image::SamplerDesc) -> spirv_cross::msl::SamplerData {
    use spirv_cross::msl;
    fn map_address(wrap: image::WrapMode) -> msl::SamplerAddress {
        match wrap {
            image::WrapMode::Tile => msl::SamplerAddress::Repeat,
            image::WrapMode::Mirror => msl::SamplerAddress::MirroredRepeat,
            image::WrapMode::Clamp => msl::SamplerAddress::ClampToEdge,
            image::WrapMode::Border => msl::SamplerAddress::ClampToBorder,
            image::WrapMode::MirrorClamp => {
                unimplemented!("https://github.com/grovesNL/spirv_cross/issues/138")
            }
        }
    }

    let lods = info.lod_range.start.0..info.lod_range.end.0;
    msl::SamplerData {
        coord: if info.normalized {
            msl::SamplerCoord::Normalized
        } else {
            msl::SamplerCoord::Pixel
        },
        min_filter: match info.min_filter {
            image::Filter::Nearest => msl::SamplerFilter::Nearest,
            image::Filter::Linear => msl::SamplerFilter::Linear,
        },
        mag_filter: match info.mag_filter {
            image::Filter::Nearest => msl::SamplerFilter::Nearest,
            image::Filter::Linear => msl::SamplerFilter::Linear,
        },
        mip_filter: match info.min_filter {
            image::Filter::Nearest if info.lod_range.end.0 < 0.5 => msl::SamplerMipFilter::None,
            image::Filter::Nearest => msl::SamplerMipFilter::Nearest,
            image::Filter::Linear => msl::SamplerMipFilter::Linear,
        },
        s_address: map_address(info.wrap_mode.0),
        t_address: map_address(info.wrap_mode.1),
        r_address: map_address(info.wrap_mode.2),
        compare_func: match info.comparison {
            Some(func) => unsafe { std::mem::transmute(map_compare_function(func) as u32) },
            None => msl::SamplerCompareFunc::Always,
        },
        border_color: match info.border {
            image::BorderColor::TransparentBlack => msl::SamplerBorderColor::TransparentBlack,
            image::BorderColor::OpaqueBlack => msl::SamplerBorderColor::OpaqueBlack,
            image::BorderColor::OpaqueWhite => msl::SamplerBorderColor::OpaqueWhite,
        },
        lod_clamp_min: lods.start.into(),
        lod_clamp_max: lods.end.into(),
        max_anisotropy: info.anisotropy_clamp.map_or(0, |aniso| aniso as i32),
        planes: 0,
        resolution: msl::FormatResolution::_444,
        chroma_filter: msl::SamplerFilter::Nearest,
        x_chroma_offset: msl::ChromaLocation::CositedEven,
        y_chroma_offset: msl::ChromaLocation::CositedEven,
        swizzle: [
            msl::ComponentSwizzle::Identity,
            msl::ComponentSwizzle::Identity,
            msl::ComponentSwizzle::Identity,
            msl::ComponentSwizzle::Identity,
        ],
        ycbcr_conversion_enable: false,
        ycbcr_model: msl::SamplerYCbCrModelConversion::RgbIdentity,
        ycbcr_range: msl::SamplerYCbCrRange::ItuFull,
        bpc: 8,
    }
}

pub fn map_sampler_data_to_naga(
    info: &image::SamplerDesc,
) -> naga::back::msl::sampler::InlineSampler {
    use naga::back::msl::sampler as sm;
    fn map_address(wrap: image::WrapMode) -> sm::Address {
        match wrap {
            image::WrapMode::Tile => sm::Address::Repeat,
            image::WrapMode::Mirror => sm::Address::MirroredRepeat,
            image::WrapMode::Clamp => sm::Address::ClampToEdge,
            image::WrapMode::Border => sm::Address::ClampToBorder,
            image::WrapMode::MirrorClamp => {
                error!("Unsupported address mode - MirrorClamp");
                sm::Address::ClampToEdge
            }
        }
    }

    sm::InlineSampler {
        coord: if info.normalized {
            sm::Coord::Normalized
        } else {
            sm::Coord::Pixel
        },
        min_filter: match info.min_filter {
            image::Filter::Nearest => sm::Filter::Nearest,
            image::Filter::Linear => sm::Filter::Linear,
        },
        mag_filter: match info.mag_filter {
            image::Filter::Nearest => sm::Filter::Nearest,
            image::Filter::Linear => sm::Filter::Linear,
        },
        mip_filter: match info.min_filter {
            image::Filter::Nearest if info.lod_range.end.0 < 0.5 => None,
            image::Filter::Nearest => Some(sm::Filter::Nearest),
            image::Filter::Linear => Some(sm::Filter::Linear),
        },
        address: [
            map_address(info.wrap_mode.0),
            map_address(info.wrap_mode.1),
            map_address(info.wrap_mode.2),
        ],
        compare_func: match info.comparison {
            Some(func) => match func {
                Comparison::Never => sm::CompareFunc::Never,
                Comparison::Less => sm::CompareFunc::Less,
                Comparison::LessEqual => sm::CompareFunc::LessEqual,
                Comparison::Equal => sm::CompareFunc::Equal,
                Comparison::GreaterEqual => sm::CompareFunc::GreaterEqual,
                Comparison::Greater => sm::CompareFunc::Greater,
                Comparison::NotEqual => sm::CompareFunc::NotEqual,
                Comparison::Always => sm::CompareFunc::Always,
            },
            None => sm::CompareFunc::Never,
        },
        border_color: match info.border {
            image::BorderColor::TransparentBlack => sm::BorderColor::TransparentBlack,
            image::BorderColor::OpaqueBlack => sm::BorderColor::OpaqueBlack,
            image::BorderColor::OpaqueWhite => sm::BorderColor::OpaqueWhite,
        },
        lod_clamp: if info.lod_range.start.0 > 0.0 || info.lod_range.end.0 < 100.0 {
            Some(info.lod_range.start.0..info.lod_range.end.0)
        } else {
            None
        },
        max_anisotropy: info
            .anisotropy_clamp
            .and_then(|aniso| NonZeroU32::new(aniso as u32)),
    }
}
