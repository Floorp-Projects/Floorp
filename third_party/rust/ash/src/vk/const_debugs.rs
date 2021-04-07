use crate::vk::bitflags::*;
use crate::vk::definitions::*;
use crate::vk::enums::*;
use std::fmt;
pub(crate) fn debug_flags(
    f: &mut fmt::Formatter,
    known: &[(Flags, &'static str)],
    value: Flags,
) -> fmt::Result {
    let mut first = true;
    let mut accum = value;
    for (bit, name) in known {
        if *bit != 0 && accum & *bit == *bit {
            if !first {
                f.write_str(" | ")?;
            }
            f.write_str(name)?;
            first = false;
            accum &= !bit;
        }
    }
    if accum != 0 {
        if !first {
            f.write_str(" | ")?;
        }
        write!(f, "{:b}", accum)?;
    }
    Ok(())
}
impl fmt::Debug for AccelerationStructureBuildTypeKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::HOST => Some("HOST"),
            Self::DEVICE => Some("DEVICE"),
            Self::HOST_OR_DEVICE => Some("HOST_OR_DEVICE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for AccelerationStructureCompatibilityKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::COMPATIBLE => Some("COMPATIBLE"),
            Self::INCOMPATIBLE => Some("INCOMPATIBLE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for AccelerationStructureCreateFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                AccelerationStructureCreateFlagsKHR::DEVICE_ADDRESS_CAPTURE_REPLAY.0,
                "DEVICE_ADDRESS_CAPTURE_REPLAY",
            ),
            (
                AccelerationStructureCreateFlagsKHR::RESERVED_2_NV.0,
                "RESERVED_2_NV",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for AccelerationStructureMemoryRequirementsTypeNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::OBJECT => Some("OBJECT"),
            Self::BUILD_SCRATCH => Some("BUILD_SCRATCH"),
            Self::UPDATE_SCRATCH => Some("UPDATE_SCRATCH"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for AccelerationStructureTypeKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::TOP_LEVEL => Some("TOP_LEVEL"),
            Self::BOTTOM_LEVEL => Some("BOTTOM_LEVEL"),
            Self::GENERIC => Some("GENERIC"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for AccessFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                AccessFlags::INDIRECT_COMMAND_READ.0,
                "INDIRECT_COMMAND_READ",
            ),
            (AccessFlags::INDEX_READ.0, "INDEX_READ"),
            (
                AccessFlags::VERTEX_ATTRIBUTE_READ.0,
                "VERTEX_ATTRIBUTE_READ",
            ),
            (AccessFlags::UNIFORM_READ.0, "UNIFORM_READ"),
            (
                AccessFlags::INPUT_ATTACHMENT_READ.0,
                "INPUT_ATTACHMENT_READ",
            ),
            (AccessFlags::SHADER_READ.0, "SHADER_READ"),
            (AccessFlags::SHADER_WRITE.0, "SHADER_WRITE"),
            (
                AccessFlags::COLOR_ATTACHMENT_READ.0,
                "COLOR_ATTACHMENT_READ",
            ),
            (
                AccessFlags::COLOR_ATTACHMENT_WRITE.0,
                "COLOR_ATTACHMENT_WRITE",
            ),
            (
                AccessFlags::DEPTH_STENCIL_ATTACHMENT_READ.0,
                "DEPTH_STENCIL_ATTACHMENT_READ",
            ),
            (
                AccessFlags::DEPTH_STENCIL_ATTACHMENT_WRITE.0,
                "DEPTH_STENCIL_ATTACHMENT_WRITE",
            ),
            (AccessFlags::TRANSFER_READ.0, "TRANSFER_READ"),
            (AccessFlags::TRANSFER_WRITE.0, "TRANSFER_WRITE"),
            (AccessFlags::HOST_READ.0, "HOST_READ"),
            (AccessFlags::HOST_WRITE.0, "HOST_WRITE"),
            (AccessFlags::MEMORY_READ.0, "MEMORY_READ"),
            (AccessFlags::MEMORY_WRITE.0, "MEMORY_WRITE"),
            (AccessFlags::RESERVED_30_KHR.0, "RESERVED_30_KHR"),
            (AccessFlags::RESERVED_28_KHR.0, "RESERVED_28_KHR"),
            (AccessFlags::RESERVED_29_KHR.0, "RESERVED_29_KHR"),
            (
                AccessFlags::TRANSFORM_FEEDBACK_WRITE_EXT.0,
                "TRANSFORM_FEEDBACK_WRITE_EXT",
            ),
            (
                AccessFlags::TRANSFORM_FEEDBACK_COUNTER_READ_EXT.0,
                "TRANSFORM_FEEDBACK_COUNTER_READ_EXT",
            ),
            (
                AccessFlags::TRANSFORM_FEEDBACK_COUNTER_WRITE_EXT.0,
                "TRANSFORM_FEEDBACK_COUNTER_WRITE_EXT",
            ),
            (
                AccessFlags::CONDITIONAL_RENDERING_READ_EXT.0,
                "CONDITIONAL_RENDERING_READ_EXT",
            ),
            (
                AccessFlags::COLOR_ATTACHMENT_READ_NONCOHERENT_EXT.0,
                "COLOR_ATTACHMENT_READ_NONCOHERENT_EXT",
            ),
            (
                AccessFlags::ACCELERATION_STRUCTURE_READ_KHR.0,
                "ACCELERATION_STRUCTURE_READ_KHR",
            ),
            (
                AccessFlags::ACCELERATION_STRUCTURE_WRITE_KHR.0,
                "ACCELERATION_STRUCTURE_WRITE_KHR",
            ),
            (
                AccessFlags::SHADING_RATE_IMAGE_READ_NV.0,
                "SHADING_RATE_IMAGE_READ_NV",
            ),
            (
                AccessFlags::FRAGMENT_DENSITY_MAP_READ_EXT.0,
                "FRAGMENT_DENSITY_MAP_READ_EXT",
            ),
            (
                AccessFlags::COMMAND_PREPROCESS_READ_NV.0,
                "COMMAND_PREPROCESS_READ_NV",
            ),
            (
                AccessFlags::COMMAND_PREPROCESS_WRITE_NV.0,
                "COMMAND_PREPROCESS_WRITE_NV",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for AcquireProfilingLockFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for AndroidSurfaceCreateFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for AttachmentDescriptionFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(AttachmentDescriptionFlags::MAY_ALIAS.0, "MAY_ALIAS")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for AttachmentLoadOp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::LOAD => Some("LOAD"),
            Self::CLEAR => Some("CLEAR"),
            Self::DONT_CARE => Some("DONT_CARE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for AttachmentStoreOp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::STORE => Some("STORE"),
            Self::DONT_CARE => Some("DONT_CARE"),
            Self::NONE_QCOM => Some("NONE_QCOM"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for BlendFactor {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::ZERO => Some("ZERO"),
            Self::ONE => Some("ONE"),
            Self::SRC_COLOR => Some("SRC_COLOR"),
            Self::ONE_MINUS_SRC_COLOR => Some("ONE_MINUS_SRC_COLOR"),
            Self::DST_COLOR => Some("DST_COLOR"),
            Self::ONE_MINUS_DST_COLOR => Some("ONE_MINUS_DST_COLOR"),
            Self::SRC_ALPHA => Some("SRC_ALPHA"),
            Self::ONE_MINUS_SRC_ALPHA => Some("ONE_MINUS_SRC_ALPHA"),
            Self::DST_ALPHA => Some("DST_ALPHA"),
            Self::ONE_MINUS_DST_ALPHA => Some("ONE_MINUS_DST_ALPHA"),
            Self::CONSTANT_COLOR => Some("CONSTANT_COLOR"),
            Self::ONE_MINUS_CONSTANT_COLOR => Some("ONE_MINUS_CONSTANT_COLOR"),
            Self::CONSTANT_ALPHA => Some("CONSTANT_ALPHA"),
            Self::ONE_MINUS_CONSTANT_ALPHA => Some("ONE_MINUS_CONSTANT_ALPHA"),
            Self::SRC_ALPHA_SATURATE => Some("SRC_ALPHA_SATURATE"),
            Self::SRC1_COLOR => Some("SRC1_COLOR"),
            Self::ONE_MINUS_SRC1_COLOR => Some("ONE_MINUS_SRC1_COLOR"),
            Self::SRC1_ALPHA => Some("SRC1_ALPHA"),
            Self::ONE_MINUS_SRC1_ALPHA => Some("ONE_MINUS_SRC1_ALPHA"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for BlendOp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::ADD => Some("ADD"),
            Self::SUBTRACT => Some("SUBTRACT"),
            Self::REVERSE_SUBTRACT => Some("REVERSE_SUBTRACT"),
            Self::MIN => Some("MIN"),
            Self::MAX => Some("MAX"),
            Self::ZERO_EXT => Some("ZERO_EXT"),
            Self::SRC_EXT => Some("SRC_EXT"),
            Self::DST_EXT => Some("DST_EXT"),
            Self::SRC_OVER_EXT => Some("SRC_OVER_EXT"),
            Self::DST_OVER_EXT => Some("DST_OVER_EXT"),
            Self::SRC_IN_EXT => Some("SRC_IN_EXT"),
            Self::DST_IN_EXT => Some("DST_IN_EXT"),
            Self::SRC_OUT_EXT => Some("SRC_OUT_EXT"),
            Self::DST_OUT_EXT => Some("DST_OUT_EXT"),
            Self::SRC_ATOP_EXT => Some("SRC_ATOP_EXT"),
            Self::DST_ATOP_EXT => Some("DST_ATOP_EXT"),
            Self::XOR_EXT => Some("XOR_EXT"),
            Self::MULTIPLY_EXT => Some("MULTIPLY_EXT"),
            Self::SCREEN_EXT => Some("SCREEN_EXT"),
            Self::OVERLAY_EXT => Some("OVERLAY_EXT"),
            Self::DARKEN_EXT => Some("DARKEN_EXT"),
            Self::LIGHTEN_EXT => Some("LIGHTEN_EXT"),
            Self::COLORDODGE_EXT => Some("COLORDODGE_EXT"),
            Self::COLORBURN_EXT => Some("COLORBURN_EXT"),
            Self::HARDLIGHT_EXT => Some("HARDLIGHT_EXT"),
            Self::SOFTLIGHT_EXT => Some("SOFTLIGHT_EXT"),
            Self::DIFFERENCE_EXT => Some("DIFFERENCE_EXT"),
            Self::EXCLUSION_EXT => Some("EXCLUSION_EXT"),
            Self::INVERT_EXT => Some("INVERT_EXT"),
            Self::INVERT_RGB_EXT => Some("INVERT_RGB_EXT"),
            Self::LINEARDODGE_EXT => Some("LINEARDODGE_EXT"),
            Self::LINEARBURN_EXT => Some("LINEARBURN_EXT"),
            Self::VIVIDLIGHT_EXT => Some("VIVIDLIGHT_EXT"),
            Self::LINEARLIGHT_EXT => Some("LINEARLIGHT_EXT"),
            Self::PINLIGHT_EXT => Some("PINLIGHT_EXT"),
            Self::HARDMIX_EXT => Some("HARDMIX_EXT"),
            Self::HSL_HUE_EXT => Some("HSL_HUE_EXT"),
            Self::HSL_SATURATION_EXT => Some("HSL_SATURATION_EXT"),
            Self::HSL_COLOR_EXT => Some("HSL_COLOR_EXT"),
            Self::HSL_LUMINOSITY_EXT => Some("HSL_LUMINOSITY_EXT"),
            Self::PLUS_EXT => Some("PLUS_EXT"),
            Self::PLUS_CLAMPED_EXT => Some("PLUS_CLAMPED_EXT"),
            Self::PLUS_CLAMPED_ALPHA_EXT => Some("PLUS_CLAMPED_ALPHA_EXT"),
            Self::PLUS_DARKER_EXT => Some("PLUS_DARKER_EXT"),
            Self::MINUS_EXT => Some("MINUS_EXT"),
            Self::MINUS_CLAMPED_EXT => Some("MINUS_CLAMPED_EXT"),
            Self::CONTRAST_EXT => Some("CONTRAST_EXT"),
            Self::INVERT_OVG_EXT => Some("INVERT_OVG_EXT"),
            Self::RED_EXT => Some("RED_EXT"),
            Self::GREEN_EXT => Some("GREEN_EXT"),
            Self::BLUE_EXT => Some("BLUE_EXT"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for BlendOverlapEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::UNCORRELATED => Some("UNCORRELATED"),
            Self::DISJOINT => Some("DISJOINT"),
            Self::CONJOINT => Some("CONJOINT"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for BorderColor {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::FLOAT_TRANSPARENT_BLACK => Some("FLOAT_TRANSPARENT_BLACK"),
            Self::INT_TRANSPARENT_BLACK => Some("INT_TRANSPARENT_BLACK"),
            Self::FLOAT_OPAQUE_BLACK => Some("FLOAT_OPAQUE_BLACK"),
            Self::INT_OPAQUE_BLACK => Some("INT_OPAQUE_BLACK"),
            Self::FLOAT_OPAQUE_WHITE => Some("FLOAT_OPAQUE_WHITE"),
            Self::INT_OPAQUE_WHITE => Some("INT_OPAQUE_WHITE"),
            Self::FLOAT_CUSTOM_EXT => Some("FLOAT_CUSTOM_EXT"),
            Self::INT_CUSTOM_EXT => Some("INT_CUSTOM_EXT"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for BufferCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (BufferCreateFlags::SPARSE_BINDING.0, "SPARSE_BINDING"),
            (BufferCreateFlags::SPARSE_RESIDENCY.0, "SPARSE_RESIDENCY"),
            (BufferCreateFlags::SPARSE_ALIASED.0, "SPARSE_ALIASED"),
            (BufferCreateFlags::RESERVED_5_NV.0, "RESERVED_5_NV"),
            (BufferCreateFlags::PROTECTED.0, "PROTECTED"),
            (
                BufferCreateFlags::DEVICE_ADDRESS_CAPTURE_REPLAY.0,
                "DEVICE_ADDRESS_CAPTURE_REPLAY",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for BufferUsageFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (BufferUsageFlags::TRANSFER_SRC.0, "TRANSFER_SRC"),
            (BufferUsageFlags::TRANSFER_DST.0, "TRANSFER_DST"),
            (
                BufferUsageFlags::UNIFORM_TEXEL_BUFFER.0,
                "UNIFORM_TEXEL_BUFFER",
            ),
            (
                BufferUsageFlags::STORAGE_TEXEL_BUFFER.0,
                "STORAGE_TEXEL_BUFFER",
            ),
            (BufferUsageFlags::UNIFORM_BUFFER.0, "UNIFORM_BUFFER"),
            (BufferUsageFlags::STORAGE_BUFFER.0, "STORAGE_BUFFER"),
            (BufferUsageFlags::INDEX_BUFFER.0, "INDEX_BUFFER"),
            (BufferUsageFlags::VERTEX_BUFFER.0, "VERTEX_BUFFER"),
            (BufferUsageFlags::INDIRECT_BUFFER.0, "INDIRECT_BUFFER"),
            (BufferUsageFlags::RESERVED_15_KHR.0, "RESERVED_15_KHR"),
            (BufferUsageFlags::RESERVED_16_KHR.0, "RESERVED_16_KHR"),
            (BufferUsageFlags::RESERVED_13_KHR.0, "RESERVED_13_KHR"),
            (BufferUsageFlags::RESERVED_14_KHR.0, "RESERVED_14_KHR"),
            (
                BufferUsageFlags::TRANSFORM_FEEDBACK_BUFFER_EXT.0,
                "TRANSFORM_FEEDBACK_BUFFER_EXT",
            ),
            (
                BufferUsageFlags::TRANSFORM_FEEDBACK_COUNTER_BUFFER_EXT.0,
                "TRANSFORM_FEEDBACK_COUNTER_BUFFER_EXT",
            ),
            (
                BufferUsageFlags::CONDITIONAL_RENDERING_EXT.0,
                "CONDITIONAL_RENDERING_EXT",
            ),
            (
                BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_KHR.0,
                "ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_KHR",
            ),
            (
                BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE_KHR.0,
                "ACCELERATION_STRUCTURE_STORAGE_KHR",
            ),
            (
                BufferUsageFlags::SHADER_BINDING_TABLE_KHR.0,
                "SHADER_BINDING_TABLE_KHR",
            ),
            (BufferUsageFlags::RESERVED_18_QCOM.0, "RESERVED_18_QCOM"),
            (
                BufferUsageFlags::SHADER_DEVICE_ADDRESS.0,
                "SHADER_DEVICE_ADDRESS",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for BufferViewCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for BuildAccelerationStructureFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                BuildAccelerationStructureFlagsKHR::ALLOW_UPDATE.0,
                "ALLOW_UPDATE",
            ),
            (
                BuildAccelerationStructureFlagsKHR::ALLOW_COMPACTION.0,
                "ALLOW_COMPACTION",
            ),
            (
                BuildAccelerationStructureFlagsKHR::PREFER_FAST_TRACE.0,
                "PREFER_FAST_TRACE",
            ),
            (
                BuildAccelerationStructureFlagsKHR::PREFER_FAST_BUILD.0,
                "PREFER_FAST_BUILD",
            ),
            (
                BuildAccelerationStructureFlagsKHR::LOW_MEMORY.0,
                "LOW_MEMORY",
            ),
            (
                BuildAccelerationStructureFlagsKHR::RESERVED_5_NV.0,
                "RESERVED_5_NV",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for BuildAccelerationStructureModeKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::BUILD => Some("BUILD"),
            Self::UPDATE => Some("UPDATE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ChromaLocation {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::COSITED_EVEN => Some("COSITED_EVEN"),
            Self::MIDPOINT => Some("MIDPOINT"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for CoarseSampleOrderTypeNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::DEFAULT => Some("DEFAULT"),
            Self::CUSTOM => Some("CUSTOM"),
            Self::PIXEL_MAJOR => Some("PIXEL_MAJOR"),
            Self::SAMPLE_MAJOR => Some("SAMPLE_MAJOR"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ColorComponentFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ColorComponentFlags::R.0, "R"),
            (ColorComponentFlags::G.0, "G"),
            (ColorComponentFlags::B.0, "B"),
            (ColorComponentFlags::A.0, "A"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ColorSpaceKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::SRGB_NONLINEAR => Some("SRGB_NONLINEAR"),
            Self::DISPLAY_P3_NONLINEAR_EXT => Some("DISPLAY_P3_NONLINEAR_EXT"),
            Self::EXTENDED_SRGB_LINEAR_EXT => Some("EXTENDED_SRGB_LINEAR_EXT"),
            Self::DISPLAY_P3_LINEAR_EXT => Some("DISPLAY_P3_LINEAR_EXT"),
            Self::DCI_P3_NONLINEAR_EXT => Some("DCI_P3_NONLINEAR_EXT"),
            Self::BT709_LINEAR_EXT => Some("BT709_LINEAR_EXT"),
            Self::BT709_NONLINEAR_EXT => Some("BT709_NONLINEAR_EXT"),
            Self::BT2020_LINEAR_EXT => Some("BT2020_LINEAR_EXT"),
            Self::HDR10_ST2084_EXT => Some("HDR10_ST2084_EXT"),
            Self::DOLBYVISION_EXT => Some("DOLBYVISION_EXT"),
            Self::HDR10_HLG_EXT => Some("HDR10_HLG_EXT"),
            Self::ADOBERGB_LINEAR_EXT => Some("ADOBERGB_LINEAR_EXT"),
            Self::ADOBERGB_NONLINEAR_EXT => Some("ADOBERGB_NONLINEAR_EXT"),
            Self::PASS_THROUGH_EXT => Some("PASS_THROUGH_EXT"),
            Self::EXTENDED_SRGB_NONLINEAR_EXT => Some("EXTENDED_SRGB_NONLINEAR_EXT"),
            Self::DISPLAY_NATIVE_AMD => Some("DISPLAY_NATIVE_AMD"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for CommandBufferLevel {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::PRIMARY => Some("PRIMARY"),
            Self::SECONDARY => Some("SECONDARY"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for CommandBufferResetFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(
            CommandBufferResetFlags::RELEASE_RESOURCES.0,
            "RELEASE_RESOURCES",
        )];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for CommandBufferUsageFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                CommandBufferUsageFlags::ONE_TIME_SUBMIT.0,
                "ONE_TIME_SUBMIT",
            ),
            (
                CommandBufferUsageFlags::RENDER_PASS_CONTINUE.0,
                "RENDER_PASS_CONTINUE",
            ),
            (
                CommandBufferUsageFlags::SIMULTANEOUS_USE.0,
                "SIMULTANEOUS_USE",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for CommandPoolCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (CommandPoolCreateFlags::TRANSIENT.0, "TRANSIENT"),
            (
                CommandPoolCreateFlags::RESET_COMMAND_BUFFER.0,
                "RESET_COMMAND_BUFFER",
            ),
            (CommandPoolCreateFlags::PROTECTED.0, "PROTECTED"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for CommandPoolResetFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(
            CommandPoolResetFlags::RELEASE_RESOURCES.0,
            "RELEASE_RESOURCES",
        )];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for CommandPoolTrimFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for CompareOp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::NEVER => Some("NEVER"),
            Self::LESS => Some("LESS"),
            Self::EQUAL => Some("EQUAL"),
            Self::LESS_OR_EQUAL => Some("LESS_OR_EQUAL"),
            Self::GREATER => Some("GREATER"),
            Self::NOT_EQUAL => Some("NOT_EQUAL"),
            Self::GREATER_OR_EQUAL => Some("GREATER_OR_EQUAL"),
            Self::ALWAYS => Some("ALWAYS"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ComponentSwizzle {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::IDENTITY => Some("IDENTITY"),
            Self::ZERO => Some("ZERO"),
            Self::ONE => Some("ONE"),
            Self::R => Some("R"),
            Self::G => Some("G"),
            Self::B => Some("B"),
            Self::A => Some("A"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ComponentTypeNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::FLOAT16 => Some("FLOAT16"),
            Self::FLOAT32 => Some("FLOAT32"),
            Self::FLOAT64 => Some("FLOAT64"),
            Self::SINT8 => Some("SINT8"),
            Self::SINT16 => Some("SINT16"),
            Self::SINT32 => Some("SINT32"),
            Self::SINT64 => Some("SINT64"),
            Self::UINT8 => Some("UINT8"),
            Self::UINT16 => Some("UINT16"),
            Self::UINT32 => Some("UINT32"),
            Self::UINT64 => Some("UINT64"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for CompositeAlphaFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (CompositeAlphaFlagsKHR::OPAQUE.0, "OPAQUE"),
            (CompositeAlphaFlagsKHR::PRE_MULTIPLIED.0, "PRE_MULTIPLIED"),
            (CompositeAlphaFlagsKHR::POST_MULTIPLIED.0, "POST_MULTIPLIED"),
            (CompositeAlphaFlagsKHR::INHERIT.0, "INHERIT"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ConditionalRenderingFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(ConditionalRenderingFlagsEXT::INVERTED.0, "INVERTED")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ConservativeRasterizationModeEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::DISABLED => Some("DISABLED"),
            Self::OVERESTIMATE => Some("OVERESTIMATE"),
            Self::UNDERESTIMATE => Some("UNDERESTIMATE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for CopyAccelerationStructureModeKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::CLONE => Some("CLONE"),
            Self::COMPACT => Some("COMPACT"),
            Self::SERIALIZE => Some("SERIALIZE"),
            Self::DESERIALIZE => Some("DESERIALIZE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for CoverageModulationModeNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::NONE => Some("NONE"),
            Self::RGB => Some("RGB"),
            Self::ALPHA => Some("ALPHA"),
            Self::RGBA => Some("RGBA"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for CoverageReductionModeNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::MERGE => Some("MERGE"),
            Self::TRUNCATE => Some("TRUNCATE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for CullModeFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (CullModeFlags::NONE.0, "NONE"),
            (CullModeFlags::FRONT.0, "FRONT"),
            (CullModeFlags::BACK.0, "BACK"),
            (CullModeFlags::FRONT_AND_BACK.0, "FRONT_AND_BACK"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DebugReportFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (DebugReportFlagsEXT::INFORMATION.0, "INFORMATION"),
            (DebugReportFlagsEXT::WARNING.0, "WARNING"),
            (
                DebugReportFlagsEXT::PERFORMANCE_WARNING.0,
                "PERFORMANCE_WARNING",
            ),
            (DebugReportFlagsEXT::ERROR.0, "ERROR"),
            (DebugReportFlagsEXT::DEBUG.0, "DEBUG"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DebugReportObjectTypeEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::UNKNOWN => Some("UNKNOWN"),
            Self::INSTANCE => Some("INSTANCE"),
            Self::PHYSICAL_DEVICE => Some("PHYSICAL_DEVICE"),
            Self::DEVICE => Some("DEVICE"),
            Self::QUEUE => Some("QUEUE"),
            Self::SEMAPHORE => Some("SEMAPHORE"),
            Self::COMMAND_BUFFER => Some("COMMAND_BUFFER"),
            Self::FENCE => Some("FENCE"),
            Self::DEVICE_MEMORY => Some("DEVICE_MEMORY"),
            Self::BUFFER => Some("BUFFER"),
            Self::IMAGE => Some("IMAGE"),
            Self::EVENT => Some("EVENT"),
            Self::QUERY_POOL => Some("QUERY_POOL"),
            Self::BUFFER_VIEW => Some("BUFFER_VIEW"),
            Self::IMAGE_VIEW => Some("IMAGE_VIEW"),
            Self::SHADER_MODULE => Some("SHADER_MODULE"),
            Self::PIPELINE_CACHE => Some("PIPELINE_CACHE"),
            Self::PIPELINE_LAYOUT => Some("PIPELINE_LAYOUT"),
            Self::RENDER_PASS => Some("RENDER_PASS"),
            Self::PIPELINE => Some("PIPELINE"),
            Self::DESCRIPTOR_SET_LAYOUT => Some("DESCRIPTOR_SET_LAYOUT"),
            Self::SAMPLER => Some("SAMPLER"),
            Self::DESCRIPTOR_POOL => Some("DESCRIPTOR_POOL"),
            Self::DESCRIPTOR_SET => Some("DESCRIPTOR_SET"),
            Self::FRAMEBUFFER => Some("FRAMEBUFFER"),
            Self::COMMAND_POOL => Some("COMMAND_POOL"),
            Self::SURFACE_KHR => Some("SURFACE_KHR"),
            Self::SWAPCHAIN_KHR => Some("SWAPCHAIN_KHR"),
            Self::DEBUG_REPORT_CALLBACK_EXT => Some("DEBUG_REPORT_CALLBACK_EXT"),
            Self::DISPLAY_KHR => Some("DISPLAY_KHR"),
            Self::DISPLAY_MODE_KHR => Some("DISPLAY_MODE_KHR"),
            Self::VALIDATION_CACHE_EXT => Some("VALIDATION_CACHE_EXT"),
            Self::SAMPLER_YCBCR_CONVERSION => Some("SAMPLER_YCBCR_CONVERSION"),
            Self::DESCRIPTOR_UPDATE_TEMPLATE => Some("DESCRIPTOR_UPDATE_TEMPLATE"),
            Self::ACCELERATION_STRUCTURE_KHR => Some("ACCELERATION_STRUCTURE_KHR"),
            Self::ACCELERATION_STRUCTURE_NV => Some("ACCELERATION_STRUCTURE_NV"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for DebugUtilsMessageSeverityFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (DebugUtilsMessageSeverityFlagsEXT::VERBOSE.0, "VERBOSE"),
            (DebugUtilsMessageSeverityFlagsEXT::INFO.0, "INFO"),
            (DebugUtilsMessageSeverityFlagsEXT::WARNING.0, "WARNING"),
            (DebugUtilsMessageSeverityFlagsEXT::ERROR.0, "ERROR"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DebugUtilsMessageTypeFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (DebugUtilsMessageTypeFlagsEXT::GENERAL.0, "GENERAL"),
            (DebugUtilsMessageTypeFlagsEXT::VALIDATION.0, "VALIDATION"),
            (DebugUtilsMessageTypeFlagsEXT::PERFORMANCE.0, "PERFORMANCE"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DebugUtilsMessengerCallbackDataFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DebugUtilsMessengerCreateFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DependencyFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (DependencyFlags::BY_REGION.0, "BY_REGION"),
            (DependencyFlags::DEVICE_GROUP.0, "DEVICE_GROUP"),
            (DependencyFlags::VIEW_LOCAL.0, "VIEW_LOCAL"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DescriptorBindingFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                DescriptorBindingFlags::UPDATE_AFTER_BIND.0,
                "UPDATE_AFTER_BIND",
            ),
            (
                DescriptorBindingFlags::UPDATE_UNUSED_WHILE_PENDING.0,
                "UPDATE_UNUSED_WHILE_PENDING",
            ),
            (DescriptorBindingFlags::PARTIALLY_BOUND.0, "PARTIALLY_BOUND"),
            (
                DescriptorBindingFlags::VARIABLE_DESCRIPTOR_COUNT.0,
                "VARIABLE_DESCRIPTOR_COUNT",
            ),
            (DescriptorBindingFlags::RESERVED_4_QCOM.0, "RESERVED_4_QCOM"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DescriptorPoolCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                DescriptorPoolCreateFlags::FREE_DESCRIPTOR_SET.0,
                "FREE_DESCRIPTOR_SET",
            ),
            (
                DescriptorPoolCreateFlags::HOST_ONLY_VALVE.0,
                "HOST_ONLY_VALVE",
            ),
            (
                DescriptorPoolCreateFlags::UPDATE_AFTER_BIND.0,
                "UPDATE_AFTER_BIND",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DescriptorPoolResetFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DescriptorSetLayoutCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                DescriptorSetLayoutCreateFlags::PUSH_DESCRIPTOR_KHR.0,
                "PUSH_DESCRIPTOR_KHR",
            ),
            (
                DescriptorSetLayoutCreateFlags::HOST_ONLY_POOL_VALVE.0,
                "HOST_ONLY_POOL_VALVE",
            ),
            (
                DescriptorSetLayoutCreateFlags::UPDATE_AFTER_BIND_POOL.0,
                "UPDATE_AFTER_BIND_POOL",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DescriptorType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::SAMPLER => Some("SAMPLER"),
            Self::COMBINED_IMAGE_SAMPLER => Some("COMBINED_IMAGE_SAMPLER"),
            Self::SAMPLED_IMAGE => Some("SAMPLED_IMAGE"),
            Self::STORAGE_IMAGE => Some("STORAGE_IMAGE"),
            Self::UNIFORM_TEXEL_BUFFER => Some("UNIFORM_TEXEL_BUFFER"),
            Self::STORAGE_TEXEL_BUFFER => Some("STORAGE_TEXEL_BUFFER"),
            Self::UNIFORM_BUFFER => Some("UNIFORM_BUFFER"),
            Self::STORAGE_BUFFER => Some("STORAGE_BUFFER"),
            Self::UNIFORM_BUFFER_DYNAMIC => Some("UNIFORM_BUFFER_DYNAMIC"),
            Self::STORAGE_BUFFER_DYNAMIC => Some("STORAGE_BUFFER_DYNAMIC"),
            Self::INPUT_ATTACHMENT => Some("INPUT_ATTACHMENT"),
            Self::INLINE_UNIFORM_BLOCK_EXT => Some("INLINE_UNIFORM_BLOCK_EXT"),
            Self::ACCELERATION_STRUCTURE_KHR => Some("ACCELERATION_STRUCTURE_KHR"),
            Self::ACCELERATION_STRUCTURE_NV => Some("ACCELERATION_STRUCTURE_NV"),
            Self::MUTABLE_VALVE => Some("MUTABLE_VALVE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for DescriptorUpdateTemplateCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DescriptorUpdateTemplateType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::DESCRIPTOR_SET => Some("DESCRIPTOR_SET"),
            Self::PUSH_DESCRIPTORS_KHR => Some("PUSH_DESCRIPTORS_KHR"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for DeviceCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DeviceDiagnosticsConfigFlagsNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                DeviceDiagnosticsConfigFlagsNV::ENABLE_SHADER_DEBUG_INFO.0,
                "ENABLE_SHADER_DEBUG_INFO",
            ),
            (
                DeviceDiagnosticsConfigFlagsNV::ENABLE_RESOURCE_TRACKING.0,
                "ENABLE_RESOURCE_TRACKING",
            ),
            (
                DeviceDiagnosticsConfigFlagsNV::ENABLE_AUTOMATIC_CHECKPOINTS.0,
                "ENABLE_AUTOMATIC_CHECKPOINTS",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DeviceEventTypeEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::DISPLAY_HOTPLUG => Some("DISPLAY_HOTPLUG"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for DeviceGroupPresentModeFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (DeviceGroupPresentModeFlagsKHR::LOCAL.0, "LOCAL"),
            (DeviceGroupPresentModeFlagsKHR::REMOTE.0, "REMOTE"),
            (DeviceGroupPresentModeFlagsKHR::SUM.0, "SUM"),
            (
                DeviceGroupPresentModeFlagsKHR::LOCAL_MULTI_DEVICE.0,
                "LOCAL_MULTI_DEVICE",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DeviceMemoryReportEventTypeEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::ALLOCATE => Some("ALLOCATE"),
            Self::FREE => Some("FREE"),
            Self::IMPORT => Some("IMPORT"),
            Self::UNIMPORT => Some("UNIMPORT"),
            Self::ALLOCATION_FAILED => Some("ALLOCATION_FAILED"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for DeviceMemoryReportFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DeviceQueueCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(DeviceQueueCreateFlags::PROTECTED.0, "PROTECTED")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DirectFBSurfaceCreateFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DiscardRectangleModeEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::INCLUSIVE => Some("INCLUSIVE"),
            Self::EXCLUSIVE => Some("EXCLUSIVE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for DisplayEventTypeEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::FIRST_PIXEL_OUT => Some("FIRST_PIXEL_OUT"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for DisplayModeCreateFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DisplayPlaneAlphaFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (DisplayPlaneAlphaFlagsKHR::OPAQUE.0, "OPAQUE"),
            (DisplayPlaneAlphaFlagsKHR::GLOBAL.0, "GLOBAL"),
            (DisplayPlaneAlphaFlagsKHR::PER_PIXEL.0, "PER_PIXEL"),
            (
                DisplayPlaneAlphaFlagsKHR::PER_PIXEL_PREMULTIPLIED.0,
                "PER_PIXEL_PREMULTIPLIED",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DisplayPowerStateEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::OFF => Some("OFF"),
            Self::SUSPEND => Some("SUSPEND"),
            Self::ON => Some("ON"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for DisplaySurfaceCreateFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for DriverId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::AMD_PROPRIETARY => Some("AMD_PROPRIETARY"),
            Self::AMD_OPEN_SOURCE => Some("AMD_OPEN_SOURCE"),
            Self::MESA_RADV => Some("MESA_RADV"),
            Self::NVIDIA_PROPRIETARY => Some("NVIDIA_PROPRIETARY"),
            Self::INTEL_PROPRIETARY_WINDOWS => Some("INTEL_PROPRIETARY_WINDOWS"),
            Self::INTEL_OPEN_SOURCE_MESA => Some("INTEL_OPEN_SOURCE_MESA"),
            Self::IMAGINATION_PROPRIETARY => Some("IMAGINATION_PROPRIETARY"),
            Self::QUALCOMM_PROPRIETARY => Some("QUALCOMM_PROPRIETARY"),
            Self::ARM_PROPRIETARY => Some("ARM_PROPRIETARY"),
            Self::GOOGLE_SWIFTSHADER => Some("GOOGLE_SWIFTSHADER"),
            Self::GGP_PROPRIETARY => Some("GGP_PROPRIETARY"),
            Self::BROADCOM_PROPRIETARY => Some("BROADCOM_PROPRIETARY"),
            Self::MESA_LLVMPIPE => Some("MESA_LLVMPIPE"),
            Self::MOLTENVK => Some("MOLTENVK"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for DynamicState {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::VIEWPORT => Some("VIEWPORT"),
            Self::SCISSOR => Some("SCISSOR"),
            Self::LINE_WIDTH => Some("LINE_WIDTH"),
            Self::DEPTH_BIAS => Some("DEPTH_BIAS"),
            Self::BLEND_CONSTANTS => Some("BLEND_CONSTANTS"),
            Self::DEPTH_BOUNDS => Some("DEPTH_BOUNDS"),
            Self::STENCIL_COMPARE_MASK => Some("STENCIL_COMPARE_MASK"),
            Self::STENCIL_WRITE_MASK => Some("STENCIL_WRITE_MASK"),
            Self::STENCIL_REFERENCE => Some("STENCIL_REFERENCE"),
            Self::VIEWPORT_W_SCALING_NV => Some("VIEWPORT_W_SCALING_NV"),
            Self::DISCARD_RECTANGLE_EXT => Some("DISCARD_RECTANGLE_EXT"),
            Self::SAMPLE_LOCATIONS_EXT => Some("SAMPLE_LOCATIONS_EXT"),
            Self::RAY_TRACING_PIPELINE_STACK_SIZE_KHR => {
                Some("RAY_TRACING_PIPELINE_STACK_SIZE_KHR")
            }
            Self::VIEWPORT_SHADING_RATE_PALETTE_NV => Some("VIEWPORT_SHADING_RATE_PALETTE_NV"),
            Self::VIEWPORT_COARSE_SAMPLE_ORDER_NV => Some("VIEWPORT_COARSE_SAMPLE_ORDER_NV"),
            Self::EXCLUSIVE_SCISSOR_NV => Some("EXCLUSIVE_SCISSOR_NV"),
            Self::FRAGMENT_SHADING_RATE_KHR => Some("FRAGMENT_SHADING_RATE_KHR"),
            Self::LINE_STIPPLE_EXT => Some("LINE_STIPPLE_EXT"),
            Self::CULL_MODE_EXT => Some("CULL_MODE_EXT"),
            Self::FRONT_FACE_EXT => Some("FRONT_FACE_EXT"),
            Self::PRIMITIVE_TOPOLOGY_EXT => Some("PRIMITIVE_TOPOLOGY_EXT"),
            Self::VIEWPORT_WITH_COUNT_EXT => Some("VIEWPORT_WITH_COUNT_EXT"),
            Self::SCISSOR_WITH_COUNT_EXT => Some("SCISSOR_WITH_COUNT_EXT"),
            Self::VERTEX_INPUT_BINDING_STRIDE_EXT => Some("VERTEX_INPUT_BINDING_STRIDE_EXT"),
            Self::DEPTH_TEST_ENABLE_EXT => Some("DEPTH_TEST_ENABLE_EXT"),
            Self::DEPTH_WRITE_ENABLE_EXT => Some("DEPTH_WRITE_ENABLE_EXT"),
            Self::DEPTH_COMPARE_OP_EXT => Some("DEPTH_COMPARE_OP_EXT"),
            Self::DEPTH_BOUNDS_TEST_ENABLE_EXT => Some("DEPTH_BOUNDS_TEST_ENABLE_EXT"),
            Self::STENCIL_TEST_ENABLE_EXT => Some("STENCIL_TEST_ENABLE_EXT"),
            Self::STENCIL_OP_EXT => Some("STENCIL_OP_EXT"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for EventCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ExternalFenceFeatureFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ExternalFenceFeatureFlags::EXPORTABLE.0, "EXPORTABLE"),
            (ExternalFenceFeatureFlags::IMPORTABLE.0, "IMPORTABLE"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ExternalFenceHandleTypeFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ExternalFenceHandleTypeFlags::OPAQUE_FD.0, "OPAQUE_FD"),
            (ExternalFenceHandleTypeFlags::OPAQUE_WIN32.0, "OPAQUE_WIN32"),
            (
                ExternalFenceHandleTypeFlags::OPAQUE_WIN32_KMT.0,
                "OPAQUE_WIN32_KMT",
            ),
            (ExternalFenceHandleTypeFlags::SYNC_FD.0, "SYNC_FD"),
            (
                ExternalFenceHandleTypeFlags::RESERVED_4_NV.0,
                "RESERVED_4_NV",
            ),
            (
                ExternalFenceHandleTypeFlags::RESERVED_5_NV.0,
                "RESERVED_5_NV",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ExternalMemoryFeatureFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                ExternalMemoryFeatureFlags::DEDICATED_ONLY.0,
                "DEDICATED_ONLY",
            ),
            (ExternalMemoryFeatureFlags::EXPORTABLE.0, "EXPORTABLE"),
            (ExternalMemoryFeatureFlags::IMPORTABLE.0, "IMPORTABLE"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ExternalMemoryFeatureFlagsNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                ExternalMemoryFeatureFlagsNV::DEDICATED_ONLY.0,
                "DEDICATED_ONLY",
            ),
            (ExternalMemoryFeatureFlagsNV::EXPORTABLE.0, "EXPORTABLE"),
            (ExternalMemoryFeatureFlagsNV::IMPORTABLE.0, "IMPORTABLE"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ExternalMemoryHandleTypeFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ExternalMemoryHandleTypeFlags::OPAQUE_FD.0, "OPAQUE_FD"),
            (
                ExternalMemoryHandleTypeFlags::OPAQUE_WIN32.0,
                "OPAQUE_WIN32",
            ),
            (
                ExternalMemoryHandleTypeFlags::OPAQUE_WIN32_KMT.0,
                "OPAQUE_WIN32_KMT",
            ),
            (
                ExternalMemoryHandleTypeFlags::D3D11_TEXTURE.0,
                "D3D11_TEXTURE",
            ),
            (
                ExternalMemoryHandleTypeFlags::D3D11_TEXTURE_KMT.0,
                "D3D11_TEXTURE_KMT",
            ),
            (ExternalMemoryHandleTypeFlags::D3D12_HEAP.0, "D3D12_HEAP"),
            (
                ExternalMemoryHandleTypeFlags::D3D12_RESOURCE.0,
                "D3D12_RESOURCE",
            ),
            (ExternalMemoryHandleTypeFlags::DMA_BUF_EXT.0, "DMA_BUF_EXT"),
            (
                ExternalMemoryHandleTypeFlags::ANDROID_HARDWARE_BUFFER_ANDROID.0,
                "ANDROID_HARDWARE_BUFFER_ANDROID",
            ),
            (
                ExternalMemoryHandleTypeFlags::HOST_ALLOCATION_EXT.0,
                "HOST_ALLOCATION_EXT",
            ),
            (
                ExternalMemoryHandleTypeFlags::HOST_MAPPED_FOREIGN_MEMORY_EXT.0,
                "HOST_MAPPED_FOREIGN_MEMORY_EXT",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ExternalMemoryHandleTypeFlagsNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                ExternalMemoryHandleTypeFlagsNV::OPAQUE_WIN32.0,
                "OPAQUE_WIN32",
            ),
            (
                ExternalMemoryHandleTypeFlagsNV::OPAQUE_WIN32_KMT.0,
                "OPAQUE_WIN32_KMT",
            ),
            (
                ExternalMemoryHandleTypeFlagsNV::D3D11_IMAGE.0,
                "D3D11_IMAGE",
            ),
            (
                ExternalMemoryHandleTypeFlagsNV::D3D11_IMAGE_KMT.0,
                "D3D11_IMAGE_KMT",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ExternalSemaphoreFeatureFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ExternalSemaphoreFeatureFlags::EXPORTABLE.0, "EXPORTABLE"),
            (ExternalSemaphoreFeatureFlags::IMPORTABLE.0, "IMPORTABLE"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ExternalSemaphoreHandleTypeFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ExternalSemaphoreHandleTypeFlags::OPAQUE_FD.0, "OPAQUE_FD"),
            (
                ExternalSemaphoreHandleTypeFlags::OPAQUE_WIN32.0,
                "OPAQUE_WIN32",
            ),
            (
                ExternalSemaphoreHandleTypeFlags::OPAQUE_WIN32_KMT.0,
                "OPAQUE_WIN32_KMT",
            ),
            (
                ExternalSemaphoreHandleTypeFlags::D3D12_FENCE.0,
                "D3D12_FENCE",
            ),
            (ExternalSemaphoreHandleTypeFlags::SYNC_FD.0, "SYNC_FD"),
            (
                ExternalSemaphoreHandleTypeFlags::RESERVED_5_NV.0,
                "RESERVED_5_NV",
            ),
            (
                ExternalSemaphoreHandleTypeFlags::RESERVED_6_NV.0,
                "RESERVED_6_NV",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for FenceCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(FenceCreateFlags::SIGNALED.0, "SIGNALED")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for FenceImportFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(FenceImportFlags::TEMPORARY.0, "TEMPORARY")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for Filter {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::NEAREST => Some("NEAREST"),
            Self::LINEAR => Some("LINEAR"),
            Self::CUBIC_IMG => Some("CUBIC_IMG"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for Format {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::UNDEFINED => Some("UNDEFINED"),
            Self::R4G4_UNORM_PACK8 => Some("R4G4_UNORM_PACK8"),
            Self::R4G4B4A4_UNORM_PACK16 => Some("R4G4B4A4_UNORM_PACK16"),
            Self::B4G4R4A4_UNORM_PACK16 => Some("B4G4R4A4_UNORM_PACK16"),
            Self::R5G6B5_UNORM_PACK16 => Some("R5G6B5_UNORM_PACK16"),
            Self::B5G6R5_UNORM_PACK16 => Some("B5G6R5_UNORM_PACK16"),
            Self::R5G5B5A1_UNORM_PACK16 => Some("R5G5B5A1_UNORM_PACK16"),
            Self::B5G5R5A1_UNORM_PACK16 => Some("B5G5R5A1_UNORM_PACK16"),
            Self::A1R5G5B5_UNORM_PACK16 => Some("A1R5G5B5_UNORM_PACK16"),
            Self::R8_UNORM => Some("R8_UNORM"),
            Self::R8_SNORM => Some("R8_SNORM"),
            Self::R8_USCALED => Some("R8_USCALED"),
            Self::R8_SSCALED => Some("R8_SSCALED"),
            Self::R8_UINT => Some("R8_UINT"),
            Self::R8_SINT => Some("R8_SINT"),
            Self::R8_SRGB => Some("R8_SRGB"),
            Self::R8G8_UNORM => Some("R8G8_UNORM"),
            Self::R8G8_SNORM => Some("R8G8_SNORM"),
            Self::R8G8_USCALED => Some("R8G8_USCALED"),
            Self::R8G8_SSCALED => Some("R8G8_SSCALED"),
            Self::R8G8_UINT => Some("R8G8_UINT"),
            Self::R8G8_SINT => Some("R8G8_SINT"),
            Self::R8G8_SRGB => Some("R8G8_SRGB"),
            Self::R8G8B8_UNORM => Some("R8G8B8_UNORM"),
            Self::R8G8B8_SNORM => Some("R8G8B8_SNORM"),
            Self::R8G8B8_USCALED => Some("R8G8B8_USCALED"),
            Self::R8G8B8_SSCALED => Some("R8G8B8_SSCALED"),
            Self::R8G8B8_UINT => Some("R8G8B8_UINT"),
            Self::R8G8B8_SINT => Some("R8G8B8_SINT"),
            Self::R8G8B8_SRGB => Some("R8G8B8_SRGB"),
            Self::B8G8R8_UNORM => Some("B8G8R8_UNORM"),
            Self::B8G8R8_SNORM => Some("B8G8R8_SNORM"),
            Self::B8G8R8_USCALED => Some("B8G8R8_USCALED"),
            Self::B8G8R8_SSCALED => Some("B8G8R8_SSCALED"),
            Self::B8G8R8_UINT => Some("B8G8R8_UINT"),
            Self::B8G8R8_SINT => Some("B8G8R8_SINT"),
            Self::B8G8R8_SRGB => Some("B8G8R8_SRGB"),
            Self::R8G8B8A8_UNORM => Some("R8G8B8A8_UNORM"),
            Self::R8G8B8A8_SNORM => Some("R8G8B8A8_SNORM"),
            Self::R8G8B8A8_USCALED => Some("R8G8B8A8_USCALED"),
            Self::R8G8B8A8_SSCALED => Some("R8G8B8A8_SSCALED"),
            Self::R8G8B8A8_UINT => Some("R8G8B8A8_UINT"),
            Self::R8G8B8A8_SINT => Some("R8G8B8A8_SINT"),
            Self::R8G8B8A8_SRGB => Some("R8G8B8A8_SRGB"),
            Self::B8G8R8A8_UNORM => Some("B8G8R8A8_UNORM"),
            Self::B8G8R8A8_SNORM => Some("B8G8R8A8_SNORM"),
            Self::B8G8R8A8_USCALED => Some("B8G8R8A8_USCALED"),
            Self::B8G8R8A8_SSCALED => Some("B8G8R8A8_SSCALED"),
            Self::B8G8R8A8_UINT => Some("B8G8R8A8_UINT"),
            Self::B8G8R8A8_SINT => Some("B8G8R8A8_SINT"),
            Self::B8G8R8A8_SRGB => Some("B8G8R8A8_SRGB"),
            Self::A8B8G8R8_UNORM_PACK32 => Some("A8B8G8R8_UNORM_PACK32"),
            Self::A8B8G8R8_SNORM_PACK32 => Some("A8B8G8R8_SNORM_PACK32"),
            Self::A8B8G8R8_USCALED_PACK32 => Some("A8B8G8R8_USCALED_PACK32"),
            Self::A8B8G8R8_SSCALED_PACK32 => Some("A8B8G8R8_SSCALED_PACK32"),
            Self::A8B8G8R8_UINT_PACK32 => Some("A8B8G8R8_UINT_PACK32"),
            Self::A8B8G8R8_SINT_PACK32 => Some("A8B8G8R8_SINT_PACK32"),
            Self::A8B8G8R8_SRGB_PACK32 => Some("A8B8G8R8_SRGB_PACK32"),
            Self::A2R10G10B10_UNORM_PACK32 => Some("A2R10G10B10_UNORM_PACK32"),
            Self::A2R10G10B10_SNORM_PACK32 => Some("A2R10G10B10_SNORM_PACK32"),
            Self::A2R10G10B10_USCALED_PACK32 => Some("A2R10G10B10_USCALED_PACK32"),
            Self::A2R10G10B10_SSCALED_PACK32 => Some("A2R10G10B10_SSCALED_PACK32"),
            Self::A2R10G10B10_UINT_PACK32 => Some("A2R10G10B10_UINT_PACK32"),
            Self::A2R10G10B10_SINT_PACK32 => Some("A2R10G10B10_SINT_PACK32"),
            Self::A2B10G10R10_UNORM_PACK32 => Some("A2B10G10R10_UNORM_PACK32"),
            Self::A2B10G10R10_SNORM_PACK32 => Some("A2B10G10R10_SNORM_PACK32"),
            Self::A2B10G10R10_USCALED_PACK32 => Some("A2B10G10R10_USCALED_PACK32"),
            Self::A2B10G10R10_SSCALED_PACK32 => Some("A2B10G10R10_SSCALED_PACK32"),
            Self::A2B10G10R10_UINT_PACK32 => Some("A2B10G10R10_UINT_PACK32"),
            Self::A2B10G10R10_SINT_PACK32 => Some("A2B10G10R10_SINT_PACK32"),
            Self::R16_UNORM => Some("R16_UNORM"),
            Self::R16_SNORM => Some("R16_SNORM"),
            Self::R16_USCALED => Some("R16_USCALED"),
            Self::R16_SSCALED => Some("R16_SSCALED"),
            Self::R16_UINT => Some("R16_UINT"),
            Self::R16_SINT => Some("R16_SINT"),
            Self::R16_SFLOAT => Some("R16_SFLOAT"),
            Self::R16G16_UNORM => Some("R16G16_UNORM"),
            Self::R16G16_SNORM => Some("R16G16_SNORM"),
            Self::R16G16_USCALED => Some("R16G16_USCALED"),
            Self::R16G16_SSCALED => Some("R16G16_SSCALED"),
            Self::R16G16_UINT => Some("R16G16_UINT"),
            Self::R16G16_SINT => Some("R16G16_SINT"),
            Self::R16G16_SFLOAT => Some("R16G16_SFLOAT"),
            Self::R16G16B16_UNORM => Some("R16G16B16_UNORM"),
            Self::R16G16B16_SNORM => Some("R16G16B16_SNORM"),
            Self::R16G16B16_USCALED => Some("R16G16B16_USCALED"),
            Self::R16G16B16_SSCALED => Some("R16G16B16_SSCALED"),
            Self::R16G16B16_UINT => Some("R16G16B16_UINT"),
            Self::R16G16B16_SINT => Some("R16G16B16_SINT"),
            Self::R16G16B16_SFLOAT => Some("R16G16B16_SFLOAT"),
            Self::R16G16B16A16_UNORM => Some("R16G16B16A16_UNORM"),
            Self::R16G16B16A16_SNORM => Some("R16G16B16A16_SNORM"),
            Self::R16G16B16A16_USCALED => Some("R16G16B16A16_USCALED"),
            Self::R16G16B16A16_SSCALED => Some("R16G16B16A16_SSCALED"),
            Self::R16G16B16A16_UINT => Some("R16G16B16A16_UINT"),
            Self::R16G16B16A16_SINT => Some("R16G16B16A16_SINT"),
            Self::R16G16B16A16_SFLOAT => Some("R16G16B16A16_SFLOAT"),
            Self::R32_UINT => Some("R32_UINT"),
            Self::R32_SINT => Some("R32_SINT"),
            Self::R32_SFLOAT => Some("R32_SFLOAT"),
            Self::R32G32_UINT => Some("R32G32_UINT"),
            Self::R32G32_SINT => Some("R32G32_SINT"),
            Self::R32G32_SFLOAT => Some("R32G32_SFLOAT"),
            Self::R32G32B32_UINT => Some("R32G32B32_UINT"),
            Self::R32G32B32_SINT => Some("R32G32B32_SINT"),
            Self::R32G32B32_SFLOAT => Some("R32G32B32_SFLOAT"),
            Self::R32G32B32A32_UINT => Some("R32G32B32A32_UINT"),
            Self::R32G32B32A32_SINT => Some("R32G32B32A32_SINT"),
            Self::R32G32B32A32_SFLOAT => Some("R32G32B32A32_SFLOAT"),
            Self::R64_UINT => Some("R64_UINT"),
            Self::R64_SINT => Some("R64_SINT"),
            Self::R64_SFLOAT => Some("R64_SFLOAT"),
            Self::R64G64_UINT => Some("R64G64_UINT"),
            Self::R64G64_SINT => Some("R64G64_SINT"),
            Self::R64G64_SFLOAT => Some("R64G64_SFLOAT"),
            Self::R64G64B64_UINT => Some("R64G64B64_UINT"),
            Self::R64G64B64_SINT => Some("R64G64B64_SINT"),
            Self::R64G64B64_SFLOAT => Some("R64G64B64_SFLOAT"),
            Self::R64G64B64A64_UINT => Some("R64G64B64A64_UINT"),
            Self::R64G64B64A64_SINT => Some("R64G64B64A64_SINT"),
            Self::R64G64B64A64_SFLOAT => Some("R64G64B64A64_SFLOAT"),
            Self::B10G11R11_UFLOAT_PACK32 => Some("B10G11R11_UFLOAT_PACK32"),
            Self::E5B9G9R9_UFLOAT_PACK32 => Some("E5B9G9R9_UFLOAT_PACK32"),
            Self::D16_UNORM => Some("D16_UNORM"),
            Self::X8_D24_UNORM_PACK32 => Some("X8_D24_UNORM_PACK32"),
            Self::D32_SFLOAT => Some("D32_SFLOAT"),
            Self::S8_UINT => Some("S8_UINT"),
            Self::D16_UNORM_S8_UINT => Some("D16_UNORM_S8_UINT"),
            Self::D24_UNORM_S8_UINT => Some("D24_UNORM_S8_UINT"),
            Self::D32_SFLOAT_S8_UINT => Some("D32_SFLOAT_S8_UINT"),
            Self::BC1_RGB_UNORM_BLOCK => Some("BC1_RGB_UNORM_BLOCK"),
            Self::BC1_RGB_SRGB_BLOCK => Some("BC1_RGB_SRGB_BLOCK"),
            Self::BC1_RGBA_UNORM_BLOCK => Some("BC1_RGBA_UNORM_BLOCK"),
            Self::BC1_RGBA_SRGB_BLOCK => Some("BC1_RGBA_SRGB_BLOCK"),
            Self::BC2_UNORM_BLOCK => Some("BC2_UNORM_BLOCK"),
            Self::BC2_SRGB_BLOCK => Some("BC2_SRGB_BLOCK"),
            Self::BC3_UNORM_BLOCK => Some("BC3_UNORM_BLOCK"),
            Self::BC3_SRGB_BLOCK => Some("BC3_SRGB_BLOCK"),
            Self::BC4_UNORM_BLOCK => Some("BC4_UNORM_BLOCK"),
            Self::BC4_SNORM_BLOCK => Some("BC4_SNORM_BLOCK"),
            Self::BC5_UNORM_BLOCK => Some("BC5_UNORM_BLOCK"),
            Self::BC5_SNORM_BLOCK => Some("BC5_SNORM_BLOCK"),
            Self::BC6H_UFLOAT_BLOCK => Some("BC6H_UFLOAT_BLOCK"),
            Self::BC6H_SFLOAT_BLOCK => Some("BC6H_SFLOAT_BLOCK"),
            Self::BC7_UNORM_BLOCK => Some("BC7_UNORM_BLOCK"),
            Self::BC7_SRGB_BLOCK => Some("BC7_SRGB_BLOCK"),
            Self::ETC2_R8G8B8_UNORM_BLOCK => Some("ETC2_R8G8B8_UNORM_BLOCK"),
            Self::ETC2_R8G8B8_SRGB_BLOCK => Some("ETC2_R8G8B8_SRGB_BLOCK"),
            Self::ETC2_R8G8B8A1_UNORM_BLOCK => Some("ETC2_R8G8B8A1_UNORM_BLOCK"),
            Self::ETC2_R8G8B8A1_SRGB_BLOCK => Some("ETC2_R8G8B8A1_SRGB_BLOCK"),
            Self::ETC2_R8G8B8A8_UNORM_BLOCK => Some("ETC2_R8G8B8A8_UNORM_BLOCK"),
            Self::ETC2_R8G8B8A8_SRGB_BLOCK => Some("ETC2_R8G8B8A8_SRGB_BLOCK"),
            Self::EAC_R11_UNORM_BLOCK => Some("EAC_R11_UNORM_BLOCK"),
            Self::EAC_R11_SNORM_BLOCK => Some("EAC_R11_SNORM_BLOCK"),
            Self::EAC_R11G11_UNORM_BLOCK => Some("EAC_R11G11_UNORM_BLOCK"),
            Self::EAC_R11G11_SNORM_BLOCK => Some("EAC_R11G11_SNORM_BLOCK"),
            Self::ASTC_4X4_UNORM_BLOCK => Some("ASTC_4X4_UNORM_BLOCK"),
            Self::ASTC_4X4_SRGB_BLOCK => Some("ASTC_4X4_SRGB_BLOCK"),
            Self::ASTC_5X4_UNORM_BLOCK => Some("ASTC_5X4_UNORM_BLOCK"),
            Self::ASTC_5X4_SRGB_BLOCK => Some("ASTC_5X4_SRGB_BLOCK"),
            Self::ASTC_5X5_UNORM_BLOCK => Some("ASTC_5X5_UNORM_BLOCK"),
            Self::ASTC_5X5_SRGB_BLOCK => Some("ASTC_5X5_SRGB_BLOCK"),
            Self::ASTC_6X5_UNORM_BLOCK => Some("ASTC_6X5_UNORM_BLOCK"),
            Self::ASTC_6X5_SRGB_BLOCK => Some("ASTC_6X5_SRGB_BLOCK"),
            Self::ASTC_6X6_UNORM_BLOCK => Some("ASTC_6X6_UNORM_BLOCK"),
            Self::ASTC_6X6_SRGB_BLOCK => Some("ASTC_6X6_SRGB_BLOCK"),
            Self::ASTC_8X5_UNORM_BLOCK => Some("ASTC_8X5_UNORM_BLOCK"),
            Self::ASTC_8X5_SRGB_BLOCK => Some("ASTC_8X5_SRGB_BLOCK"),
            Self::ASTC_8X6_UNORM_BLOCK => Some("ASTC_8X6_UNORM_BLOCK"),
            Self::ASTC_8X6_SRGB_BLOCK => Some("ASTC_8X6_SRGB_BLOCK"),
            Self::ASTC_8X8_UNORM_BLOCK => Some("ASTC_8X8_UNORM_BLOCK"),
            Self::ASTC_8X8_SRGB_BLOCK => Some("ASTC_8X8_SRGB_BLOCK"),
            Self::ASTC_10X5_UNORM_BLOCK => Some("ASTC_10X5_UNORM_BLOCK"),
            Self::ASTC_10X5_SRGB_BLOCK => Some("ASTC_10X5_SRGB_BLOCK"),
            Self::ASTC_10X6_UNORM_BLOCK => Some("ASTC_10X6_UNORM_BLOCK"),
            Self::ASTC_10X6_SRGB_BLOCK => Some("ASTC_10X6_SRGB_BLOCK"),
            Self::ASTC_10X8_UNORM_BLOCK => Some("ASTC_10X8_UNORM_BLOCK"),
            Self::ASTC_10X8_SRGB_BLOCK => Some("ASTC_10X8_SRGB_BLOCK"),
            Self::ASTC_10X10_UNORM_BLOCK => Some("ASTC_10X10_UNORM_BLOCK"),
            Self::ASTC_10X10_SRGB_BLOCK => Some("ASTC_10X10_SRGB_BLOCK"),
            Self::ASTC_12X10_UNORM_BLOCK => Some("ASTC_12X10_UNORM_BLOCK"),
            Self::ASTC_12X10_SRGB_BLOCK => Some("ASTC_12X10_SRGB_BLOCK"),
            Self::ASTC_12X12_UNORM_BLOCK => Some("ASTC_12X12_UNORM_BLOCK"),
            Self::ASTC_12X12_SRGB_BLOCK => Some("ASTC_12X12_SRGB_BLOCK"),
            Self::PVRTC1_2BPP_UNORM_BLOCK_IMG => Some("PVRTC1_2BPP_UNORM_BLOCK_IMG"),
            Self::PVRTC1_4BPP_UNORM_BLOCK_IMG => Some("PVRTC1_4BPP_UNORM_BLOCK_IMG"),
            Self::PVRTC2_2BPP_UNORM_BLOCK_IMG => Some("PVRTC2_2BPP_UNORM_BLOCK_IMG"),
            Self::PVRTC2_4BPP_UNORM_BLOCK_IMG => Some("PVRTC2_4BPP_UNORM_BLOCK_IMG"),
            Self::PVRTC1_2BPP_SRGB_BLOCK_IMG => Some("PVRTC1_2BPP_SRGB_BLOCK_IMG"),
            Self::PVRTC1_4BPP_SRGB_BLOCK_IMG => Some("PVRTC1_4BPP_SRGB_BLOCK_IMG"),
            Self::PVRTC2_2BPP_SRGB_BLOCK_IMG => Some("PVRTC2_2BPP_SRGB_BLOCK_IMG"),
            Self::PVRTC2_4BPP_SRGB_BLOCK_IMG => Some("PVRTC2_4BPP_SRGB_BLOCK_IMG"),
            Self::ASTC_4X4_SFLOAT_BLOCK_EXT => Some("ASTC_4X4_SFLOAT_BLOCK_EXT"),
            Self::ASTC_5X4_SFLOAT_BLOCK_EXT => Some("ASTC_5X4_SFLOAT_BLOCK_EXT"),
            Self::ASTC_5X5_SFLOAT_BLOCK_EXT => Some("ASTC_5X5_SFLOAT_BLOCK_EXT"),
            Self::ASTC_6X5_SFLOAT_BLOCK_EXT => Some("ASTC_6X5_SFLOAT_BLOCK_EXT"),
            Self::ASTC_6X6_SFLOAT_BLOCK_EXT => Some("ASTC_6X6_SFLOAT_BLOCK_EXT"),
            Self::ASTC_8X5_SFLOAT_BLOCK_EXT => Some("ASTC_8X5_SFLOAT_BLOCK_EXT"),
            Self::ASTC_8X6_SFLOAT_BLOCK_EXT => Some("ASTC_8X6_SFLOAT_BLOCK_EXT"),
            Self::ASTC_8X8_SFLOAT_BLOCK_EXT => Some("ASTC_8X8_SFLOAT_BLOCK_EXT"),
            Self::ASTC_10X5_SFLOAT_BLOCK_EXT => Some("ASTC_10X5_SFLOAT_BLOCK_EXT"),
            Self::ASTC_10X6_SFLOAT_BLOCK_EXT => Some("ASTC_10X6_SFLOAT_BLOCK_EXT"),
            Self::ASTC_10X8_SFLOAT_BLOCK_EXT => Some("ASTC_10X8_SFLOAT_BLOCK_EXT"),
            Self::ASTC_10X10_SFLOAT_BLOCK_EXT => Some("ASTC_10X10_SFLOAT_BLOCK_EXT"),
            Self::ASTC_12X10_SFLOAT_BLOCK_EXT => Some("ASTC_12X10_SFLOAT_BLOCK_EXT"),
            Self::ASTC_12X12_SFLOAT_BLOCK_EXT => Some("ASTC_12X12_SFLOAT_BLOCK_EXT"),
            Self::ASTC_3X3X3_UNORM_BLOCK_EXT => Some("ASTC_3X3X3_UNORM_BLOCK_EXT"),
            Self::ASTC_3X3X3_SRGB_BLOCK_EXT => Some("ASTC_3X3X3_SRGB_BLOCK_EXT"),
            Self::ASTC_3X3X3_SFLOAT_BLOCK_EXT => Some("ASTC_3X3X3_SFLOAT_BLOCK_EXT"),
            Self::ASTC_4X3X3_UNORM_BLOCK_EXT => Some("ASTC_4X3X3_UNORM_BLOCK_EXT"),
            Self::ASTC_4X3X3_SRGB_BLOCK_EXT => Some("ASTC_4X3X3_SRGB_BLOCK_EXT"),
            Self::ASTC_4X3X3_SFLOAT_BLOCK_EXT => Some("ASTC_4X3X3_SFLOAT_BLOCK_EXT"),
            Self::ASTC_4X4X3_UNORM_BLOCK_EXT => Some("ASTC_4X4X3_UNORM_BLOCK_EXT"),
            Self::ASTC_4X4X3_SRGB_BLOCK_EXT => Some("ASTC_4X4X3_SRGB_BLOCK_EXT"),
            Self::ASTC_4X4X3_SFLOAT_BLOCK_EXT => Some("ASTC_4X4X3_SFLOAT_BLOCK_EXT"),
            Self::ASTC_4X4X4_UNORM_BLOCK_EXT => Some("ASTC_4X4X4_UNORM_BLOCK_EXT"),
            Self::ASTC_4X4X4_SRGB_BLOCK_EXT => Some("ASTC_4X4X4_SRGB_BLOCK_EXT"),
            Self::ASTC_4X4X4_SFLOAT_BLOCK_EXT => Some("ASTC_4X4X4_SFLOAT_BLOCK_EXT"),
            Self::ASTC_5X4X4_UNORM_BLOCK_EXT => Some("ASTC_5X4X4_UNORM_BLOCK_EXT"),
            Self::ASTC_5X4X4_SRGB_BLOCK_EXT => Some("ASTC_5X4X4_SRGB_BLOCK_EXT"),
            Self::ASTC_5X4X4_SFLOAT_BLOCK_EXT => Some("ASTC_5X4X4_SFLOAT_BLOCK_EXT"),
            Self::ASTC_5X5X4_UNORM_BLOCK_EXT => Some("ASTC_5X5X4_UNORM_BLOCK_EXT"),
            Self::ASTC_5X5X4_SRGB_BLOCK_EXT => Some("ASTC_5X5X4_SRGB_BLOCK_EXT"),
            Self::ASTC_5X5X4_SFLOAT_BLOCK_EXT => Some("ASTC_5X5X4_SFLOAT_BLOCK_EXT"),
            Self::ASTC_5X5X5_UNORM_BLOCK_EXT => Some("ASTC_5X5X5_UNORM_BLOCK_EXT"),
            Self::ASTC_5X5X5_SRGB_BLOCK_EXT => Some("ASTC_5X5X5_SRGB_BLOCK_EXT"),
            Self::ASTC_5X5X5_SFLOAT_BLOCK_EXT => Some("ASTC_5X5X5_SFLOAT_BLOCK_EXT"),
            Self::ASTC_6X5X5_UNORM_BLOCK_EXT => Some("ASTC_6X5X5_UNORM_BLOCK_EXT"),
            Self::ASTC_6X5X5_SRGB_BLOCK_EXT => Some("ASTC_6X5X5_SRGB_BLOCK_EXT"),
            Self::ASTC_6X5X5_SFLOAT_BLOCK_EXT => Some("ASTC_6X5X5_SFLOAT_BLOCK_EXT"),
            Self::ASTC_6X6X5_UNORM_BLOCK_EXT => Some("ASTC_6X6X5_UNORM_BLOCK_EXT"),
            Self::ASTC_6X6X5_SRGB_BLOCK_EXT => Some("ASTC_6X6X5_SRGB_BLOCK_EXT"),
            Self::ASTC_6X6X5_SFLOAT_BLOCK_EXT => Some("ASTC_6X6X5_SFLOAT_BLOCK_EXT"),
            Self::ASTC_6X6X6_UNORM_BLOCK_EXT => Some("ASTC_6X6X6_UNORM_BLOCK_EXT"),
            Self::ASTC_6X6X6_SRGB_BLOCK_EXT => Some("ASTC_6X6X6_SRGB_BLOCK_EXT"),
            Self::ASTC_6X6X6_SFLOAT_BLOCK_EXT => Some("ASTC_6X6X6_SFLOAT_BLOCK_EXT"),
            Self::A4R4G4B4_UNORM_PACK16_EXT => Some("A4R4G4B4_UNORM_PACK16_EXT"),
            Self::A4B4G4R4_UNORM_PACK16_EXT => Some("A4B4G4R4_UNORM_PACK16_EXT"),
            Self::G8B8G8R8_422_UNORM => Some("G8B8G8R8_422_UNORM"),
            Self::B8G8R8G8_422_UNORM => Some("B8G8R8G8_422_UNORM"),
            Self::G8_B8_R8_3PLANE_420_UNORM => Some("G8_B8_R8_3PLANE_420_UNORM"),
            Self::G8_B8R8_2PLANE_420_UNORM => Some("G8_B8R8_2PLANE_420_UNORM"),
            Self::G8_B8_R8_3PLANE_422_UNORM => Some("G8_B8_R8_3PLANE_422_UNORM"),
            Self::G8_B8R8_2PLANE_422_UNORM => Some("G8_B8R8_2PLANE_422_UNORM"),
            Self::G8_B8_R8_3PLANE_444_UNORM => Some("G8_B8_R8_3PLANE_444_UNORM"),
            Self::R10X6_UNORM_PACK16 => Some("R10X6_UNORM_PACK16"),
            Self::R10X6G10X6_UNORM_2PACK16 => Some("R10X6G10X6_UNORM_2PACK16"),
            Self::R10X6G10X6B10X6A10X6_UNORM_4PACK16 => Some("R10X6G10X6B10X6A10X6_UNORM_4PACK16"),
            Self::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 => {
                Some("G10X6B10X6G10X6R10X6_422_UNORM_4PACK16")
            }
            Self::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 => {
                Some("B10X6G10X6R10X6G10X6_422_UNORM_4PACK16")
            }
            Self::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 => {
                Some("G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16")
            }
            Self::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 => {
                Some("G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16")
            }
            Self::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 => {
                Some("G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16")
            }
            Self::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 => {
                Some("G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16")
            }
            Self::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 => {
                Some("G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16")
            }
            Self::R12X4_UNORM_PACK16 => Some("R12X4_UNORM_PACK16"),
            Self::R12X4G12X4_UNORM_2PACK16 => Some("R12X4G12X4_UNORM_2PACK16"),
            Self::R12X4G12X4B12X4A12X4_UNORM_4PACK16 => Some("R12X4G12X4B12X4A12X4_UNORM_4PACK16"),
            Self::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 => {
                Some("G12X4B12X4G12X4R12X4_422_UNORM_4PACK16")
            }
            Self::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 => {
                Some("B12X4G12X4R12X4G12X4_422_UNORM_4PACK16")
            }
            Self::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 => {
                Some("G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16")
            }
            Self::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 => {
                Some("G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16")
            }
            Self::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 => {
                Some("G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16")
            }
            Self::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 => {
                Some("G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16")
            }
            Self::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 => {
                Some("G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16")
            }
            Self::G16B16G16R16_422_UNORM => Some("G16B16G16R16_422_UNORM"),
            Self::B16G16R16G16_422_UNORM => Some("B16G16R16G16_422_UNORM"),
            Self::G16_B16_R16_3PLANE_420_UNORM => Some("G16_B16_R16_3PLANE_420_UNORM"),
            Self::G16_B16R16_2PLANE_420_UNORM => Some("G16_B16R16_2PLANE_420_UNORM"),
            Self::G16_B16_R16_3PLANE_422_UNORM => Some("G16_B16_R16_3PLANE_422_UNORM"),
            Self::G16_B16R16_2PLANE_422_UNORM => Some("G16_B16R16_2PLANE_422_UNORM"),
            Self::G16_B16_R16_3PLANE_444_UNORM => Some("G16_B16_R16_3PLANE_444_UNORM"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for FormatFeatureFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN : & [(Flags , & str)] = & [(FormatFeatureFlags :: SAMPLED_IMAGE . 0 , "SAMPLED_IMAGE") , (FormatFeatureFlags :: STORAGE_IMAGE . 0 , "STORAGE_IMAGE") , (FormatFeatureFlags :: STORAGE_IMAGE_ATOMIC . 0 , "STORAGE_IMAGE_ATOMIC") , (FormatFeatureFlags :: UNIFORM_TEXEL_BUFFER . 0 , "UNIFORM_TEXEL_BUFFER") , (FormatFeatureFlags :: STORAGE_TEXEL_BUFFER . 0 , "STORAGE_TEXEL_BUFFER") , (FormatFeatureFlags :: STORAGE_TEXEL_BUFFER_ATOMIC . 0 , "STORAGE_TEXEL_BUFFER_ATOMIC") , (FormatFeatureFlags :: VERTEX_BUFFER . 0 , "VERTEX_BUFFER") , (FormatFeatureFlags :: COLOR_ATTACHMENT . 0 , "COLOR_ATTACHMENT") , (FormatFeatureFlags :: COLOR_ATTACHMENT_BLEND . 0 , "COLOR_ATTACHMENT_BLEND") , (FormatFeatureFlags :: DEPTH_STENCIL_ATTACHMENT . 0 , "DEPTH_STENCIL_ATTACHMENT") , (FormatFeatureFlags :: BLIT_SRC . 0 , "BLIT_SRC") , (FormatFeatureFlags :: BLIT_DST . 0 , "BLIT_DST") , (FormatFeatureFlags :: SAMPLED_IMAGE_FILTER_LINEAR . 0 , "SAMPLED_IMAGE_FILTER_LINEAR") , (FormatFeatureFlags :: SAMPLED_IMAGE_FILTER_CUBIC_IMG . 0 , "SAMPLED_IMAGE_FILTER_CUBIC_IMG") , (FormatFeatureFlags :: RESERVED_27_KHR . 0 , "RESERVED_27_KHR") , (FormatFeatureFlags :: RESERVED_28_KHR . 0 , "RESERVED_28_KHR") , (FormatFeatureFlags :: RESERVED_25_KHR . 0 , "RESERVED_25_KHR") , (FormatFeatureFlags :: RESERVED_26_KHR . 0 , "RESERVED_26_KHR") , (FormatFeatureFlags :: ACCELERATION_STRUCTURE_VERTEX_BUFFER_KHR . 0 , "ACCELERATION_STRUCTURE_VERTEX_BUFFER_KHR") , (FormatFeatureFlags :: FRAGMENT_DENSITY_MAP_EXT . 0 , "FRAGMENT_DENSITY_MAP_EXT") , (FormatFeatureFlags :: FRAGMENT_SHADING_RATE_ATTACHMENT_KHR . 0 , "FRAGMENT_SHADING_RATE_ATTACHMENT_KHR") , (FormatFeatureFlags :: TRANSFER_SRC . 0 , "TRANSFER_SRC") , (FormatFeatureFlags :: TRANSFER_DST . 0 , "TRANSFER_DST") , (FormatFeatureFlags :: MIDPOINT_CHROMA_SAMPLES . 0 , "MIDPOINT_CHROMA_SAMPLES") , (FormatFeatureFlags :: SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER . 0 , "SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER") , (FormatFeatureFlags :: SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER . 0 , "SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER") , (FormatFeatureFlags :: SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT . 0 , "SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT") , (FormatFeatureFlags :: SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE . 0 , "SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE") , (FormatFeatureFlags :: DISJOINT . 0 , "DISJOINT") , (FormatFeatureFlags :: COSITED_CHROMA_SAMPLES . 0 , "COSITED_CHROMA_SAMPLES") , (FormatFeatureFlags :: SAMPLED_IMAGE_FILTER_MINMAX . 0 , "SAMPLED_IMAGE_FILTER_MINMAX")] ;
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for FragmentShadingRateCombinerOpKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::KEEP => Some("KEEP"),
            Self::REPLACE => Some("REPLACE"),
            Self::MIN => Some("MIN"),
            Self::MAX => Some("MAX"),
            Self::MUL => Some("MUL"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for FragmentShadingRateNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::TYPE_1_INVOCATION_PER_PIXEL => Some("TYPE_1_INVOCATION_PER_PIXEL"),
            Self::TYPE_1_INVOCATION_PER_1X2_PIXELS => Some("TYPE_1_INVOCATION_PER_1X2_PIXELS"),
            Self::TYPE_1_INVOCATION_PER_2X1_PIXELS => Some("TYPE_1_INVOCATION_PER_2X1_PIXELS"),
            Self::TYPE_1_INVOCATION_PER_2X2_PIXELS => Some("TYPE_1_INVOCATION_PER_2X2_PIXELS"),
            Self::TYPE_1_INVOCATION_PER_2X4_PIXELS => Some("TYPE_1_INVOCATION_PER_2X4_PIXELS"),
            Self::TYPE_1_INVOCATION_PER_4X2_PIXELS => Some("TYPE_1_INVOCATION_PER_4X2_PIXELS"),
            Self::TYPE_1_INVOCATION_PER_4X4_PIXELS => Some("TYPE_1_INVOCATION_PER_4X4_PIXELS"),
            Self::TYPE_2_INVOCATIONS_PER_PIXEL => Some("TYPE_2_INVOCATIONS_PER_PIXEL"),
            Self::TYPE_4_INVOCATIONS_PER_PIXEL => Some("TYPE_4_INVOCATIONS_PER_PIXEL"),
            Self::TYPE_8_INVOCATIONS_PER_PIXEL => Some("TYPE_8_INVOCATIONS_PER_PIXEL"),
            Self::TYPE_16_INVOCATIONS_PER_PIXEL => Some("TYPE_16_INVOCATIONS_PER_PIXEL"),
            Self::NO_INVOCATIONS => Some("NO_INVOCATIONS"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for FragmentShadingRateTypeNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::FRAGMENT_SIZE => Some("FRAGMENT_SIZE"),
            Self::ENUMS => Some("ENUMS"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for FramebufferCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(FramebufferCreateFlags::IMAGELESS.0, "IMAGELESS")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for FrontFace {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::COUNTER_CLOCKWISE => Some("COUNTER_CLOCKWISE"),
            Self::CLOCKWISE => Some("CLOCKWISE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for FullScreenExclusiveEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::DEFAULT => Some("DEFAULT"),
            Self::ALLOWED => Some("ALLOWED"),
            Self::DISALLOWED => Some("DISALLOWED"),
            Self::APPLICATION_CONTROLLED => Some("APPLICATION_CONTROLLED"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for GeometryFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (GeometryFlagsKHR::OPAQUE.0, "OPAQUE"),
            (
                GeometryFlagsKHR::NO_DUPLICATE_ANY_HIT_INVOCATION.0,
                "NO_DUPLICATE_ANY_HIT_INVOCATION",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for GeometryInstanceFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                GeometryInstanceFlagsKHR::TRIANGLE_FACING_CULL_DISABLE.0,
                "TRIANGLE_FACING_CULL_DISABLE",
            ),
            (
                GeometryInstanceFlagsKHR::TRIANGLE_FRONT_COUNTERCLOCKWISE.0,
                "TRIANGLE_FRONT_COUNTERCLOCKWISE",
            ),
            (GeometryInstanceFlagsKHR::FORCE_OPAQUE.0, "FORCE_OPAQUE"),
            (
                GeometryInstanceFlagsKHR::FORCE_NO_OPAQUE.0,
                "FORCE_NO_OPAQUE",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for GeometryTypeKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::TRIANGLES => Some("TRIANGLES"),
            Self::AABBS => Some("AABBS"),
            Self::INSTANCES => Some("INSTANCES"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for HeadlessSurfaceCreateFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for IOSSurfaceCreateFlagsMVK {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ImageAspectFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ImageAspectFlags::COLOR.0, "COLOR"),
            (ImageAspectFlags::DEPTH.0, "DEPTH"),
            (ImageAspectFlags::STENCIL.0, "STENCIL"),
            (ImageAspectFlags::METADATA.0, "METADATA"),
            (ImageAspectFlags::MEMORY_PLANE_0_EXT.0, "MEMORY_PLANE_0_EXT"),
            (ImageAspectFlags::MEMORY_PLANE_1_EXT.0, "MEMORY_PLANE_1_EXT"),
            (ImageAspectFlags::MEMORY_PLANE_2_EXT.0, "MEMORY_PLANE_2_EXT"),
            (ImageAspectFlags::MEMORY_PLANE_3_EXT.0, "MEMORY_PLANE_3_EXT"),
            (ImageAspectFlags::PLANE_0.0, "PLANE_0"),
            (ImageAspectFlags::PLANE_1.0, "PLANE_1"),
            (ImageAspectFlags::PLANE_2.0, "PLANE_2"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ImageCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ImageCreateFlags::SPARSE_BINDING.0, "SPARSE_BINDING"),
            (ImageCreateFlags::SPARSE_RESIDENCY.0, "SPARSE_RESIDENCY"),
            (ImageCreateFlags::SPARSE_ALIASED.0, "SPARSE_ALIASED"),
            (ImageCreateFlags::MUTABLE_FORMAT.0, "MUTABLE_FORMAT"),
            (ImageCreateFlags::CUBE_COMPATIBLE.0, "CUBE_COMPATIBLE"),
            (ImageCreateFlags::CORNER_SAMPLED_NV.0, "CORNER_SAMPLED_NV"),
            (
                ImageCreateFlags::SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_EXT.0,
                "SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_EXT",
            ),
            (ImageCreateFlags::SUBSAMPLED_EXT.0, "SUBSAMPLED_EXT"),
            (ImageCreateFlags::RESERVED_15_NV.0, "RESERVED_15_NV"),
            (ImageCreateFlags::ALIAS.0, "ALIAS"),
            (
                ImageCreateFlags::SPLIT_INSTANCE_BIND_REGIONS.0,
                "SPLIT_INSTANCE_BIND_REGIONS",
            ),
            (
                ImageCreateFlags::TYPE_2D_ARRAY_COMPATIBLE.0,
                "TYPE_2D_ARRAY_COMPATIBLE",
            ),
            (
                ImageCreateFlags::BLOCK_TEXEL_VIEW_COMPATIBLE.0,
                "BLOCK_TEXEL_VIEW_COMPATIBLE",
            ),
            (ImageCreateFlags::EXTENDED_USAGE.0, "EXTENDED_USAGE"),
            (ImageCreateFlags::PROTECTED.0, "PROTECTED"),
            (ImageCreateFlags::DISJOINT.0, "DISJOINT"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ImageLayout {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::UNDEFINED => Some("UNDEFINED"),
            Self::GENERAL => Some("GENERAL"),
            Self::COLOR_ATTACHMENT_OPTIMAL => Some("COLOR_ATTACHMENT_OPTIMAL"),
            Self::DEPTH_STENCIL_ATTACHMENT_OPTIMAL => Some("DEPTH_STENCIL_ATTACHMENT_OPTIMAL"),
            Self::DEPTH_STENCIL_READ_ONLY_OPTIMAL => Some("DEPTH_STENCIL_READ_ONLY_OPTIMAL"),
            Self::SHADER_READ_ONLY_OPTIMAL => Some("SHADER_READ_ONLY_OPTIMAL"),
            Self::TRANSFER_SRC_OPTIMAL => Some("TRANSFER_SRC_OPTIMAL"),
            Self::TRANSFER_DST_OPTIMAL => Some("TRANSFER_DST_OPTIMAL"),
            Self::PREINITIALIZED => Some("PREINITIALIZED"),
            Self::PRESENT_SRC_KHR => Some("PRESENT_SRC_KHR"),
            Self::SHARED_PRESENT_KHR => Some("SHARED_PRESENT_KHR"),
            Self::SHADING_RATE_OPTIMAL_NV => Some("SHADING_RATE_OPTIMAL_NV"),
            Self::FRAGMENT_DENSITY_MAP_OPTIMAL_EXT => Some("FRAGMENT_DENSITY_MAP_OPTIMAL_EXT"),
            Self::DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL => {
                Some("DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL")
            }
            Self::DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL => {
                Some("DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL")
            }
            Self::DEPTH_ATTACHMENT_OPTIMAL => Some("DEPTH_ATTACHMENT_OPTIMAL"),
            Self::DEPTH_READ_ONLY_OPTIMAL => Some("DEPTH_READ_ONLY_OPTIMAL"),
            Self::STENCIL_ATTACHMENT_OPTIMAL => Some("STENCIL_ATTACHMENT_OPTIMAL"),
            Self::STENCIL_READ_ONLY_OPTIMAL => Some("STENCIL_READ_ONLY_OPTIMAL"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ImagePipeSurfaceCreateFlagsFUCHSIA {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ImageTiling {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::OPTIMAL => Some("OPTIMAL"),
            Self::LINEAR => Some("LINEAR"),
            Self::DRM_FORMAT_MODIFIER_EXT => Some("DRM_FORMAT_MODIFIER_EXT"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ImageType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::TYPE_1D => Some("TYPE_1D"),
            Self::TYPE_2D => Some("TYPE_2D"),
            Self::TYPE_3D => Some("TYPE_3D"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ImageUsageFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ImageUsageFlags::TRANSFER_SRC.0, "TRANSFER_SRC"),
            (ImageUsageFlags::TRANSFER_DST.0, "TRANSFER_DST"),
            (ImageUsageFlags::SAMPLED.0, "SAMPLED"),
            (ImageUsageFlags::STORAGE.0, "STORAGE"),
            (ImageUsageFlags::COLOR_ATTACHMENT.0, "COLOR_ATTACHMENT"),
            (
                ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT.0,
                "DEPTH_STENCIL_ATTACHMENT",
            ),
            (
                ImageUsageFlags::TRANSIENT_ATTACHMENT.0,
                "TRANSIENT_ATTACHMENT",
            ),
            (ImageUsageFlags::INPUT_ATTACHMENT.0, "INPUT_ATTACHMENT"),
            (ImageUsageFlags::RESERVED_13_KHR.0, "RESERVED_13_KHR"),
            (ImageUsageFlags::RESERVED_14_KHR.0, "RESERVED_14_KHR"),
            (ImageUsageFlags::RESERVED_15_KHR.0, "RESERVED_15_KHR"),
            (ImageUsageFlags::RESERVED_10_KHR.0, "RESERVED_10_KHR"),
            (ImageUsageFlags::RESERVED_11_KHR.0, "RESERVED_11_KHR"),
            (ImageUsageFlags::RESERVED_12_KHR.0, "RESERVED_12_KHR"),
            (
                ImageUsageFlags::SHADING_RATE_IMAGE_NV.0,
                "SHADING_RATE_IMAGE_NV",
            ),
            (ImageUsageFlags::RESERVED_16_QCOM.0, "RESERVED_16_QCOM"),
            (ImageUsageFlags::RESERVED_17_QCOM.0, "RESERVED_17_QCOM"),
            (
                ImageUsageFlags::FRAGMENT_DENSITY_MAP_EXT.0,
                "FRAGMENT_DENSITY_MAP_EXT",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ImageViewCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                ImageViewCreateFlags::FRAGMENT_DENSITY_MAP_DYNAMIC_EXT.0,
                "FRAGMENT_DENSITY_MAP_DYNAMIC_EXT",
            ),
            (
                ImageViewCreateFlags::FRAGMENT_DENSITY_MAP_DEFERRED_EXT.0,
                "FRAGMENT_DENSITY_MAP_DEFERRED_EXT",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ImageViewType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::TYPE_1D => Some("TYPE_1D"),
            Self::TYPE_2D => Some("TYPE_2D"),
            Self::TYPE_3D => Some("TYPE_3D"),
            Self::CUBE => Some("CUBE"),
            Self::TYPE_1D_ARRAY => Some("TYPE_1D_ARRAY"),
            Self::TYPE_2D_ARRAY => Some("TYPE_2D_ARRAY"),
            Self::CUBE_ARRAY => Some("CUBE_ARRAY"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for IndexType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::UINT16 => Some("UINT16"),
            Self::UINT32 => Some("UINT32"),
            Self::NONE_KHR => Some("NONE_KHR"),
            Self::UINT8_EXT => Some("UINT8_EXT"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for IndirectCommandsLayoutUsageFlagsNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                IndirectCommandsLayoutUsageFlagsNV::EXPLICIT_PREPROCESS.0,
                "EXPLICIT_PREPROCESS",
            ),
            (
                IndirectCommandsLayoutUsageFlagsNV::INDEXED_SEQUENCES.0,
                "INDEXED_SEQUENCES",
            ),
            (
                IndirectCommandsLayoutUsageFlagsNV::UNORDERED_SEQUENCES.0,
                "UNORDERED_SEQUENCES",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for IndirectCommandsTokenTypeNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::SHADER_GROUP => Some("SHADER_GROUP"),
            Self::STATE_FLAGS => Some("STATE_FLAGS"),
            Self::INDEX_BUFFER => Some("INDEX_BUFFER"),
            Self::VERTEX_BUFFER => Some("VERTEX_BUFFER"),
            Self::PUSH_CONSTANT => Some("PUSH_CONSTANT"),
            Self::DRAW_INDEXED => Some("DRAW_INDEXED"),
            Self::DRAW => Some("DRAW"),
            Self::DRAW_TASKS => Some("DRAW_TASKS"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for IndirectStateFlagsNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] =
            &[(IndirectStateFlagsNV::FLAG_FRONTFACE.0, "FLAG_FRONTFACE")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for InstanceCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for InternalAllocationType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::EXECUTABLE => Some("EXECUTABLE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for LineRasterizationModeEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::DEFAULT => Some("DEFAULT"),
            Self::RECTANGULAR => Some("RECTANGULAR"),
            Self::BRESENHAM => Some("BRESENHAM"),
            Self::RECTANGULAR_SMOOTH => Some("RECTANGULAR_SMOOTH"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for LogicOp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::CLEAR => Some("CLEAR"),
            Self::AND => Some("AND"),
            Self::AND_REVERSE => Some("AND_REVERSE"),
            Self::COPY => Some("COPY"),
            Self::AND_INVERTED => Some("AND_INVERTED"),
            Self::NO_OP => Some("NO_OP"),
            Self::XOR => Some("XOR"),
            Self::OR => Some("OR"),
            Self::NOR => Some("NOR"),
            Self::EQUIVALENT => Some("EQUIVALENT"),
            Self::INVERT => Some("INVERT"),
            Self::OR_REVERSE => Some("OR_REVERSE"),
            Self::COPY_INVERTED => Some("COPY_INVERTED"),
            Self::OR_INVERTED => Some("OR_INVERTED"),
            Self::NAND => Some("NAND"),
            Self::SET => Some("SET"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for MacOSSurfaceCreateFlagsMVK {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for MemoryAllocateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (MemoryAllocateFlags::DEVICE_MASK.0, "DEVICE_MASK"),
            (MemoryAllocateFlags::DEVICE_ADDRESS.0, "DEVICE_ADDRESS"),
            (
                MemoryAllocateFlags::DEVICE_ADDRESS_CAPTURE_REPLAY.0,
                "DEVICE_ADDRESS_CAPTURE_REPLAY",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for MemoryHeapFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (MemoryHeapFlags::DEVICE_LOCAL.0, "DEVICE_LOCAL"),
            (MemoryHeapFlags::RESERVED_2_KHR.0, "RESERVED_2_KHR"),
            (MemoryHeapFlags::MULTI_INSTANCE.0, "MULTI_INSTANCE"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for MemoryMapFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for MemoryOverallocationBehaviorAMD {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::DEFAULT => Some("DEFAULT"),
            Self::ALLOWED => Some("ALLOWED"),
            Self::DISALLOWED => Some("DISALLOWED"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for MemoryPropertyFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (MemoryPropertyFlags::DEVICE_LOCAL.0, "DEVICE_LOCAL"),
            (MemoryPropertyFlags::HOST_VISIBLE.0, "HOST_VISIBLE"),
            (MemoryPropertyFlags::HOST_COHERENT.0, "HOST_COHERENT"),
            (MemoryPropertyFlags::HOST_CACHED.0, "HOST_CACHED"),
            (MemoryPropertyFlags::LAZILY_ALLOCATED.0, "LAZILY_ALLOCATED"),
            (
                MemoryPropertyFlags::DEVICE_COHERENT_AMD.0,
                "DEVICE_COHERENT_AMD",
            ),
            (
                MemoryPropertyFlags::DEVICE_UNCACHED_AMD.0,
                "DEVICE_UNCACHED_AMD",
            ),
            (MemoryPropertyFlags::PROTECTED.0, "PROTECTED"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for MetalSurfaceCreateFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ObjectType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::UNKNOWN => Some("UNKNOWN"),
            Self::INSTANCE => Some("INSTANCE"),
            Self::PHYSICAL_DEVICE => Some("PHYSICAL_DEVICE"),
            Self::DEVICE => Some("DEVICE"),
            Self::QUEUE => Some("QUEUE"),
            Self::SEMAPHORE => Some("SEMAPHORE"),
            Self::COMMAND_BUFFER => Some("COMMAND_BUFFER"),
            Self::FENCE => Some("FENCE"),
            Self::DEVICE_MEMORY => Some("DEVICE_MEMORY"),
            Self::BUFFER => Some("BUFFER"),
            Self::IMAGE => Some("IMAGE"),
            Self::EVENT => Some("EVENT"),
            Self::QUERY_POOL => Some("QUERY_POOL"),
            Self::BUFFER_VIEW => Some("BUFFER_VIEW"),
            Self::IMAGE_VIEW => Some("IMAGE_VIEW"),
            Self::SHADER_MODULE => Some("SHADER_MODULE"),
            Self::PIPELINE_CACHE => Some("PIPELINE_CACHE"),
            Self::PIPELINE_LAYOUT => Some("PIPELINE_LAYOUT"),
            Self::RENDER_PASS => Some("RENDER_PASS"),
            Self::PIPELINE => Some("PIPELINE"),
            Self::DESCRIPTOR_SET_LAYOUT => Some("DESCRIPTOR_SET_LAYOUT"),
            Self::SAMPLER => Some("SAMPLER"),
            Self::DESCRIPTOR_POOL => Some("DESCRIPTOR_POOL"),
            Self::DESCRIPTOR_SET => Some("DESCRIPTOR_SET"),
            Self::FRAMEBUFFER => Some("FRAMEBUFFER"),
            Self::COMMAND_POOL => Some("COMMAND_POOL"),
            Self::SURFACE_KHR => Some("SURFACE_KHR"),
            Self::SWAPCHAIN_KHR => Some("SWAPCHAIN_KHR"),
            Self::DISPLAY_KHR => Some("DISPLAY_KHR"),
            Self::DISPLAY_MODE_KHR => Some("DISPLAY_MODE_KHR"),
            Self::DEBUG_REPORT_CALLBACK_EXT => Some("DEBUG_REPORT_CALLBACK_EXT"),
            Self::DEBUG_UTILS_MESSENGER_EXT => Some("DEBUG_UTILS_MESSENGER_EXT"),
            Self::ACCELERATION_STRUCTURE_KHR => Some("ACCELERATION_STRUCTURE_KHR"),
            Self::VALIDATION_CACHE_EXT => Some("VALIDATION_CACHE_EXT"),
            Self::ACCELERATION_STRUCTURE_NV => Some("ACCELERATION_STRUCTURE_NV"),
            Self::PERFORMANCE_CONFIGURATION_INTEL => Some("PERFORMANCE_CONFIGURATION_INTEL"),
            Self::DEFERRED_OPERATION_KHR => Some("DEFERRED_OPERATION_KHR"),
            Self::INDIRECT_COMMANDS_LAYOUT_NV => Some("INDIRECT_COMMANDS_LAYOUT_NV"),
            Self::PRIVATE_DATA_SLOT_EXT => Some("PRIVATE_DATA_SLOT_EXT"),
            Self::SAMPLER_YCBCR_CONVERSION => Some("SAMPLER_YCBCR_CONVERSION"),
            Self::DESCRIPTOR_UPDATE_TEMPLATE => Some("DESCRIPTOR_UPDATE_TEMPLATE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PeerMemoryFeatureFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (PeerMemoryFeatureFlags::COPY_SRC.0, "COPY_SRC"),
            (PeerMemoryFeatureFlags::COPY_DST.0, "COPY_DST"),
            (PeerMemoryFeatureFlags::GENERIC_SRC.0, "GENERIC_SRC"),
            (PeerMemoryFeatureFlags::GENERIC_DST.0, "GENERIC_DST"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PerformanceConfigurationTypeINTEL {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match * self { Self :: PERFORMANCE_CONFIGURATION_TYPE_COMMAND_QUEUE_METRICS_DISCOVERY_ACTIVATED_INTEL => Some ("PERFORMANCE_CONFIGURATION_TYPE_COMMAND_QUEUE_METRICS_DISCOVERY_ACTIVATED_INTEL") , _ => None , } ;
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PerformanceCounterDescriptionFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                PerformanceCounterDescriptionFlagsKHR::PERFORMANCE_IMPACTING.0,
                "PERFORMANCE_IMPACTING",
            ),
            (
                PerformanceCounterDescriptionFlagsKHR::CONCURRENTLY_IMPACTED.0,
                "CONCURRENTLY_IMPACTED",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PerformanceCounterScopeKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::COMMAND_BUFFER => Some("COMMAND_BUFFER"),
            Self::RENDER_PASS => Some("RENDER_PASS"),
            Self::COMMAND => Some("COMMAND"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PerformanceCounterStorageKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::INT32 => Some("INT32"),
            Self::INT64 => Some("INT64"),
            Self::UINT32 => Some("UINT32"),
            Self::UINT64 => Some("UINT64"),
            Self::FLOAT32 => Some("FLOAT32"),
            Self::FLOAT64 => Some("FLOAT64"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PerformanceCounterUnitKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::GENERIC => Some("GENERIC"),
            Self::PERCENTAGE => Some("PERCENTAGE"),
            Self::NANOSECONDS => Some("NANOSECONDS"),
            Self::BYTES => Some("BYTES"),
            Self::BYTES_PER_SECOND => Some("BYTES_PER_SECOND"),
            Self::KELVIN => Some("KELVIN"),
            Self::WATTS => Some("WATTS"),
            Self::VOLTS => Some("VOLTS"),
            Self::AMPS => Some("AMPS"),
            Self::HERTZ => Some("HERTZ"),
            Self::CYCLES => Some("CYCLES"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PerformanceOverrideTypeINTEL {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::PERFORMANCE_OVERRIDE_TYPE_NULL_HARDWARE_INTEL => {
                Some("PERFORMANCE_OVERRIDE_TYPE_NULL_HARDWARE_INTEL")
            }
            Self::PERFORMANCE_OVERRIDE_TYPE_FLUSH_GPU_CACHES_INTEL => {
                Some("PERFORMANCE_OVERRIDE_TYPE_FLUSH_GPU_CACHES_INTEL")
            }
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PerformanceParameterTypeINTEL {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::PERFORMANCE_PARAMETER_TYPE_HW_COUNTERS_SUPPORTED_INTEL => {
                Some("PERFORMANCE_PARAMETER_TYPE_HW_COUNTERS_SUPPORTED_INTEL")
            }
            Self::PERFORMANCE_PARAMETER_TYPE_STREAM_MARKER_VALIDS_INTEL => {
                Some("PERFORMANCE_PARAMETER_TYPE_STREAM_MARKER_VALIDS_INTEL")
            }
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PerformanceValueTypeINTEL {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::PERFORMANCE_VALUE_TYPE_UINT32_INTEL => {
                Some("PERFORMANCE_VALUE_TYPE_UINT32_INTEL")
            }
            Self::PERFORMANCE_VALUE_TYPE_UINT64_INTEL => {
                Some("PERFORMANCE_VALUE_TYPE_UINT64_INTEL")
            }
            Self::PERFORMANCE_VALUE_TYPE_FLOAT_INTEL => Some("PERFORMANCE_VALUE_TYPE_FLOAT_INTEL"),
            Self::PERFORMANCE_VALUE_TYPE_BOOL_INTEL => Some("PERFORMANCE_VALUE_TYPE_BOOL_INTEL"),
            Self::PERFORMANCE_VALUE_TYPE_STRING_INTEL => {
                Some("PERFORMANCE_VALUE_TYPE_STRING_INTEL")
            }
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PhysicalDeviceType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::OTHER => Some("OTHER"),
            Self::INTEGRATED_GPU => Some("INTEGRATED_GPU"),
            Self::DISCRETE_GPU => Some("DISCRETE_GPU"),
            Self::VIRTUAL_GPU => Some("VIRTUAL_GPU"),
            Self::CPU => Some("CPU"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PipelineBindPoint {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::GRAPHICS => Some("GRAPHICS"),
            Self::COMPUTE => Some("COMPUTE"),
            Self::RAY_TRACING_KHR => Some("RAY_TRACING_KHR"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PipelineCacheCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (PipelineCacheCreateFlags::RESERVED_1_EXT.0, "RESERVED_1_EXT"),
            (
                PipelineCacheCreateFlags::EXTERNALLY_SYNCHRONIZED_EXT.0,
                "EXTERNALLY_SYNCHRONIZED_EXT",
            ),
            (PipelineCacheCreateFlags::RESERVED_2_EXT.0, "RESERVED_2_EXT"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineCacheHeaderVersion {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::ONE => Some("ONE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PipelineColorBlendStateCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineCompilerControlFlagsAMD {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineCoverageModulationStateCreateFlagsNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineCoverageReductionStateCreateFlagsNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineCoverageToColorStateCreateFlagsNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                PipelineCreateFlags::DISABLE_OPTIMIZATION.0,
                "DISABLE_OPTIMIZATION",
            ),
            (
                PipelineCreateFlags::ALLOW_DERIVATIVES.0,
                "ALLOW_DERIVATIVES",
            ),
            (PipelineCreateFlags::DERIVATIVE.0, "DERIVATIVE"),
            (
                PipelineCreateFlags::RAY_TRACING_NO_NULL_ANY_HIT_SHADERS_KHR.0,
                "RAY_TRACING_NO_NULL_ANY_HIT_SHADERS_KHR",
            ),
            (
                PipelineCreateFlags::RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_KHR.0,
                "RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_KHR",
            ),
            (
                PipelineCreateFlags::RAY_TRACING_NO_NULL_MISS_SHADERS_KHR.0,
                "RAY_TRACING_NO_NULL_MISS_SHADERS_KHR",
            ),
            (
                PipelineCreateFlags::RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_KHR.0,
                "RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_KHR",
            ),
            (
                PipelineCreateFlags::RAY_TRACING_SKIP_TRIANGLES_KHR.0,
                "RAY_TRACING_SKIP_TRIANGLES_KHR",
            ),
            (
                PipelineCreateFlags::RAY_TRACING_SKIP_AABBS_KHR.0,
                "RAY_TRACING_SKIP_AABBS_KHR",
            ),
            (
                PipelineCreateFlags::RAY_TRACING_SHADER_GROUP_HANDLE_CAPTURE_REPLAY_KHR.0,
                "RAY_TRACING_SHADER_GROUP_HANDLE_CAPTURE_REPLAY_KHR",
            ),
            (PipelineCreateFlags::DEFER_COMPILE_NV.0, "DEFER_COMPILE_NV"),
            (
                PipelineCreateFlags::CAPTURE_STATISTICS_KHR.0,
                "CAPTURE_STATISTICS_KHR",
            ),
            (
                PipelineCreateFlags::CAPTURE_INTERNAL_REPRESENTATIONS_KHR.0,
                "CAPTURE_INTERNAL_REPRESENTATIONS_KHR",
            ),
            (
                PipelineCreateFlags::INDIRECT_BINDABLE_NV.0,
                "INDIRECT_BINDABLE_NV",
            ),
            (PipelineCreateFlags::LIBRARY_KHR.0, "LIBRARY_KHR"),
            (
                PipelineCreateFlags::FAIL_ON_PIPELINE_COMPILE_REQUIRED_EXT.0,
                "FAIL_ON_PIPELINE_COMPILE_REQUIRED_EXT",
            ),
            (
                PipelineCreateFlags::EARLY_RETURN_ON_FAILURE_EXT.0,
                "EARLY_RETURN_ON_FAILURE_EXT",
            ),
            (
                PipelineCreateFlags::VIEW_INDEX_FROM_DEVICE_INDEX.0,
                "VIEW_INDEX_FROM_DEVICE_INDEX",
            ),
            (PipelineCreateFlags::DISPATCH_BASE.0, "DISPATCH_BASE"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineCreationFeedbackFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (PipelineCreationFeedbackFlagsEXT::VALID.0, "VALID"),
            (
                PipelineCreationFeedbackFlagsEXT::APPLICATION_PIPELINE_CACHE_HIT.0,
                "APPLICATION_PIPELINE_CACHE_HIT",
            ),
            (
                PipelineCreationFeedbackFlagsEXT::BASE_PIPELINE_ACCELERATION.0,
                "BASE_PIPELINE_ACCELERATION",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineDepthStencilStateCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineDiscardRectangleStateCreateFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineDynamicStateCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineExecutableStatisticFormatKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::BOOL32 => Some("BOOL32"),
            Self::INT64 => Some("INT64"),
            Self::UINT64 => Some("UINT64"),
            Self::FLOAT64 => Some("FLOAT64"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PipelineInputAssemblyStateCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineLayoutCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineMultisampleStateCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineRasterizationConservativeStateCreateFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineRasterizationDepthClipStateCreateFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineRasterizationStateCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineRasterizationStateStreamCreateFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineShaderStageCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                PipelineShaderStageCreateFlags::RESERVED_2_NV.0,
                "RESERVED_2_NV",
            ),
            (
                PipelineShaderStageCreateFlags::ALLOW_VARYING_SUBGROUP_SIZE_EXT.0,
                "ALLOW_VARYING_SUBGROUP_SIZE_EXT",
            ),
            (
                PipelineShaderStageCreateFlags::REQUIRE_FULL_SUBGROUPS_EXT.0,
                "REQUIRE_FULL_SUBGROUPS_EXT",
            ),
            (
                PipelineShaderStageCreateFlags::RESERVED_3_KHR.0,
                "RESERVED_3_KHR",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineStageFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (PipelineStageFlags::TOP_OF_PIPE.0, "TOP_OF_PIPE"),
            (PipelineStageFlags::DRAW_INDIRECT.0, "DRAW_INDIRECT"),
            (PipelineStageFlags::VERTEX_INPUT.0, "VERTEX_INPUT"),
            (PipelineStageFlags::VERTEX_SHADER.0, "VERTEX_SHADER"),
            (
                PipelineStageFlags::TESSELLATION_CONTROL_SHADER.0,
                "TESSELLATION_CONTROL_SHADER",
            ),
            (
                PipelineStageFlags::TESSELLATION_EVALUATION_SHADER.0,
                "TESSELLATION_EVALUATION_SHADER",
            ),
            (PipelineStageFlags::GEOMETRY_SHADER.0, "GEOMETRY_SHADER"),
            (PipelineStageFlags::FRAGMENT_SHADER.0, "FRAGMENT_SHADER"),
            (
                PipelineStageFlags::EARLY_FRAGMENT_TESTS.0,
                "EARLY_FRAGMENT_TESTS",
            ),
            (
                PipelineStageFlags::LATE_FRAGMENT_TESTS.0,
                "LATE_FRAGMENT_TESTS",
            ),
            (
                PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT.0,
                "COLOR_ATTACHMENT_OUTPUT",
            ),
            (PipelineStageFlags::COMPUTE_SHADER.0, "COMPUTE_SHADER"),
            (PipelineStageFlags::TRANSFER.0, "TRANSFER"),
            (PipelineStageFlags::BOTTOM_OF_PIPE.0, "BOTTOM_OF_PIPE"),
            (PipelineStageFlags::HOST.0, "HOST"),
            (PipelineStageFlags::ALL_GRAPHICS.0, "ALL_GRAPHICS"),
            (PipelineStageFlags::ALL_COMMANDS.0, "ALL_COMMANDS"),
            (PipelineStageFlags::RESERVED_27_KHR.0, "RESERVED_27_KHR"),
            (PipelineStageFlags::RESERVED_26_KHR.0, "RESERVED_26_KHR"),
            (
                PipelineStageFlags::TRANSFORM_FEEDBACK_EXT.0,
                "TRANSFORM_FEEDBACK_EXT",
            ),
            (
                PipelineStageFlags::CONDITIONAL_RENDERING_EXT.0,
                "CONDITIONAL_RENDERING_EXT",
            ),
            (
                PipelineStageFlags::ACCELERATION_STRUCTURE_BUILD_KHR.0,
                "ACCELERATION_STRUCTURE_BUILD_KHR",
            ),
            (
                PipelineStageFlags::RAY_TRACING_SHADER_KHR.0,
                "RAY_TRACING_SHADER_KHR",
            ),
            (
                PipelineStageFlags::SHADING_RATE_IMAGE_NV.0,
                "SHADING_RATE_IMAGE_NV",
            ),
            (PipelineStageFlags::TASK_SHADER_NV.0, "TASK_SHADER_NV"),
            (PipelineStageFlags::MESH_SHADER_NV.0, "MESH_SHADER_NV"),
            (
                PipelineStageFlags::FRAGMENT_DENSITY_PROCESS_EXT.0,
                "FRAGMENT_DENSITY_PROCESS_EXT",
            ),
            (
                PipelineStageFlags::COMMAND_PREPROCESS_NV.0,
                "COMMAND_PREPROCESS_NV",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineTessellationStateCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineVertexInputStateCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineViewportStateCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PipelineViewportSwizzleStateCreateFlagsNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for PointClippingBehavior {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::ALL_CLIP_PLANES => Some("ALL_CLIP_PLANES"),
            Self::USER_CLIP_PLANES_ONLY => Some("USER_CLIP_PLANES_ONLY"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PolygonMode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::FILL => Some("FILL"),
            Self::LINE => Some("LINE"),
            Self::POINT => Some("POINT"),
            Self::FILL_RECTANGLE_NV => Some("FILL_RECTANGLE_NV"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PresentModeKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::IMMEDIATE => Some("IMMEDIATE"),
            Self::MAILBOX => Some("MAILBOX"),
            Self::FIFO => Some("FIFO"),
            Self::FIFO_RELAXED => Some("FIFO_RELAXED"),
            Self::SHARED_DEMAND_REFRESH => Some("SHARED_DEMAND_REFRESH"),
            Self::SHARED_CONTINUOUS_REFRESH => Some("SHARED_CONTINUOUS_REFRESH"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PrimitiveTopology {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::POINT_LIST => Some("POINT_LIST"),
            Self::LINE_LIST => Some("LINE_LIST"),
            Self::LINE_STRIP => Some("LINE_STRIP"),
            Self::TRIANGLE_LIST => Some("TRIANGLE_LIST"),
            Self::TRIANGLE_STRIP => Some("TRIANGLE_STRIP"),
            Self::TRIANGLE_FAN => Some("TRIANGLE_FAN"),
            Self::LINE_LIST_WITH_ADJACENCY => Some("LINE_LIST_WITH_ADJACENCY"),
            Self::LINE_STRIP_WITH_ADJACENCY => Some("LINE_STRIP_WITH_ADJACENCY"),
            Self::TRIANGLE_LIST_WITH_ADJACENCY => Some("TRIANGLE_LIST_WITH_ADJACENCY"),
            Self::TRIANGLE_STRIP_WITH_ADJACENCY => Some("TRIANGLE_STRIP_WITH_ADJACENCY"),
            Self::PATCH_LIST => Some("PATCH_LIST"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for PrivateDataSlotCreateFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for QueryControlFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(QueryControlFlags::PRECISE.0, "PRECISE")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for QueryPipelineStatisticFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                QueryPipelineStatisticFlags::INPUT_ASSEMBLY_VERTICES.0,
                "INPUT_ASSEMBLY_VERTICES",
            ),
            (
                QueryPipelineStatisticFlags::INPUT_ASSEMBLY_PRIMITIVES.0,
                "INPUT_ASSEMBLY_PRIMITIVES",
            ),
            (
                QueryPipelineStatisticFlags::VERTEX_SHADER_INVOCATIONS.0,
                "VERTEX_SHADER_INVOCATIONS",
            ),
            (
                QueryPipelineStatisticFlags::GEOMETRY_SHADER_INVOCATIONS.0,
                "GEOMETRY_SHADER_INVOCATIONS",
            ),
            (
                QueryPipelineStatisticFlags::GEOMETRY_SHADER_PRIMITIVES.0,
                "GEOMETRY_SHADER_PRIMITIVES",
            ),
            (
                QueryPipelineStatisticFlags::CLIPPING_INVOCATIONS.0,
                "CLIPPING_INVOCATIONS",
            ),
            (
                QueryPipelineStatisticFlags::CLIPPING_PRIMITIVES.0,
                "CLIPPING_PRIMITIVES",
            ),
            (
                QueryPipelineStatisticFlags::FRAGMENT_SHADER_INVOCATIONS.0,
                "FRAGMENT_SHADER_INVOCATIONS",
            ),
            (
                QueryPipelineStatisticFlags::TESSELLATION_CONTROL_SHADER_PATCHES.0,
                "TESSELLATION_CONTROL_SHADER_PATCHES",
            ),
            (
                QueryPipelineStatisticFlags::TESSELLATION_EVALUATION_SHADER_INVOCATIONS.0,
                "TESSELLATION_EVALUATION_SHADER_INVOCATIONS",
            ),
            (
                QueryPipelineStatisticFlags::COMPUTE_SHADER_INVOCATIONS.0,
                "COMPUTE_SHADER_INVOCATIONS",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for QueryPoolCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for QueryPoolSamplingModeINTEL {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::QUERY_POOL_SAMPLING_MODE_MANUAL_INTEL => {
                Some("QUERY_POOL_SAMPLING_MODE_MANUAL_INTEL")
            }
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for QueryResultFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (QueryResultFlags::TYPE_64.0, "TYPE_64"),
            (QueryResultFlags::WAIT.0, "WAIT"),
            (QueryResultFlags::WITH_AVAILABILITY.0, "WITH_AVAILABILITY"),
            (QueryResultFlags::PARTIAL.0, "PARTIAL"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for QueryType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::OCCLUSION => Some("OCCLUSION"),
            Self::PIPELINE_STATISTICS => Some("PIPELINE_STATISTICS"),
            Self::TIMESTAMP => Some("TIMESTAMP"),
            Self::RESERVED_8 => Some("RESERVED_8"),
            Self::RESERVED_4 => Some("RESERVED_4"),
            Self::TRANSFORM_FEEDBACK_STREAM_EXT => Some("TRANSFORM_FEEDBACK_STREAM_EXT"),
            Self::PERFORMANCE_QUERY_KHR => Some("PERFORMANCE_QUERY_KHR"),
            Self::ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR => {
                Some("ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR")
            }
            Self::ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR => {
                Some("ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR")
            }
            Self::ACCELERATION_STRUCTURE_COMPACTED_SIZE_NV => {
                Some("ACCELERATION_STRUCTURE_COMPACTED_SIZE_NV")
            }
            Self::PERFORMANCE_QUERY_INTEL => Some("PERFORMANCE_QUERY_INTEL"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for QueueFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (QueueFlags::GRAPHICS.0, "GRAPHICS"),
            (QueueFlags::COMPUTE.0, "COMPUTE"),
            (QueueFlags::TRANSFER.0, "TRANSFER"),
            (QueueFlags::SPARSE_BINDING.0, "SPARSE_BINDING"),
            (QueueFlags::RESERVED_6_KHR.0, "RESERVED_6_KHR"),
            (QueueFlags::RESERVED_5_KHR.0, "RESERVED_5_KHR"),
            (QueueFlags::PROTECTED.0, "PROTECTED"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for QueueGlobalPriorityEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::LOW => Some("LOW"),
            Self::MEDIUM => Some("MEDIUM"),
            Self::HIGH => Some("HIGH"),
            Self::REALTIME => Some("REALTIME"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for RasterizationOrderAMD {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::STRICT => Some("STRICT"),
            Self::RELAXED => Some("RELAXED"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for RayTracingShaderGroupTypeKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::GENERAL => Some("GENERAL"),
            Self::TRIANGLES_HIT_GROUP => Some("TRIANGLES_HIT_GROUP"),
            Self::PROCEDURAL_HIT_GROUP => Some("PROCEDURAL_HIT_GROUP"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for RenderPassCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (RenderPassCreateFlags::RESERVED_0_KHR.0, "RESERVED_0_KHR"),
            (RenderPassCreateFlags::TRANSFORM_QCOM.0, "TRANSFORM_QCOM"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ResolveModeFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ResolveModeFlags::NONE.0, "NONE"),
            (ResolveModeFlags::SAMPLE_ZERO.0, "SAMPLE_ZERO"),
            (ResolveModeFlags::AVERAGE.0, "AVERAGE"),
            (ResolveModeFlags::MIN.0, "MIN"),
            (ResolveModeFlags::MAX.0, "MAX"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for Result {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::SUCCESS => Some("SUCCESS"),
            Self::NOT_READY => Some("NOT_READY"),
            Self::TIMEOUT => Some("TIMEOUT"),
            Self::EVENT_SET => Some("EVENT_SET"),
            Self::EVENT_RESET => Some("EVENT_RESET"),
            Self::INCOMPLETE => Some("INCOMPLETE"),
            Self::ERROR_OUT_OF_HOST_MEMORY => Some("ERROR_OUT_OF_HOST_MEMORY"),
            Self::ERROR_OUT_OF_DEVICE_MEMORY => Some("ERROR_OUT_OF_DEVICE_MEMORY"),
            Self::ERROR_INITIALIZATION_FAILED => Some("ERROR_INITIALIZATION_FAILED"),
            Self::ERROR_DEVICE_LOST => Some("ERROR_DEVICE_LOST"),
            Self::ERROR_MEMORY_MAP_FAILED => Some("ERROR_MEMORY_MAP_FAILED"),
            Self::ERROR_LAYER_NOT_PRESENT => Some("ERROR_LAYER_NOT_PRESENT"),
            Self::ERROR_EXTENSION_NOT_PRESENT => Some("ERROR_EXTENSION_NOT_PRESENT"),
            Self::ERROR_FEATURE_NOT_PRESENT => Some("ERROR_FEATURE_NOT_PRESENT"),
            Self::ERROR_INCOMPATIBLE_DRIVER => Some("ERROR_INCOMPATIBLE_DRIVER"),
            Self::ERROR_TOO_MANY_OBJECTS => Some("ERROR_TOO_MANY_OBJECTS"),
            Self::ERROR_FORMAT_NOT_SUPPORTED => Some("ERROR_FORMAT_NOT_SUPPORTED"),
            Self::ERROR_FRAGMENTED_POOL => Some("ERROR_FRAGMENTED_POOL"),
            Self::ERROR_UNKNOWN => Some("ERROR_UNKNOWN"),
            Self::ERROR_SURFACE_LOST_KHR => Some("ERROR_SURFACE_LOST_KHR"),
            Self::ERROR_NATIVE_WINDOW_IN_USE_KHR => Some("ERROR_NATIVE_WINDOW_IN_USE_KHR"),
            Self::SUBOPTIMAL_KHR => Some("SUBOPTIMAL_KHR"),
            Self::ERROR_OUT_OF_DATE_KHR => Some("ERROR_OUT_OF_DATE_KHR"),
            Self::ERROR_INCOMPATIBLE_DISPLAY_KHR => Some("ERROR_INCOMPATIBLE_DISPLAY_KHR"),
            Self::ERROR_VALIDATION_FAILED_EXT => Some("ERROR_VALIDATION_FAILED_EXT"),
            Self::ERROR_INVALID_SHADER_NV => Some("ERROR_INVALID_SHADER_NV"),
            Self::ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT => {
                Some("ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT")
            }
            Self::ERROR_NOT_PERMITTED_EXT => Some("ERROR_NOT_PERMITTED_EXT"),
            Self::ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT => {
                Some("ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT")
            }
            Self::THREAD_IDLE_KHR => Some("THREAD_IDLE_KHR"),
            Self::THREAD_DONE_KHR => Some("THREAD_DONE_KHR"),
            Self::OPERATION_DEFERRED_KHR => Some("OPERATION_DEFERRED_KHR"),
            Self::OPERATION_NOT_DEFERRED_KHR => Some("OPERATION_NOT_DEFERRED_KHR"),
            Self::PIPELINE_COMPILE_REQUIRED_EXT => Some("PIPELINE_COMPILE_REQUIRED_EXT"),
            Self::ERROR_OUT_OF_POOL_MEMORY => Some("ERROR_OUT_OF_POOL_MEMORY"),
            Self::ERROR_INVALID_EXTERNAL_HANDLE => Some("ERROR_INVALID_EXTERNAL_HANDLE"),
            Self::ERROR_FRAGMENTATION => Some("ERROR_FRAGMENTATION"),
            Self::ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS => {
                Some("ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS")
            }
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SampleCountFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (SampleCountFlags::TYPE_1.0, "TYPE_1"),
            (SampleCountFlags::TYPE_2.0, "TYPE_2"),
            (SampleCountFlags::TYPE_4.0, "TYPE_4"),
            (SampleCountFlags::TYPE_8.0, "TYPE_8"),
            (SampleCountFlags::TYPE_16.0, "TYPE_16"),
            (SampleCountFlags::TYPE_32.0, "TYPE_32"),
            (SampleCountFlags::TYPE_64.0, "TYPE_64"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SamplerAddressMode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::REPEAT => Some("REPEAT"),
            Self::MIRRORED_REPEAT => Some("MIRRORED_REPEAT"),
            Self::CLAMP_TO_EDGE => Some("CLAMP_TO_EDGE"),
            Self::CLAMP_TO_BORDER => Some("CLAMP_TO_BORDER"),
            Self::MIRROR_CLAMP_TO_EDGE => Some("MIRROR_CLAMP_TO_EDGE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SamplerCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (SamplerCreateFlags::SUBSAMPLED_EXT.0, "SUBSAMPLED_EXT"),
            (
                SamplerCreateFlags::SUBSAMPLED_COARSE_RECONSTRUCTION_EXT.0,
                "SUBSAMPLED_COARSE_RECONSTRUCTION_EXT",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SamplerMipmapMode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::NEAREST => Some("NEAREST"),
            Self::LINEAR => Some("LINEAR"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SamplerReductionMode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::WEIGHTED_AVERAGE => Some("WEIGHTED_AVERAGE"),
            Self::MIN => Some("MIN"),
            Self::MAX => Some("MAX"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SamplerYcbcrModelConversion {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::RGB_IDENTITY => Some("RGB_IDENTITY"),
            Self::YCBCR_IDENTITY => Some("YCBCR_IDENTITY"),
            Self::YCBCR_709 => Some("YCBCR_709"),
            Self::YCBCR_601 => Some("YCBCR_601"),
            Self::YCBCR_2020 => Some("YCBCR_2020"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SamplerYcbcrRange {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::ITU_FULL => Some("ITU_FULL"),
            Self::ITU_NARROW => Some("ITU_NARROW"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ScopeNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::DEVICE => Some("DEVICE"),
            Self::WORKGROUP => Some("WORKGROUP"),
            Self::SUBGROUP => Some("SUBGROUP"),
            Self::QUEUE_FAMILY => Some("QUEUE_FAMILY"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SemaphoreCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SemaphoreImportFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(SemaphoreImportFlags::TEMPORARY.0, "TEMPORARY")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SemaphoreType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::BINARY => Some("BINARY"),
            Self::TIMELINE => Some("TIMELINE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SemaphoreWaitFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(SemaphoreWaitFlags::ANY.0, "ANY")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ShaderCorePropertiesFlagsAMD {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ShaderFloatControlsIndependence {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::TYPE_32_ONLY => Some("TYPE_32_ONLY"),
            Self::ALL => Some("ALL"),
            Self::NONE => Some("NONE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ShaderGroupShaderKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::GENERAL => Some("GENERAL"),
            Self::CLOSEST_HIT => Some("CLOSEST_HIT"),
            Self::ANY_HIT => Some("ANY_HIT"),
            Self::INTERSECTION => Some("INTERSECTION"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ShaderInfoTypeAMD {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::STATISTICS => Some("STATISTICS"),
            Self::BINARY => Some("BINARY"),
            Self::DISASSEMBLY => Some("DISASSEMBLY"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ShaderModuleCreateFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] =
            &[(ShaderModuleCreateFlags::RESERVED_0_NV.0, "RESERVED_0_NV")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ShaderStageFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ShaderStageFlags::VERTEX.0, "VERTEX"),
            (
                ShaderStageFlags::TESSELLATION_CONTROL.0,
                "TESSELLATION_CONTROL",
            ),
            (
                ShaderStageFlags::TESSELLATION_EVALUATION.0,
                "TESSELLATION_EVALUATION",
            ),
            (ShaderStageFlags::GEOMETRY.0, "GEOMETRY"),
            (ShaderStageFlags::FRAGMENT.0, "FRAGMENT"),
            (ShaderStageFlags::COMPUTE.0, "COMPUTE"),
            (ShaderStageFlags::ALL_GRAPHICS.0, "ALL_GRAPHICS"),
            (ShaderStageFlags::ALL.0, "ALL"),
            (ShaderStageFlags::RAYGEN_KHR.0, "RAYGEN_KHR"),
            (ShaderStageFlags::ANY_HIT_KHR.0, "ANY_HIT_KHR"),
            (ShaderStageFlags::CLOSEST_HIT_KHR.0, "CLOSEST_HIT_KHR"),
            (ShaderStageFlags::MISS_KHR.0, "MISS_KHR"),
            (ShaderStageFlags::INTERSECTION_KHR.0, "INTERSECTION_KHR"),
            (ShaderStageFlags::CALLABLE_KHR.0, "CALLABLE_KHR"),
            (ShaderStageFlags::TASK_NV.0, "TASK_NV"),
            (ShaderStageFlags::MESH_NV.0, "MESH_NV"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ShadingRatePaletteEntryNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::NO_INVOCATIONS => Some("NO_INVOCATIONS"),
            Self::TYPE_16_INVOCATIONS_PER_PIXEL => Some("TYPE_16_INVOCATIONS_PER_PIXEL"),
            Self::TYPE_8_INVOCATIONS_PER_PIXEL => Some("TYPE_8_INVOCATIONS_PER_PIXEL"),
            Self::TYPE_4_INVOCATIONS_PER_PIXEL => Some("TYPE_4_INVOCATIONS_PER_PIXEL"),
            Self::TYPE_2_INVOCATIONS_PER_PIXEL => Some("TYPE_2_INVOCATIONS_PER_PIXEL"),
            Self::TYPE_1_INVOCATION_PER_PIXEL => Some("TYPE_1_INVOCATION_PER_PIXEL"),
            Self::TYPE_1_INVOCATION_PER_2X1_PIXELS => Some("TYPE_1_INVOCATION_PER_2X1_PIXELS"),
            Self::TYPE_1_INVOCATION_PER_1X2_PIXELS => Some("TYPE_1_INVOCATION_PER_1X2_PIXELS"),
            Self::TYPE_1_INVOCATION_PER_2X2_PIXELS => Some("TYPE_1_INVOCATION_PER_2X2_PIXELS"),
            Self::TYPE_1_INVOCATION_PER_4X2_PIXELS => Some("TYPE_1_INVOCATION_PER_4X2_PIXELS"),
            Self::TYPE_1_INVOCATION_PER_2X4_PIXELS => Some("TYPE_1_INVOCATION_PER_2X4_PIXELS"),
            Self::TYPE_1_INVOCATION_PER_4X4_PIXELS => Some("TYPE_1_INVOCATION_PER_4X4_PIXELS"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SharingMode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::EXCLUSIVE => Some("EXCLUSIVE"),
            Self::CONCURRENT => Some("CONCURRENT"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SparseImageFormatFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (SparseImageFormatFlags::SINGLE_MIPTAIL.0, "SINGLE_MIPTAIL"),
            (
                SparseImageFormatFlags::ALIGNED_MIP_SIZE.0,
                "ALIGNED_MIP_SIZE",
            ),
            (
                SparseImageFormatFlags::NONSTANDARD_BLOCK_SIZE.0,
                "NONSTANDARD_BLOCK_SIZE",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SparseMemoryBindFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(SparseMemoryBindFlags::METADATA.0, "METADATA")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for StencilFaceFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (StencilFaceFlags::FRONT.0, "FRONT"),
            (StencilFaceFlags::BACK.0, "BACK"),
            (StencilFaceFlags::FRONT_AND_BACK.0, "FRONT_AND_BACK"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for StencilOp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::KEEP => Some("KEEP"),
            Self::ZERO => Some("ZERO"),
            Self::REPLACE => Some("REPLACE"),
            Self::INCREMENT_AND_CLAMP => Some("INCREMENT_AND_CLAMP"),
            Self::DECREMENT_AND_CLAMP => Some("DECREMENT_AND_CLAMP"),
            Self::INVERT => Some("INVERT"),
            Self::INCREMENT_AND_WRAP => Some("INCREMENT_AND_WRAP"),
            Self::DECREMENT_AND_WRAP => Some("DECREMENT_AND_WRAP"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for StreamDescriptorSurfaceCreateFlagsGGP {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for StructureType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::APPLICATION_INFO => Some("APPLICATION_INFO"),
            Self::INSTANCE_CREATE_INFO => Some("INSTANCE_CREATE_INFO"),
            Self::DEVICE_QUEUE_CREATE_INFO => Some("DEVICE_QUEUE_CREATE_INFO"),
            Self::DEVICE_CREATE_INFO => Some("DEVICE_CREATE_INFO"),
            Self::SUBMIT_INFO => Some("SUBMIT_INFO"),
            Self::MEMORY_ALLOCATE_INFO => Some("MEMORY_ALLOCATE_INFO"),
            Self::MAPPED_MEMORY_RANGE => Some("MAPPED_MEMORY_RANGE"),
            Self::BIND_SPARSE_INFO => Some("BIND_SPARSE_INFO"),
            Self::FENCE_CREATE_INFO => Some("FENCE_CREATE_INFO"),
            Self::SEMAPHORE_CREATE_INFO => Some("SEMAPHORE_CREATE_INFO"),
            Self::EVENT_CREATE_INFO => Some("EVENT_CREATE_INFO"),
            Self::QUERY_POOL_CREATE_INFO => Some("QUERY_POOL_CREATE_INFO"),
            Self::BUFFER_CREATE_INFO => Some("BUFFER_CREATE_INFO"),
            Self::BUFFER_VIEW_CREATE_INFO => Some("BUFFER_VIEW_CREATE_INFO"),
            Self::IMAGE_CREATE_INFO => Some("IMAGE_CREATE_INFO"),
            Self::IMAGE_VIEW_CREATE_INFO => Some("IMAGE_VIEW_CREATE_INFO"),
            Self::SHADER_MODULE_CREATE_INFO => Some("SHADER_MODULE_CREATE_INFO"),
            Self::PIPELINE_CACHE_CREATE_INFO => Some("PIPELINE_CACHE_CREATE_INFO"),
            Self::PIPELINE_SHADER_STAGE_CREATE_INFO => Some("PIPELINE_SHADER_STAGE_CREATE_INFO"),
            Self::PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO => {
                Some("PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO")
            }
            Self::PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO => {
                Some("PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO")
            }
            Self::PIPELINE_TESSELLATION_STATE_CREATE_INFO => {
                Some("PIPELINE_TESSELLATION_STATE_CREATE_INFO")
            }
            Self::PIPELINE_VIEWPORT_STATE_CREATE_INFO => {
                Some("PIPELINE_VIEWPORT_STATE_CREATE_INFO")
            }
            Self::PIPELINE_RASTERIZATION_STATE_CREATE_INFO => {
                Some("PIPELINE_RASTERIZATION_STATE_CREATE_INFO")
            }
            Self::PIPELINE_MULTISAMPLE_STATE_CREATE_INFO => {
                Some("PIPELINE_MULTISAMPLE_STATE_CREATE_INFO")
            }
            Self::PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO => {
                Some("PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO")
            }
            Self::PIPELINE_COLOR_BLEND_STATE_CREATE_INFO => {
                Some("PIPELINE_COLOR_BLEND_STATE_CREATE_INFO")
            }
            Self::PIPELINE_DYNAMIC_STATE_CREATE_INFO => Some("PIPELINE_DYNAMIC_STATE_CREATE_INFO"),
            Self::GRAPHICS_PIPELINE_CREATE_INFO => Some("GRAPHICS_PIPELINE_CREATE_INFO"),
            Self::COMPUTE_PIPELINE_CREATE_INFO => Some("COMPUTE_PIPELINE_CREATE_INFO"),
            Self::PIPELINE_LAYOUT_CREATE_INFO => Some("PIPELINE_LAYOUT_CREATE_INFO"),
            Self::SAMPLER_CREATE_INFO => Some("SAMPLER_CREATE_INFO"),
            Self::DESCRIPTOR_SET_LAYOUT_CREATE_INFO => Some("DESCRIPTOR_SET_LAYOUT_CREATE_INFO"),
            Self::DESCRIPTOR_POOL_CREATE_INFO => Some("DESCRIPTOR_POOL_CREATE_INFO"),
            Self::DESCRIPTOR_SET_ALLOCATE_INFO => Some("DESCRIPTOR_SET_ALLOCATE_INFO"),
            Self::WRITE_DESCRIPTOR_SET => Some("WRITE_DESCRIPTOR_SET"),
            Self::COPY_DESCRIPTOR_SET => Some("COPY_DESCRIPTOR_SET"),
            Self::FRAMEBUFFER_CREATE_INFO => Some("FRAMEBUFFER_CREATE_INFO"),
            Self::RENDER_PASS_CREATE_INFO => Some("RENDER_PASS_CREATE_INFO"),
            Self::COMMAND_POOL_CREATE_INFO => Some("COMMAND_POOL_CREATE_INFO"),
            Self::COMMAND_BUFFER_ALLOCATE_INFO => Some("COMMAND_BUFFER_ALLOCATE_INFO"),
            Self::COMMAND_BUFFER_INHERITANCE_INFO => Some("COMMAND_BUFFER_INHERITANCE_INFO"),
            Self::COMMAND_BUFFER_BEGIN_INFO => Some("COMMAND_BUFFER_BEGIN_INFO"),
            Self::RENDER_PASS_BEGIN_INFO => Some("RENDER_PASS_BEGIN_INFO"),
            Self::BUFFER_MEMORY_BARRIER => Some("BUFFER_MEMORY_BARRIER"),
            Self::IMAGE_MEMORY_BARRIER => Some("IMAGE_MEMORY_BARRIER"),
            Self::MEMORY_BARRIER => Some("MEMORY_BARRIER"),
            Self::LOADER_INSTANCE_CREATE_INFO => Some("LOADER_INSTANCE_CREATE_INFO"),
            Self::LOADER_DEVICE_CREATE_INFO => Some("LOADER_DEVICE_CREATE_INFO"),
            Self::SWAPCHAIN_CREATE_INFO_KHR => Some("SWAPCHAIN_CREATE_INFO_KHR"),
            Self::PRESENT_INFO_KHR => Some("PRESENT_INFO_KHR"),
            Self::DEVICE_GROUP_PRESENT_CAPABILITIES_KHR => {
                Some("DEVICE_GROUP_PRESENT_CAPABILITIES_KHR")
            }
            Self::IMAGE_SWAPCHAIN_CREATE_INFO_KHR => Some("IMAGE_SWAPCHAIN_CREATE_INFO_KHR"),
            Self::BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR => {
                Some("BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR")
            }
            Self::ACQUIRE_NEXT_IMAGE_INFO_KHR => Some("ACQUIRE_NEXT_IMAGE_INFO_KHR"),
            Self::DEVICE_GROUP_PRESENT_INFO_KHR => Some("DEVICE_GROUP_PRESENT_INFO_KHR"),
            Self::DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR => {
                Some("DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR")
            }
            Self::DISPLAY_MODE_CREATE_INFO_KHR => Some("DISPLAY_MODE_CREATE_INFO_KHR"),
            Self::DISPLAY_SURFACE_CREATE_INFO_KHR => Some("DISPLAY_SURFACE_CREATE_INFO_KHR"),
            Self::DISPLAY_PRESENT_INFO_KHR => Some("DISPLAY_PRESENT_INFO_KHR"),
            Self::XLIB_SURFACE_CREATE_INFO_KHR => Some("XLIB_SURFACE_CREATE_INFO_KHR"),
            Self::XCB_SURFACE_CREATE_INFO_KHR => Some("XCB_SURFACE_CREATE_INFO_KHR"),
            Self::WAYLAND_SURFACE_CREATE_INFO_KHR => Some("WAYLAND_SURFACE_CREATE_INFO_KHR"),
            Self::ANDROID_SURFACE_CREATE_INFO_KHR => Some("ANDROID_SURFACE_CREATE_INFO_KHR"),
            Self::WIN32_SURFACE_CREATE_INFO_KHR => Some("WIN32_SURFACE_CREATE_INFO_KHR"),
            Self::NATIVE_BUFFER_ANDROID => Some("NATIVE_BUFFER_ANDROID"),
            Self::SWAPCHAIN_IMAGE_CREATE_INFO_ANDROID => {
                Some("SWAPCHAIN_IMAGE_CREATE_INFO_ANDROID")
            }
            Self::PHYSICAL_DEVICE_PRESENTATION_PROPERTIES_ANDROID => {
                Some("PHYSICAL_DEVICE_PRESENTATION_PROPERTIES_ANDROID")
            }
            Self::DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT => {
                Some("DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT")
            }
            Self::PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD => {
                Some("PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD")
            }
            Self::DEBUG_MARKER_OBJECT_NAME_INFO_EXT => Some("DEBUG_MARKER_OBJECT_NAME_INFO_EXT"),
            Self::DEBUG_MARKER_OBJECT_TAG_INFO_EXT => Some("DEBUG_MARKER_OBJECT_TAG_INFO_EXT"),
            Self::DEBUG_MARKER_MARKER_INFO_EXT => Some("DEBUG_MARKER_MARKER_INFO_EXT"),
            Self::DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV => {
                Some("DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV")
            }
            Self::DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV => {
                Some("DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV")
            }
            Self::DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV => {
                Some("DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV")
            }
            Self::PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT")
            }
            Self::PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT => {
                Some("PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT")
            }
            Self::IMAGE_VIEW_HANDLE_INFO_NVX => Some("IMAGE_VIEW_HANDLE_INFO_NVX"),
            Self::IMAGE_VIEW_ADDRESS_PROPERTIES_NVX => Some("IMAGE_VIEW_ADDRESS_PROPERTIES_NVX"),
            Self::TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD => {
                Some("TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD")
            }
            Self::STREAM_DESCRIPTOR_SURFACE_CREATE_INFO_GGP => {
                Some("STREAM_DESCRIPTOR_SURFACE_CREATE_INFO_GGP")
            }
            Self::PHYSICAL_DEVICE_CORNER_SAMPLED_IMAGE_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_CORNER_SAMPLED_IMAGE_FEATURES_NV")
            }
            Self::EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV => {
                Some("EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV")
            }
            Self::EXPORT_MEMORY_ALLOCATE_INFO_NV => Some("EXPORT_MEMORY_ALLOCATE_INFO_NV"),
            Self::IMPORT_MEMORY_WIN32_HANDLE_INFO_NV => Some("IMPORT_MEMORY_WIN32_HANDLE_INFO_NV"),
            Self::EXPORT_MEMORY_WIN32_HANDLE_INFO_NV => Some("EXPORT_MEMORY_WIN32_HANDLE_INFO_NV"),
            Self::WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV => {
                Some("WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV")
            }
            Self::VALIDATION_FLAGS_EXT => Some("VALIDATION_FLAGS_EXT"),
            Self::VI_SURFACE_CREATE_INFO_NN => Some("VI_SURFACE_CREATE_INFO_NN"),
            Self::PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES_EXT")
            }
            Self::IMAGE_VIEW_ASTC_DECODE_MODE_EXT => Some("IMAGE_VIEW_ASTC_DECODE_MODE_EXT"),
            Self::PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT")
            }
            Self::IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR => {
                Some("IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR")
            }
            Self::EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR => {
                Some("EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR")
            }
            Self::MEMORY_WIN32_HANDLE_PROPERTIES_KHR => Some("MEMORY_WIN32_HANDLE_PROPERTIES_KHR"),
            Self::MEMORY_GET_WIN32_HANDLE_INFO_KHR => Some("MEMORY_GET_WIN32_HANDLE_INFO_KHR"),
            Self::IMPORT_MEMORY_FD_INFO_KHR => Some("IMPORT_MEMORY_FD_INFO_KHR"),
            Self::MEMORY_FD_PROPERTIES_KHR => Some("MEMORY_FD_PROPERTIES_KHR"),
            Self::MEMORY_GET_FD_INFO_KHR => Some("MEMORY_GET_FD_INFO_KHR"),
            Self::WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR => {
                Some("WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR")
            }
            Self::IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR => {
                Some("IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR")
            }
            Self::EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR => {
                Some("EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR")
            }
            Self::D3D12_FENCE_SUBMIT_INFO_KHR => Some("D3D12_FENCE_SUBMIT_INFO_KHR"),
            Self::SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR => {
                Some("SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR")
            }
            Self::IMPORT_SEMAPHORE_FD_INFO_KHR => Some("IMPORT_SEMAPHORE_FD_INFO_KHR"),
            Self::SEMAPHORE_GET_FD_INFO_KHR => Some("SEMAPHORE_GET_FD_INFO_KHR"),
            Self::PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR => {
                Some("PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR")
            }
            Self::COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT => {
                Some("COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT")
            }
            Self::CONDITIONAL_RENDERING_BEGIN_INFO_EXT => {
                Some("CONDITIONAL_RENDERING_BEGIN_INFO_EXT")
            }
            Self::PRESENT_REGIONS_KHR => Some("PRESENT_REGIONS_KHR"),
            Self::PIPELINE_VIEWPORT_W_SCALING_STATE_CREATE_INFO_NV => {
                Some("PIPELINE_VIEWPORT_W_SCALING_STATE_CREATE_INFO_NV")
            }
            Self::SURFACE_CAPABILITIES_2_EXT => Some("SURFACE_CAPABILITIES_2_EXT"),
            Self::DISPLAY_POWER_INFO_EXT => Some("DISPLAY_POWER_INFO_EXT"),
            Self::DEVICE_EVENT_INFO_EXT => Some("DEVICE_EVENT_INFO_EXT"),
            Self::DISPLAY_EVENT_INFO_EXT => Some("DISPLAY_EVENT_INFO_EXT"),
            Self::SWAPCHAIN_COUNTER_CREATE_INFO_EXT => Some("SWAPCHAIN_COUNTER_CREATE_INFO_EXT"),
            Self::PRESENT_TIMES_INFO_GOOGLE => Some("PRESENT_TIMES_INFO_GOOGLE"),
            Self::PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX => {
                Some("PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX")
            }
            Self::PIPELINE_VIEWPORT_SWIZZLE_STATE_CREATE_INFO_NV => {
                Some("PIPELINE_VIEWPORT_SWIZZLE_STATE_CREATE_INFO_NV")
            }
            Self::PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT")
            }
            Self::PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT => {
                Some("PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT")
            }
            Self::PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT => {
                Some("PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT")
            }
            Self::PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT => {
                Some("PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT")
            }
            Self::HDR_METADATA_EXT => Some("HDR_METADATA_EXT"),
            Self::SHARED_PRESENT_SURFACE_CAPABILITIES_KHR => {
                Some("SHARED_PRESENT_SURFACE_CAPABILITIES_KHR")
            }
            Self::IMPORT_FENCE_WIN32_HANDLE_INFO_KHR => Some("IMPORT_FENCE_WIN32_HANDLE_INFO_KHR"),
            Self::EXPORT_FENCE_WIN32_HANDLE_INFO_KHR => Some("EXPORT_FENCE_WIN32_HANDLE_INFO_KHR"),
            Self::FENCE_GET_WIN32_HANDLE_INFO_KHR => Some("FENCE_GET_WIN32_HANDLE_INFO_KHR"),
            Self::IMPORT_FENCE_FD_INFO_KHR => Some("IMPORT_FENCE_FD_INFO_KHR"),
            Self::FENCE_GET_FD_INFO_KHR => Some("FENCE_GET_FD_INFO_KHR"),
            Self::PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR")
            }
            Self::PHYSICAL_DEVICE_PERFORMANCE_QUERY_PROPERTIES_KHR => {
                Some("PHYSICAL_DEVICE_PERFORMANCE_QUERY_PROPERTIES_KHR")
            }
            Self::QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR => {
                Some("QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR")
            }
            Self::PERFORMANCE_QUERY_SUBMIT_INFO_KHR => Some("PERFORMANCE_QUERY_SUBMIT_INFO_KHR"),
            Self::ACQUIRE_PROFILING_LOCK_INFO_KHR => Some("ACQUIRE_PROFILING_LOCK_INFO_KHR"),
            Self::PERFORMANCE_COUNTER_KHR => Some("PERFORMANCE_COUNTER_KHR"),
            Self::PERFORMANCE_COUNTER_DESCRIPTION_KHR => {
                Some("PERFORMANCE_COUNTER_DESCRIPTION_KHR")
            }
            Self::PHYSICAL_DEVICE_SURFACE_INFO_2_KHR => Some("PHYSICAL_DEVICE_SURFACE_INFO_2_KHR"),
            Self::SURFACE_CAPABILITIES_2_KHR => Some("SURFACE_CAPABILITIES_2_KHR"),
            Self::SURFACE_FORMAT_2_KHR => Some("SURFACE_FORMAT_2_KHR"),
            Self::DISPLAY_PROPERTIES_2_KHR => Some("DISPLAY_PROPERTIES_2_KHR"),
            Self::DISPLAY_PLANE_PROPERTIES_2_KHR => Some("DISPLAY_PLANE_PROPERTIES_2_KHR"),
            Self::DISPLAY_MODE_PROPERTIES_2_KHR => Some("DISPLAY_MODE_PROPERTIES_2_KHR"),
            Self::DISPLAY_PLANE_INFO_2_KHR => Some("DISPLAY_PLANE_INFO_2_KHR"),
            Self::DISPLAY_PLANE_CAPABILITIES_2_KHR => Some("DISPLAY_PLANE_CAPABILITIES_2_KHR"),
            Self::IOS_SURFACE_CREATE_INFO_MVK => Some("IOS_SURFACE_CREATE_INFO_MVK"),
            Self::MACOS_SURFACE_CREATE_INFO_MVK => Some("MACOS_SURFACE_CREATE_INFO_MVK"),
            Self::DEBUG_UTILS_OBJECT_NAME_INFO_EXT => Some("DEBUG_UTILS_OBJECT_NAME_INFO_EXT"),
            Self::DEBUG_UTILS_OBJECT_TAG_INFO_EXT => Some("DEBUG_UTILS_OBJECT_TAG_INFO_EXT"),
            Self::DEBUG_UTILS_LABEL_EXT => Some("DEBUG_UTILS_LABEL_EXT"),
            Self::DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT => {
                Some("DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT")
            }
            Self::DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT => {
                Some("DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT")
            }
            Self::ANDROID_HARDWARE_BUFFER_USAGE_ANDROID => {
                Some("ANDROID_HARDWARE_BUFFER_USAGE_ANDROID")
            }
            Self::ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID => {
                Some("ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID")
            }
            Self::ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID => {
                Some("ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID")
            }
            Self::IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID => {
                Some("IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID")
            }
            Self::MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID => {
                Some("MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID")
            }
            Self::EXTERNAL_FORMAT_ANDROID => Some("EXTERNAL_FORMAT_ANDROID"),
            Self::PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT")
            }
            Self::WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT => {
                Some("WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT")
            }
            Self::DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT => {
                Some("DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT")
            }
            Self::SAMPLE_LOCATIONS_INFO_EXT => Some("SAMPLE_LOCATIONS_INFO_EXT"),
            Self::RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT => {
                Some("RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT")
            }
            Self::PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT => {
                Some("PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT")
            }
            Self::MULTISAMPLE_PROPERTIES_EXT => Some("MULTISAMPLE_PROPERTIES_EXT"),
            Self::PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT")
            }
            Self::PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT => {
                Some("PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT")
            }
            Self::PIPELINE_COVERAGE_TO_COLOR_STATE_CREATE_INFO_NV => {
                Some("PIPELINE_COVERAGE_TO_COLOR_STATE_CREATE_INFO_NV")
            }
            Self::WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR => {
                Some("WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR")
            }
            Self::ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR => {
                Some("ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR")
            }
            Self::ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR => {
                Some("ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR")
            }
            Self::ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR => {
                Some("ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR")
            }
            Self::ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR => {
                Some("ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR")
            }
            Self::ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR => {
                Some("ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR")
            }
            Self::ACCELERATION_STRUCTURE_GEOMETRY_KHR => {
                Some("ACCELERATION_STRUCTURE_GEOMETRY_KHR")
            }
            Self::ACCELERATION_STRUCTURE_VERSION_INFO_KHR => {
                Some("ACCELERATION_STRUCTURE_VERSION_INFO_KHR")
            }
            Self::COPY_ACCELERATION_STRUCTURE_INFO_KHR => {
                Some("COPY_ACCELERATION_STRUCTURE_INFO_KHR")
            }
            Self::COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR => {
                Some("COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR")
            }
            Self::COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR => {
                Some("COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR")
            }
            Self::PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR")
            }
            Self::PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR => {
                Some("PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR")
            }
            Self::ACCELERATION_STRUCTURE_CREATE_INFO_KHR => {
                Some("ACCELERATION_STRUCTURE_CREATE_INFO_KHR")
            }
            Self::ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR => {
                Some("ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR")
            }
            Self::PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR")
            }
            Self::PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR => {
                Some("PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR")
            }
            Self::RAY_TRACING_PIPELINE_CREATE_INFO_KHR => {
                Some("RAY_TRACING_PIPELINE_CREATE_INFO_KHR")
            }
            Self::RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR => {
                Some("RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR")
            }
            Self::RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR => {
                Some("RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR")
            }
            Self::PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR")
            }
            Self::PIPELINE_COVERAGE_MODULATION_STATE_CREATE_INFO_NV => {
                Some("PIPELINE_COVERAGE_MODULATION_STATE_CREATE_INFO_NV")
            }
            Self::PHYSICAL_DEVICE_SHADER_SM_BUILTINS_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_SHADER_SM_BUILTINS_FEATURES_NV")
            }
            Self::PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV => {
                Some("PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV")
            }
            Self::DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT => {
                Some("DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT")
            }
            Self::PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT => {
                Some("PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT")
            }
            Self::IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT => {
                Some("IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT")
            }
            Self::IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT => {
                Some("IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT")
            }
            Self::IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT => {
                Some("IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT")
            }
            Self::VALIDATION_CACHE_CREATE_INFO_EXT => Some("VALIDATION_CACHE_CREATE_INFO_EXT"),
            Self::SHADER_MODULE_VALIDATION_CACHE_CREATE_INFO_EXT => {
                Some("SHADER_MODULE_VALIDATION_CACHE_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR")
            }
            Self::PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR => {
                Some("PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR")
            }
            Self::PIPELINE_VIEWPORT_SHADING_RATE_IMAGE_STATE_CREATE_INFO_NV => {
                Some("PIPELINE_VIEWPORT_SHADING_RATE_IMAGE_STATE_CREATE_INFO_NV")
            }
            Self::PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV")
            }
            Self::PHYSICAL_DEVICE_SHADING_RATE_IMAGE_PROPERTIES_NV => {
                Some("PHYSICAL_DEVICE_SHADING_RATE_IMAGE_PROPERTIES_NV")
            }
            Self::PIPELINE_VIEWPORT_COARSE_SAMPLE_ORDER_STATE_CREATE_INFO_NV => {
                Some("PIPELINE_VIEWPORT_COARSE_SAMPLE_ORDER_STATE_CREATE_INFO_NV")
            }
            Self::RAY_TRACING_PIPELINE_CREATE_INFO_NV => {
                Some("RAY_TRACING_PIPELINE_CREATE_INFO_NV")
            }
            Self::ACCELERATION_STRUCTURE_CREATE_INFO_NV => {
                Some("ACCELERATION_STRUCTURE_CREATE_INFO_NV")
            }
            Self::GEOMETRY_NV => Some("GEOMETRY_NV"),
            Self::GEOMETRY_TRIANGLES_NV => Some("GEOMETRY_TRIANGLES_NV"),
            Self::GEOMETRY_AABB_NV => Some("GEOMETRY_AABB_NV"),
            Self::BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV => {
                Some("BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV")
            }
            Self::WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV => {
                Some("WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV")
            }
            Self::ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV => {
                Some("ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV")
            }
            Self::PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV => {
                Some("PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV")
            }
            Self::RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV => {
                Some("RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV")
            }
            Self::ACCELERATION_STRUCTURE_INFO_NV => Some("ACCELERATION_STRUCTURE_INFO_NV"),
            Self::PHYSICAL_DEVICE_REPRESENTATIVE_FRAGMENT_TEST_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_REPRESENTATIVE_FRAGMENT_TEST_FEATURES_NV")
            }
            Self::PIPELINE_REPRESENTATIVE_FRAGMENT_TEST_STATE_CREATE_INFO_NV => {
                Some("PIPELINE_REPRESENTATIVE_FRAGMENT_TEST_STATE_CREATE_INFO_NV")
            }
            Self::PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT => {
                Some("PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT")
            }
            Self::FILTER_CUBIC_IMAGE_VIEW_IMAGE_FORMAT_PROPERTIES_EXT => {
                Some("FILTER_CUBIC_IMAGE_VIEW_IMAGE_FORMAT_PROPERTIES_EXT")
            }
            Self::DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT => {
                Some("DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT")
            }
            Self::IMPORT_MEMORY_HOST_POINTER_INFO_EXT => {
                Some("IMPORT_MEMORY_HOST_POINTER_INFO_EXT")
            }
            Self::MEMORY_HOST_POINTER_PROPERTIES_EXT => Some("MEMORY_HOST_POINTER_PROPERTIES_EXT"),
            Self::PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT")
            }
            Self::PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR")
            }
            Self::PIPELINE_COMPILER_CONTROL_CREATE_INFO_AMD => {
                Some("PIPELINE_COMPILER_CONTROL_CREATE_INFO_AMD")
            }
            Self::CALIBRATED_TIMESTAMP_INFO_EXT => Some("CALIBRATED_TIMESTAMP_INFO_EXT"),
            Self::PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD => {
                Some("PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD")
            }
            Self::DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD => {
                Some("DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD")
            }
            Self::PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT")
            }
            Self::PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT => {
                Some("PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT")
            }
            Self::PRESENT_FRAME_TOKEN_GGP => Some("PRESENT_FRAME_TOKEN_GGP"),
            Self::PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT => {
                Some("PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV")
            }
            Self::PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV")
            }
            Self::PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV => {
                Some("PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV")
            }
            Self::PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV")
            }
            Self::PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV")
            }
            Self::PIPELINE_VIEWPORT_EXCLUSIVE_SCISSOR_STATE_CREATE_INFO_NV => {
                Some("PIPELINE_VIEWPORT_EXCLUSIVE_SCISSOR_STATE_CREATE_INFO_NV")
            }
            Self::PHYSICAL_DEVICE_EXCLUSIVE_SCISSOR_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_EXCLUSIVE_SCISSOR_FEATURES_NV")
            }
            Self::CHECKPOINT_DATA_NV => Some("CHECKPOINT_DATA_NV"),
            Self::QUEUE_FAMILY_CHECKPOINT_PROPERTIES_NV => {
                Some("QUEUE_FAMILY_CHECKPOINT_PROPERTIES_NV")
            }
            Self::PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL => {
                Some("PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL")
            }
            Self::QUERY_POOL_PERFORMANCE_QUERY_CREATE_INFO_INTEL => {
                Some("QUERY_POOL_PERFORMANCE_QUERY_CREATE_INFO_INTEL")
            }
            Self::INITIALIZE_PERFORMANCE_API_INFO_INTEL => {
                Some("INITIALIZE_PERFORMANCE_API_INFO_INTEL")
            }
            Self::PERFORMANCE_MARKER_INFO_INTEL => Some("PERFORMANCE_MARKER_INFO_INTEL"),
            Self::PERFORMANCE_STREAM_MARKER_INFO_INTEL => {
                Some("PERFORMANCE_STREAM_MARKER_INFO_INTEL")
            }
            Self::PERFORMANCE_OVERRIDE_INFO_INTEL => Some("PERFORMANCE_OVERRIDE_INFO_INTEL"),
            Self::PERFORMANCE_CONFIGURATION_ACQUIRE_INFO_INTEL => {
                Some("PERFORMANCE_CONFIGURATION_ACQUIRE_INFO_INTEL")
            }
            Self::PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT")
            }
            Self::DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD => {
                Some("DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD")
            }
            Self::SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD => {
                Some("SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD")
            }
            Self::IMAGEPIPE_SURFACE_CREATE_INFO_FUCHSIA => {
                Some("IMAGEPIPE_SURFACE_CREATE_INFO_FUCHSIA")
            }
            Self::PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES_KHR")
            }
            Self::METAL_SURFACE_CREATE_INFO_EXT => Some("METAL_SURFACE_CREATE_INFO_EXT"),
            Self::PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT")
            }
            Self::RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT => {
                Some("RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT")
            }
            Self::PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT => {
                Some("PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT")
            }
            Self::FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR => {
                Some("FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR")
            }
            Self::PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR => {
                Some("PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR")
            }
            Self::PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR => {
                Some("PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR")
            }
            Self::PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR")
            }
            Self::PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR => {
                Some("PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR")
            }
            Self::PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD => {
                Some("PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD")
            }
            Self::PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD => {
                Some("PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD")
            }
            Self::PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT")
            }
            Self::PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT")
            }
            Self::MEMORY_PRIORITY_ALLOCATE_INFO_EXT => Some("MEMORY_PRIORITY_ALLOCATE_INFO_EXT"),
            Self::SURFACE_PROTECTED_CAPABILITIES_KHR => Some("SURFACE_PROTECTED_CAPABILITIES_KHR"),
            Self::PHYSICAL_DEVICE_DEDICATED_ALLOCATION_IMAGE_ALIASING_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_DEDICATED_ALLOCATION_IMAGE_ALIASING_FEATURES_NV")
            }
            Self::PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT")
            }
            Self::BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT => {
                Some("BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT")
            }
            Self::VALIDATION_FEATURES_EXT => Some("VALIDATION_FEATURES_EXT"),
            Self::PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV")
            }
            Self::COOPERATIVE_MATRIX_PROPERTIES_NV => Some("COOPERATIVE_MATRIX_PROPERTIES_NV"),
            Self::PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_NV => {
                Some("PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_NV")
            }
            Self::PHYSICAL_DEVICE_COVERAGE_REDUCTION_MODE_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_COVERAGE_REDUCTION_MODE_FEATURES_NV")
            }
            Self::PIPELINE_COVERAGE_REDUCTION_STATE_CREATE_INFO_NV => {
                Some("PIPELINE_COVERAGE_REDUCTION_STATE_CREATE_INFO_NV")
            }
            Self::FRAMEBUFFER_MIXED_SAMPLES_COMBINATION_NV => {
                Some("FRAMEBUFFER_MIXED_SAMPLES_COMBINATION_NV")
            }
            Self::PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT")
            }
            Self::SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT => {
                Some("SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT")
            }
            Self::SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT => {
                Some("SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT")
            }
            Self::SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT => {
                Some("SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT")
            }
            Self::HEADLESS_SURFACE_CREATE_INFO_EXT => Some("HEADLESS_SURFACE_CREATE_INFO_EXT"),
            Self::PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT")
            }
            Self::PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT => {
                Some("PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT")
            }
            Self::PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR")
            }
            Self::PIPELINE_INFO_KHR => Some("PIPELINE_INFO_KHR"),
            Self::PIPELINE_EXECUTABLE_PROPERTIES_KHR => Some("PIPELINE_EXECUTABLE_PROPERTIES_KHR"),
            Self::PIPELINE_EXECUTABLE_INFO_KHR => Some("PIPELINE_EXECUTABLE_INFO_KHR"),
            Self::PIPELINE_EXECUTABLE_STATISTIC_KHR => Some("PIPELINE_EXECUTABLE_STATISTIC_KHR"),
            Self::PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR => {
                Some("PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR")
            }
            Self::PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV => {
                Some("PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV")
            }
            Self::GRAPHICS_SHADER_GROUP_CREATE_INFO_NV => {
                Some("GRAPHICS_SHADER_GROUP_CREATE_INFO_NV")
            }
            Self::GRAPHICS_PIPELINE_SHADER_GROUPS_CREATE_INFO_NV => {
                Some("GRAPHICS_PIPELINE_SHADER_GROUPS_CREATE_INFO_NV")
            }
            Self::INDIRECT_COMMANDS_LAYOUT_TOKEN_NV => Some("INDIRECT_COMMANDS_LAYOUT_TOKEN_NV"),
            Self::INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV => {
                Some("INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV")
            }
            Self::GENERATED_COMMANDS_INFO_NV => Some("GENERATED_COMMANDS_INFO_NV"),
            Self::GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV => {
                Some("GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV")
            }
            Self::PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV")
            }
            Self::PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES_EXT")
            }
            Self::COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM => {
                Some("COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM")
            }
            Self::RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM => {
                Some("RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM")
            }
            Self::PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT")
            }
            Self::DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT => {
                Some("DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT")
            }
            Self::DEVICE_MEMORY_REPORT_CALLBACK_DATA_EXT => {
                Some("DEVICE_MEMORY_REPORT_CALLBACK_DATA_EXT")
            }
            Self::PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT")
            }
            Self::SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT => {
                Some("SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT")
            }
            Self::PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT")
            }
            Self::PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT")
            }
            Self::PIPELINE_LIBRARY_CREATE_INFO_KHR => Some("PIPELINE_LIBRARY_CREATE_INFO_KHR"),
            Self::PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES_EXT")
            }
            Self::DEVICE_PRIVATE_DATA_CREATE_INFO_EXT => {
                Some("DEVICE_PRIVATE_DATA_CREATE_INFO_EXT")
            }
            Self::PRIVATE_DATA_SLOT_CREATE_INFO_EXT => Some("PRIVATE_DATA_SLOT_CREATE_INFO_EXT"),
            Self::PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV")
            }
            Self::DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV => {
                Some("DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV")
            }
            Self::RESERVED_QCOM => Some("RESERVED_QCOM"),
            Self::PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES_KHR")
            }
            Self::PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_PROPERTIES_NV => {
                Some("PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_PROPERTIES_NV")
            }
            Self::PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_FEATURES_NV => {
                Some("PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_FEATURES_NV")
            }
            Self::PIPELINE_FRAGMENT_SHADING_RATE_ENUM_STATE_CREATE_INFO_NV => {
                Some("PIPELINE_FRAGMENT_SHADING_RATE_ENUM_STATE_CREATE_INFO_NV")
            }
            Self::PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_PROPERTIES_EXT => {
                Some("PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_PROPERTIES_EXT")
            }
            Self::COPY_COMMAND_TRANSFORM_INFO_QCOM => Some("COPY_COMMAND_TRANSFORM_INFO_QCOM"),
            Self::PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT")
            }
            Self::PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR => {
                Some("PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR")
            }
            Self::COPY_BUFFER_INFO_2_KHR => Some("COPY_BUFFER_INFO_2_KHR"),
            Self::COPY_IMAGE_INFO_2_KHR => Some("COPY_IMAGE_INFO_2_KHR"),
            Self::COPY_BUFFER_TO_IMAGE_INFO_2_KHR => Some("COPY_BUFFER_TO_IMAGE_INFO_2_KHR"),
            Self::COPY_IMAGE_TO_BUFFER_INFO_2_KHR => Some("COPY_IMAGE_TO_BUFFER_INFO_2_KHR"),
            Self::BLIT_IMAGE_INFO_2_KHR => Some("BLIT_IMAGE_INFO_2_KHR"),
            Self::RESOLVE_IMAGE_INFO_2_KHR => Some("RESOLVE_IMAGE_INFO_2_KHR"),
            Self::BUFFER_COPY_2_KHR => Some("BUFFER_COPY_2_KHR"),
            Self::IMAGE_COPY_2_KHR => Some("IMAGE_COPY_2_KHR"),
            Self::IMAGE_BLIT_2_KHR => Some("IMAGE_BLIT_2_KHR"),
            Self::BUFFER_IMAGE_COPY_2_KHR => Some("BUFFER_IMAGE_COPY_2_KHR"),
            Self::IMAGE_RESOLVE_2_KHR => Some("IMAGE_RESOLVE_2_KHR"),
            Self::PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT => {
                Some("PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT")
            }
            Self::DIRECTFB_SURFACE_CREATE_INFO_EXT => Some("DIRECTFB_SURFACE_CREATE_INFO_EXT"),
            Self::PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_VALVE => {
                Some("PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_VALVE")
            }
            Self::MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE => {
                Some("MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE")
            }
            Self::SCREEN_SURFACE_CREATE_INFO_QNX => Some("SCREEN_SURFACE_CREATE_INFO_QNX"),
            Self::PHYSICAL_DEVICE_SUBGROUP_PROPERTIES => {
                Some("PHYSICAL_DEVICE_SUBGROUP_PROPERTIES")
            }
            Self::BIND_BUFFER_MEMORY_INFO => Some("BIND_BUFFER_MEMORY_INFO"),
            Self::BIND_IMAGE_MEMORY_INFO => Some("BIND_IMAGE_MEMORY_INFO"),
            Self::PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES => {
                Some("PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES")
            }
            Self::MEMORY_DEDICATED_REQUIREMENTS => Some("MEMORY_DEDICATED_REQUIREMENTS"),
            Self::MEMORY_DEDICATED_ALLOCATE_INFO => Some("MEMORY_DEDICATED_ALLOCATE_INFO"),
            Self::MEMORY_ALLOCATE_FLAGS_INFO => Some("MEMORY_ALLOCATE_FLAGS_INFO"),
            Self::DEVICE_GROUP_RENDER_PASS_BEGIN_INFO => {
                Some("DEVICE_GROUP_RENDER_PASS_BEGIN_INFO")
            }
            Self::DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO => {
                Some("DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO")
            }
            Self::DEVICE_GROUP_SUBMIT_INFO => Some("DEVICE_GROUP_SUBMIT_INFO"),
            Self::DEVICE_GROUP_BIND_SPARSE_INFO => Some("DEVICE_GROUP_BIND_SPARSE_INFO"),
            Self::BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO => {
                Some("BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO")
            }
            Self::BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO => {
                Some("BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO")
            }
            Self::PHYSICAL_DEVICE_GROUP_PROPERTIES => Some("PHYSICAL_DEVICE_GROUP_PROPERTIES"),
            Self::DEVICE_GROUP_DEVICE_CREATE_INFO => Some("DEVICE_GROUP_DEVICE_CREATE_INFO"),
            Self::BUFFER_MEMORY_REQUIREMENTS_INFO_2 => Some("BUFFER_MEMORY_REQUIREMENTS_INFO_2"),
            Self::IMAGE_MEMORY_REQUIREMENTS_INFO_2 => Some("IMAGE_MEMORY_REQUIREMENTS_INFO_2"),
            Self::IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2 => {
                Some("IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2")
            }
            Self::MEMORY_REQUIREMENTS_2 => Some("MEMORY_REQUIREMENTS_2"),
            Self::SPARSE_IMAGE_MEMORY_REQUIREMENTS_2 => Some("SPARSE_IMAGE_MEMORY_REQUIREMENTS_2"),
            Self::PHYSICAL_DEVICE_FEATURES_2 => Some("PHYSICAL_DEVICE_FEATURES_2"),
            Self::PHYSICAL_DEVICE_PROPERTIES_2 => Some("PHYSICAL_DEVICE_PROPERTIES_2"),
            Self::FORMAT_PROPERTIES_2 => Some("FORMAT_PROPERTIES_2"),
            Self::IMAGE_FORMAT_PROPERTIES_2 => Some("IMAGE_FORMAT_PROPERTIES_2"),
            Self::PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2 => {
                Some("PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2")
            }
            Self::QUEUE_FAMILY_PROPERTIES_2 => Some("QUEUE_FAMILY_PROPERTIES_2"),
            Self::PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 => {
                Some("PHYSICAL_DEVICE_MEMORY_PROPERTIES_2")
            }
            Self::SPARSE_IMAGE_FORMAT_PROPERTIES_2 => Some("SPARSE_IMAGE_FORMAT_PROPERTIES_2"),
            Self::PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2 => {
                Some("PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2")
            }
            Self::PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES => {
                Some("PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES")
            }
            Self::RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO => {
                Some("RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO")
            }
            Self::IMAGE_VIEW_USAGE_CREATE_INFO => Some("IMAGE_VIEW_USAGE_CREATE_INFO"),
            Self::PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO => {
                Some("PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO")
            }
            Self::RENDER_PASS_MULTIVIEW_CREATE_INFO => Some("RENDER_PASS_MULTIVIEW_CREATE_INFO"),
            Self::PHYSICAL_DEVICE_MULTIVIEW_FEATURES => Some("PHYSICAL_DEVICE_MULTIVIEW_FEATURES"),
            Self::PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES => {
                Some("PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES")
            }
            Self::PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES => {
                Some("PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES")
            }
            Self::PROTECTED_SUBMIT_INFO => Some("PROTECTED_SUBMIT_INFO"),
            Self::PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES => {
                Some("PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES")
            }
            Self::PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES => {
                Some("PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES")
            }
            Self::DEVICE_QUEUE_INFO_2 => Some("DEVICE_QUEUE_INFO_2"),
            Self::SAMPLER_YCBCR_CONVERSION_CREATE_INFO => {
                Some("SAMPLER_YCBCR_CONVERSION_CREATE_INFO")
            }
            Self::SAMPLER_YCBCR_CONVERSION_INFO => Some("SAMPLER_YCBCR_CONVERSION_INFO"),
            Self::BIND_IMAGE_PLANE_MEMORY_INFO => Some("BIND_IMAGE_PLANE_MEMORY_INFO"),
            Self::IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO => {
                Some("IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO")
            }
            Self::PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES => {
                Some("PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES")
            }
            Self::SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES => {
                Some("SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES")
            }
            Self::DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO => {
                Some("DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO")
            }
            Self::PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO => {
                Some("PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO")
            }
            Self::EXTERNAL_IMAGE_FORMAT_PROPERTIES => Some("EXTERNAL_IMAGE_FORMAT_PROPERTIES"),
            Self::PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO => {
                Some("PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO")
            }
            Self::EXTERNAL_BUFFER_PROPERTIES => Some("EXTERNAL_BUFFER_PROPERTIES"),
            Self::PHYSICAL_DEVICE_ID_PROPERTIES => Some("PHYSICAL_DEVICE_ID_PROPERTIES"),
            Self::EXTERNAL_MEMORY_BUFFER_CREATE_INFO => Some("EXTERNAL_MEMORY_BUFFER_CREATE_INFO"),
            Self::EXTERNAL_MEMORY_IMAGE_CREATE_INFO => Some("EXTERNAL_MEMORY_IMAGE_CREATE_INFO"),
            Self::EXPORT_MEMORY_ALLOCATE_INFO => Some("EXPORT_MEMORY_ALLOCATE_INFO"),
            Self::PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO => {
                Some("PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO")
            }
            Self::EXTERNAL_FENCE_PROPERTIES => Some("EXTERNAL_FENCE_PROPERTIES"),
            Self::EXPORT_FENCE_CREATE_INFO => Some("EXPORT_FENCE_CREATE_INFO"),
            Self::EXPORT_SEMAPHORE_CREATE_INFO => Some("EXPORT_SEMAPHORE_CREATE_INFO"),
            Self::PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO => {
                Some("PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO")
            }
            Self::EXTERNAL_SEMAPHORE_PROPERTIES => Some("EXTERNAL_SEMAPHORE_PROPERTIES"),
            Self::PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES => {
                Some("PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES")
            }
            Self::DESCRIPTOR_SET_LAYOUT_SUPPORT => Some("DESCRIPTOR_SET_LAYOUT_SUPPORT"),
            Self::PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES => {
                Some("PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES")
            }
            Self::PHYSICAL_DEVICE_VULKAN_1_1_FEATURES => {
                Some("PHYSICAL_DEVICE_VULKAN_1_1_FEATURES")
            }
            Self::PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES => {
                Some("PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES")
            }
            Self::PHYSICAL_DEVICE_VULKAN_1_2_FEATURES => {
                Some("PHYSICAL_DEVICE_VULKAN_1_2_FEATURES")
            }
            Self::PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES => {
                Some("PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES")
            }
            Self::IMAGE_FORMAT_LIST_CREATE_INFO => Some("IMAGE_FORMAT_LIST_CREATE_INFO"),
            Self::ATTACHMENT_DESCRIPTION_2 => Some("ATTACHMENT_DESCRIPTION_2"),
            Self::ATTACHMENT_REFERENCE_2 => Some("ATTACHMENT_REFERENCE_2"),
            Self::SUBPASS_DESCRIPTION_2 => Some("SUBPASS_DESCRIPTION_2"),
            Self::SUBPASS_DEPENDENCY_2 => Some("SUBPASS_DEPENDENCY_2"),
            Self::RENDER_PASS_CREATE_INFO_2 => Some("RENDER_PASS_CREATE_INFO_2"),
            Self::SUBPASS_BEGIN_INFO => Some("SUBPASS_BEGIN_INFO"),
            Self::SUBPASS_END_INFO => Some("SUBPASS_END_INFO"),
            Self::PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES => {
                Some("PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES")
            }
            Self::PHYSICAL_DEVICE_DRIVER_PROPERTIES => Some("PHYSICAL_DEVICE_DRIVER_PROPERTIES"),
            Self::PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES => {
                Some("PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES")
            }
            Self::PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES => {
                Some("PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES")
            }
            Self::PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES => {
                Some("PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES")
            }
            Self::DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO => {
                Some("DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO")
            }
            Self::PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES => {
                Some("PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES")
            }
            Self::PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES => {
                Some("PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES")
            }
            Self::DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO => {
                Some("DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO")
            }
            Self::DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT => {
                Some("DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT")
            }
            Self::PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES => {
                Some("PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES")
            }
            Self::SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE => {
                Some("SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE")
            }
            Self::PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES => {
                Some("PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES")
            }
            Self::IMAGE_STENCIL_USAGE_CREATE_INFO => Some("IMAGE_STENCIL_USAGE_CREATE_INFO"),
            Self::PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES => {
                Some("PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES")
            }
            Self::SAMPLER_REDUCTION_MODE_CREATE_INFO => Some("SAMPLER_REDUCTION_MODE_CREATE_INFO"),
            Self::PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES => {
                Some("PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES")
            }
            Self::PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES => {
                Some("PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES")
            }
            Self::FRAMEBUFFER_ATTACHMENTS_CREATE_INFO => {
                Some("FRAMEBUFFER_ATTACHMENTS_CREATE_INFO")
            }
            Self::FRAMEBUFFER_ATTACHMENT_IMAGE_INFO => Some("FRAMEBUFFER_ATTACHMENT_IMAGE_INFO"),
            Self::RENDER_PASS_ATTACHMENT_BEGIN_INFO => Some("RENDER_PASS_ATTACHMENT_BEGIN_INFO"),
            Self::PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES => {
                Some("PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES")
            }
            Self::PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES => {
                Some("PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES")
            }
            Self::PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES => {
                Some("PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES")
            }
            Self::ATTACHMENT_REFERENCE_STENCIL_LAYOUT => {
                Some("ATTACHMENT_REFERENCE_STENCIL_LAYOUT")
            }
            Self::ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT => {
                Some("ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT")
            }
            Self::PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES => {
                Some("PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES")
            }
            Self::PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES => {
                Some("PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES")
            }
            Self::PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES => {
                Some("PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES")
            }
            Self::SEMAPHORE_TYPE_CREATE_INFO => Some("SEMAPHORE_TYPE_CREATE_INFO"),
            Self::TIMELINE_SEMAPHORE_SUBMIT_INFO => Some("TIMELINE_SEMAPHORE_SUBMIT_INFO"),
            Self::SEMAPHORE_WAIT_INFO => Some("SEMAPHORE_WAIT_INFO"),
            Self::SEMAPHORE_SIGNAL_INFO => Some("SEMAPHORE_SIGNAL_INFO"),
            Self::PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES => {
                Some("PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES")
            }
            Self::BUFFER_DEVICE_ADDRESS_INFO => Some("BUFFER_DEVICE_ADDRESS_INFO"),
            Self::BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO => {
                Some("BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO")
            }
            Self::MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO => {
                Some("MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO")
            }
            Self::DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO => {
                Some("DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO")
            }
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SubgroupFeatureFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (SubgroupFeatureFlags::BASIC.0, "BASIC"),
            (SubgroupFeatureFlags::VOTE.0, "VOTE"),
            (SubgroupFeatureFlags::ARITHMETIC.0, "ARITHMETIC"),
            (SubgroupFeatureFlags::BALLOT.0, "BALLOT"),
            (SubgroupFeatureFlags::SHUFFLE.0, "SHUFFLE"),
            (SubgroupFeatureFlags::SHUFFLE_RELATIVE.0, "SHUFFLE_RELATIVE"),
            (SubgroupFeatureFlags::CLUSTERED.0, "CLUSTERED"),
            (SubgroupFeatureFlags::QUAD.0, "QUAD"),
            (SubgroupFeatureFlags::PARTITIONED_NV.0, "PARTITIONED_NV"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SubpassContents {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::INLINE => Some("INLINE"),
            Self::SECONDARY_COMMAND_BUFFERS => Some("SECONDARY_COMMAND_BUFFERS"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for SubpassDescriptionFlags {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                SubpassDescriptionFlags::PER_VIEW_ATTRIBUTES_NVX.0,
                "PER_VIEW_ATTRIBUTES_NVX",
            ),
            (
                SubpassDescriptionFlags::PER_VIEW_POSITION_X_ONLY_NVX.0,
                "PER_VIEW_POSITION_X_ONLY_NVX",
            ),
            (
                SubpassDescriptionFlags::FRAGMENT_REGION_QCOM.0,
                "FRAGMENT_REGION_QCOM",
            ),
            (
                SubpassDescriptionFlags::SHADER_RESOLVE_QCOM.0,
                "SHADER_RESOLVE_QCOM",
            ),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SurfaceCounterFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(SurfaceCounterFlagsEXT::VBLANK.0, "VBLANK")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SurfaceTransformFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (SurfaceTransformFlagsKHR::IDENTITY.0, "IDENTITY"),
            (SurfaceTransformFlagsKHR::ROTATE_90.0, "ROTATE_90"),
            (SurfaceTransformFlagsKHR::ROTATE_180.0, "ROTATE_180"),
            (SurfaceTransformFlagsKHR::ROTATE_270.0, "ROTATE_270"),
            (
                SurfaceTransformFlagsKHR::HORIZONTAL_MIRROR.0,
                "HORIZONTAL_MIRROR",
            ),
            (
                SurfaceTransformFlagsKHR::HORIZONTAL_MIRROR_ROTATE_90.0,
                "HORIZONTAL_MIRROR_ROTATE_90",
            ),
            (
                SurfaceTransformFlagsKHR::HORIZONTAL_MIRROR_ROTATE_180.0,
                "HORIZONTAL_MIRROR_ROTATE_180",
            ),
            (
                SurfaceTransformFlagsKHR::HORIZONTAL_MIRROR_ROTATE_270.0,
                "HORIZONTAL_MIRROR_ROTATE_270",
            ),
            (SurfaceTransformFlagsKHR::INHERIT.0, "INHERIT"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SwapchainCreateFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (
                SwapchainCreateFlagsKHR::SPLIT_INSTANCE_BIND_REGIONS.0,
                "SPLIT_INSTANCE_BIND_REGIONS",
            ),
            (SwapchainCreateFlagsKHR::PROTECTED.0, "PROTECTED"),
            (SwapchainCreateFlagsKHR::MUTABLE_FORMAT.0, "MUTABLE_FORMAT"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SwapchainImageUsageFlagsANDROID {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[(SwapchainImageUsageFlagsANDROID::SHARED.0, "SHARED")];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for SystemAllocationScope {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::COMMAND => Some("COMMAND"),
            Self::OBJECT => Some("OBJECT"),
            Self::CACHE => Some("CACHE"),
            Self::DEVICE => Some("DEVICE"),
            Self::INSTANCE => Some("INSTANCE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for TessellationDomainOrigin {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::UPPER_LEFT => Some("UPPER_LEFT"),
            Self::LOWER_LEFT => Some("LOWER_LEFT"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for TimeDomainEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::DEVICE => Some("DEVICE"),
            Self::CLOCK_MONOTONIC => Some("CLOCK_MONOTONIC"),
            Self::CLOCK_MONOTONIC_RAW => Some("CLOCK_MONOTONIC_RAW"),
            Self::QUERY_PERFORMANCE_COUNTER => Some("QUERY_PERFORMANCE_COUNTER"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ToolPurposeFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[
            (ToolPurposeFlagsEXT::VALIDATION.0, "VALIDATION"),
            (ToolPurposeFlagsEXT::PROFILING.0, "PROFILING"),
            (ToolPurposeFlagsEXT::TRACING.0, "TRACING"),
            (
                ToolPurposeFlagsEXT::ADDITIONAL_FEATURES.0,
                "ADDITIONAL_FEATURES",
            ),
            (
                ToolPurposeFlagsEXT::MODIFYING_FEATURES.0,
                "MODIFYING_FEATURES",
            ),
            (ToolPurposeFlagsEXT::DEBUG_REPORTING.0, "DEBUG_REPORTING"),
            (ToolPurposeFlagsEXT::DEBUG_MARKERS.0, "DEBUG_MARKERS"),
        ];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ValidationCacheCreateFlagsEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ValidationCacheHeaderVersionEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::ONE => Some("ONE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ValidationCheckEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::ALL => Some("ALL"),
            Self::SHADERS => Some("SHADERS"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ValidationFeatureDisableEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::ALL => Some("ALL"),
            Self::SHADERS => Some("SHADERS"),
            Self::THREAD_SAFETY => Some("THREAD_SAFETY"),
            Self::API_PARAMETERS => Some("API_PARAMETERS"),
            Self::OBJECT_LIFETIMES => Some("OBJECT_LIFETIMES"),
            Self::CORE_CHECKS => Some("CORE_CHECKS"),
            Self::UNIQUE_HANDLES => Some("UNIQUE_HANDLES"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ValidationFeatureEnableEXT {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::GPU_ASSISTED => Some("GPU_ASSISTED"),
            Self::GPU_ASSISTED_RESERVE_BINDING_SLOT => Some("GPU_ASSISTED_RESERVE_BINDING_SLOT"),
            Self::BEST_PRACTICES => Some("BEST_PRACTICES"),
            Self::DEBUG_PRINTF => Some("DEBUG_PRINTF"),
            Self::SYNCHRONIZATION_VALIDATION => Some("SYNCHRONIZATION_VALIDATION"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for VendorId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::VIV => Some("VIV"),
            Self::VSI => Some("VSI"),
            Self::KAZAN => Some("KAZAN"),
            Self::CODEPLAY => Some("CODEPLAY"),
            Self::MESA => Some("MESA"),
            Self::POCL => Some("POCL"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for VertexInputRate {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::VERTEX => Some("VERTEX"),
            Self::INSTANCE => Some("INSTANCE"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for ViSurfaceCreateFlagsNN {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for ViewportCoordinateSwizzleNV {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            Self::POSITIVE_X => Some("POSITIVE_X"),
            Self::NEGATIVE_X => Some("NEGATIVE_X"),
            Self::POSITIVE_Y => Some("POSITIVE_Y"),
            Self::NEGATIVE_Y => Some("NEGATIVE_Y"),
            Self::POSITIVE_Z => Some("POSITIVE_Z"),
            Self::NEGATIVE_Z => Some("NEGATIVE_Z"),
            Self::POSITIVE_W => Some("POSITIVE_W"),
            Self::NEGATIVE_W => Some("NEGATIVE_W"),
            _ => None,
        };
        if let Some(x) = name {
            f.write_str(x)
        } else {
            self.0.fmt(f)
        }
    }
}
impl fmt::Debug for WaylandSurfaceCreateFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for Win32SurfaceCreateFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for XcbSurfaceCreateFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
impl fmt::Debug for XlibSurfaceCreateFlagsKHR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        const KNOWN: &[(Flags, &str)] = &[];
        debug_flags(f, KNOWN, self.0)
    }
}
