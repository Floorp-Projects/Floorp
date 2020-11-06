use crate::bindings as br;
use crate::{compiler, spirv, ErrorCode};

use std::collections::BTreeMap;
use std::ffi::CStr;
use std::marker::PhantomData;
use std::ptr;
use std::u8;

/// A MSL target.
#[derive(Debug, Clone)]
pub enum Target {}

pub struct TargetData {
    vertex_attribute_overrides: Vec<br::spirv_cross::MSLVertexAttr>,
    resource_binding_overrides: Vec<br::spirv_cross::MSLResourceBinding>,
    const_samplers: Vec<br::ScMslConstSamplerMapping>,
}

impl spirv::Target for Target {
    type Data = TargetData;
}

/// Location of a vertex attribute to override
#[derive(Debug, Clone, Hash, Eq, PartialEq, Ord, PartialOrd)]
pub struct VertexAttributeLocation(pub u32);

/// Format of the vertex attribute
#[derive(Debug, Clone, Hash, Eq, PartialEq, Ord, PartialOrd)]
pub enum Format {
    Other,
    Uint8,
    Uint16,
}

impl Format {
    fn as_raw(&self) -> br::spirv_cross::MSLVertexFormat {
        use self::Format::*;
        use crate::bindings::root::spirv_cross::MSLVertexFormat as R;
        match self {
            Other => R::MSL_VERTEX_FORMAT_OTHER,
            Uint8 => R::MSL_VERTEX_FORMAT_UINT8,
            Uint16 => R::MSL_VERTEX_FORMAT_UINT16,
        }
    }
}

/// Vertex attribute description for overriding
#[derive(Debug, Clone, Hash, Eq, PartialEq)]
pub struct VertexAttribute {
    pub buffer_id: u32,
    pub offset: u32,
    pub stride: u32,
    pub step: spirv::VertexAttributeStep,
    pub format: Format,
    pub built_in: Option<spirv::BuiltIn>,
}

/// Location of a resource binding to override
#[derive(Debug, Clone, Hash, Eq, PartialEq, Ord, PartialOrd)]
pub struct ResourceBindingLocation {
    pub stage: spirv::ExecutionModel,
    pub desc_set: u32,
    pub binding: u32,
}

/// Resource binding description for overriding
#[derive(Debug, Clone, Hash, Eq, PartialEq)]
pub struct ResourceBinding {
    pub buffer_id: u32,
    pub texture_id: u32,
    pub sampler_id: u32,
}

/// Location of a sampler binding to override
#[derive(Debug, Clone, Hash, Eq, PartialEq, Ord, PartialOrd)]
pub struct SamplerLocation {
    pub desc_set: u32,
    pub binding: u32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub enum SamplerCoord {
    Normalized = 0,
    Pixel = 1,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub enum SamplerFilter {
    Nearest = 0,
    Linear = 1,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub enum SamplerMipFilter {
    None = 0,
    Nearest = 1,
    Linear = 2,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub enum SamplerAddress {
    ClampToZero = 0,
    ClampToEdge = 1,
    ClampToBorder = 2,
    Repeat = 3,
    MirroredRepeat = 4,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub enum SamplerCompareFunc {
    Never = 0,
    Less = 1,
    LessEqual = 2,
    Greater = 3,
    GreaterEqual = 4,
    Equal = 5,
    NotEqual = 6,
    Always = 7,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub enum SamplerBorderColor {
    TransparentBlack = 0,
    OpaqueBlack = 1,
    OpaqueWhite = 2,
}

#[repr(transparent)]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub struct LodBase16(u8);

impl LodBase16 {
    pub const ZERO: Self = LodBase16(0);
    pub const MAX: Self = LodBase16(!0);
}

impl From<f32> for LodBase16 {
    fn from(v: f32) -> Self {
        LodBase16((v * 16.0).max(0.0).min(u8::MAX as f32) as u8)
    }
}

impl Into<f32> for LodBase16 {
    fn into(self) -> f32 {
        self.0 as f32 / 16.0
    }
}

/// MSL format resolution.
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum FormatResolution {
    _444 = 0,
    _422 = 1,
    _420 = 2,
}

/// MSL chroma location.
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum ChromaLocation {
    CositedEven = 0,
    LocationMidpoint = 1,
}

/// MSL component swizzle.
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum ComponentSwizzle {
    Identity = 0,
    Zero = 1,
    One = 2,
    R = 3,
    G = 4,
    B = 5,
    A = 6,
}

/// Data fully defining a constant sampler.
#[derive(Debug, Clone, Hash, Eq, PartialEq)]
pub struct SamplerData {
    pub coord: SamplerCoord,
    pub min_filter: SamplerFilter,
    pub mag_filter: SamplerFilter,
    pub mip_filter: SamplerMipFilter,
    pub s_address: SamplerAddress,
    pub t_address: SamplerAddress,
    pub r_address: SamplerAddress,
    pub compare_func: SamplerCompareFunc,
    pub border_color: SamplerBorderColor,
    pub lod_clamp_min: LodBase16,
    pub lod_clamp_max: LodBase16,
    pub max_anisotropy: i32,
    // Sampler YCbCr conversion parameters
    pub planes: u32,
    pub resolution: FormatResolution,
    pub chroma_filter: SamplerFilter,
    pub x_chroma_offset: ChromaLocation,
    pub y_chroma_offset: ChromaLocation,
    pub swizzle: [ComponentSwizzle; 4],
    pub ycbcr_conversion_enable: bool,
    pub ycbcr_model: SamplerYCbCrModelConversion,
    pub ycbcr_range: SamplerYCbCrRange,
    pub bpc: u32,
}

/// A MSL sampler YCbCr model conversion.
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum SamplerYCbCrModelConversion {
    RgbIdentity = 0,
    YCbCrIdentity = 1,
    YCbCrBt709 = 2,
    YCbCrBt601 = 3,
    YCbCrBt2020 = 4,
}

/// A MSL sampler YCbCr range.
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum SamplerYCbCrRange {
    ItuFull = 0,
    ItuNarrow = 1,
}

/// A MSL shader platform.
#[repr(u8)]
#[allow(non_snake_case, non_camel_case_types)]
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
pub enum Platform {
    iOS = 0,
    macOS = 1,
}

/// A MSL shader model version.
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
pub enum Version {
    V1_0,
    V1_1,
    V1_2,
    V2_0,
    V2_1,
    V2_2,
}

impl Version {
    fn as_raw(self) -> u32 {
        use self::Version::*;
        match self {
            V1_0 => 10000,
            V1_1 => 10100,
            V1_2 => 10200,
            V2_0 => 20000,
            V2_1 => 20100,
            V2_2 => 20200,
        }
    }
}

#[derive(Debug, Clone, Hash, Eq, PartialEq)]
pub struct CompilerVertexOptions {
    pub invert_y: bool,
    pub transform_clip_space: bool,
}

impl Default for CompilerVertexOptions {
    fn default() -> Self {
        CompilerVertexOptions {
            invert_y: false,
            transform_clip_space: false,
        }
    }
}

/// MSL compiler options.
#[derive(Debug, Clone, Hash, Eq, PartialEq)]
pub struct CompilerOptions {
    /// The target platform.
    pub platform: Platform,
    /// The target MSL version.
    pub version: Version,
    /// Vertex compiler options.
    pub vertex: CompilerVertexOptions,
    /// The buffer index to use for swizzle.
    pub swizzle_buffer_index: u32,
    // The buffer index to use for indirect params.
    pub indirect_params_buffer_index: u32,
    /// The buffer index to use for output.
    pub output_buffer_index: u32,
    /// The buffer index to use for patch output.
    pub patch_output_buffer_index: u32,
    /// The buffer index to use for tessellation factor.
    pub tessellation_factor_buffer_index: u32,
    /// The buffer index to use for buffer size.
    pub buffer_size_buffer_index: u32,
    /// Whether the built-in point size should be enabled.
    pub enable_point_size_builtin: bool,
    /// Whether rasterization should be enabled.
    pub enable_rasterization: bool,
    /// Whether to capture output to buffer.
    pub capture_output_to_buffer: bool,
    /// Whether to swizzle texture samples.
    pub swizzle_texture_samples: bool,
    /// Whether to place the origin of tessellation domain shaders in the lower left.
    pub tessellation_domain_origin_lower_left: bool,
    /// Whether to enable use of argument buffers (only compatible with MSL 2.0).
    pub enable_argument_buffers: bool,
    /// Whether to pad fragment output to have at least the number of components as the render pass.
    pub pad_fragment_output_components: bool,
    /// MSL resource bindings overrides.
    pub resource_binding_overrides: BTreeMap<ResourceBindingLocation, ResourceBinding>,
    /// MSL vertex attribute overrides.
    pub vertex_attribute_overrides: BTreeMap<VertexAttributeLocation, VertexAttribute>,
    /// MSL const sampler mappings.
    pub const_samplers: BTreeMap<SamplerLocation, SamplerData>,
}

impl CompilerOptions {
    fn as_raw(&self) -> br::ScMslCompilerOptions {
        br::ScMslCompilerOptions {
            vertex_invert_y: self.vertex.invert_y,
            vertex_transform_clip_space: self.vertex.transform_clip_space,
            platform: self.platform as _,
            version: self.version.as_raw(),
            enable_point_size_builtin: self.enable_point_size_builtin,
            disable_rasterization: !self.enable_rasterization,
            swizzle_buffer_index: self.swizzle_buffer_index,
            indirect_params_buffer_index: self.indirect_params_buffer_index,
            shader_output_buffer_index: self.output_buffer_index,
            shader_patch_output_buffer_index: self.patch_output_buffer_index,
            shader_tess_factor_buffer_index: self.tessellation_factor_buffer_index,
            buffer_size_buffer_index: self.buffer_size_buffer_index,
            capture_output_to_buffer: self.capture_output_to_buffer,
            swizzle_texture_samples: self.swizzle_texture_samples,
            tess_domain_origin_lower_left: self.tessellation_domain_origin_lower_left,
            argument_buffers: self.enable_argument_buffers,
            pad_fragment_output_components: self.pad_fragment_output_components,
        }
    }
}

impl Default for CompilerOptions {
    fn default() -> Self {
        CompilerOptions {
            platform: Platform::macOS,
            version: Version::V1_2,
            vertex: CompilerVertexOptions::default(),
            swizzle_buffer_index: 30,
            indirect_params_buffer_index: 29,
            output_buffer_index: 28,
            patch_output_buffer_index: 27,
            tessellation_factor_buffer_index: 26,
            buffer_size_buffer_index: 25,
            enable_point_size_builtin: true,
            enable_rasterization: true,
            capture_output_to_buffer: false,
            swizzle_texture_samples: false,
            tessellation_domain_origin_lower_left: false,
            enable_argument_buffers: false,
            pad_fragment_output_components: false,
            resource_binding_overrides: Default::default(),
            vertex_attribute_overrides: Default::default(),
            const_samplers: Default::default(),
        }
    }
}

impl<'a> spirv::Parse<Target> for spirv::Ast<Target> {
    fn parse(module: &spirv::Module) -> Result<Self, ErrorCode> {
        let mut sc_compiler = ptr::null_mut();
        unsafe {
            check!(br::sc_internal_compiler_msl_new(
                &mut sc_compiler,
                module.words.as_ptr(),
                module.words.len(),
            ));
        }

        Ok(spirv::Ast {
            compiler: compiler::Compiler {
                sc_compiler,
                target_data: TargetData {
                    resource_binding_overrides: Vec::new(),
                    vertex_attribute_overrides: Vec::new(),
                    const_samplers: Vec::new(),
                },
                has_been_compiled: false,
            },
            target_type: PhantomData,
        })
    }
}

impl spirv::Compile<Target> for spirv::Ast<Target> {
    type CompilerOptions = CompilerOptions;

    /// Set MSL compiler specific compilation settings.
    fn set_compiler_options(&mut self, options: &CompilerOptions) -> Result<(), ErrorCode> {
        let raw_options = options.as_raw();
        unsafe {
            check!(br::sc_internal_compiler_msl_set_options(
                self.compiler.sc_compiler,
                &raw_options,
            ));
        }

        self.compiler.target_data.resource_binding_overrides.clear();
        self.compiler.target_data.resource_binding_overrides.extend(
            options.resource_binding_overrides.iter().map(|(loc, res)| {
                br::spirv_cross::MSLResourceBinding {
                    stage: loc.stage.as_raw(),
                    desc_set: loc.desc_set,
                    binding: loc.binding,
                    msl_buffer: res.buffer_id,
                    msl_texture: res.texture_id,
                    msl_sampler: res.sampler_id,
                }
            }),
        );

        self.compiler.target_data.vertex_attribute_overrides.clear();
        self.compiler.target_data.vertex_attribute_overrides.extend(
            options.vertex_attribute_overrides.iter().map(|(loc, vat)| {
                br::spirv_cross::MSLVertexAttr {
                    location: loc.0,
                    msl_buffer: vat.buffer_id,
                    msl_offset: vat.offset,
                    msl_stride: vat.stride,
                    per_instance: match vat.step {
                        spirv::VertexAttributeStep::Vertex => false,
                        spirv::VertexAttributeStep::Instance => true,
                    },
                    format: vat.format.as_raw(),
                    builtin: spirv::built_in_as_raw(vat.built_in),
                }
            }),
        );

        self.compiler.target_data.const_samplers.clear();
        self.compiler
            .target_data
            .const_samplers
            .extend(options.const_samplers.iter().map(|(loc, data)| unsafe {
                use std::mem::transmute;
                br::ScMslConstSamplerMapping {
                    desc_set: loc.desc_set,
                    binding: loc.binding,
                    sampler: br::spirv_cross::MSLConstexprSampler {
                        coord: transmute(data.coord),
                        min_filter: transmute(data.min_filter),
                        mag_filter: transmute(data.mag_filter),
                        mip_filter: transmute(data.mip_filter),
                        s_address: transmute(data.s_address),
                        t_address: transmute(data.t_address),
                        r_address: transmute(data.r_address),
                        compare_func: transmute(data.compare_func),
                        border_color: transmute(data.border_color),
                        lod_clamp_min: data.lod_clamp_min.into(),
                        lod_clamp_max: data.lod_clamp_max.into(),
                        max_anisotropy: data.max_anisotropy,
                        compare_enable: data.compare_func != SamplerCompareFunc::Always,
                        lod_clamp_enable: data.lod_clamp_min != LodBase16::ZERO
                            || data.lod_clamp_max != LodBase16::MAX,
                        anisotropy_enable: data.max_anisotropy != 0,
                        bpc: data.bpc,
                        chroma_filter: transmute(data.chroma_filter),
                        planes: data.planes,
                        resolution: transmute(data.resolution),
                        swizzle: transmute(data.swizzle),
                        x_chroma_offset: transmute(data.x_chroma_offset),
                        y_chroma_offset: transmute(data.y_chroma_offset),
                        ycbcr_conversion_enable: data.ycbcr_conversion_enable,
                        ycbcr_model: transmute(data.ycbcr_model),
                        ycbcr_range: transmute(data.ycbcr_range),
                    },
                }
            }));

        Ok(())
    }

    /// Generate MSL shader from the AST.
    fn compile(&mut self) -> Result<String, ErrorCode> {
        self.compile_internal()
    }
}

impl spirv::Ast<Target> {
    fn compile_internal(&self) -> Result<String, ErrorCode> {
        let vat_overrides = &self.compiler.target_data.vertex_attribute_overrides;
        let res_overrides = &self.compiler.target_data.resource_binding_overrides;
        let const_samplers = &self.compiler.target_data.const_samplers;
        unsafe {
            let mut shader_ptr = ptr::null();
            check!(br::sc_internal_compiler_msl_compile(
                self.compiler.sc_compiler,
                &mut shader_ptr,
                vat_overrides.as_ptr(),
                vat_overrides.len(),
                res_overrides.as_ptr(),
                res_overrides.len(),
                const_samplers.as_ptr(),
                const_samplers.len(),
            ));
            let shader = match CStr::from_ptr(shader_ptr).to_str() {
                Ok(v) => v.to_owned(),
                Err(_) => return Err(ErrorCode::Unhandled),
            };
            check!(br::sc_internal_free_pointer(
                shader_ptr as *mut std::os::raw::c_void
            ));
            Ok(shader)
        }
    }

    pub fn is_rasterization_enabled(&self) -> Result<bool, ErrorCode> {
        unsafe {
            let mut is_disabled = false;
            check!(br::sc_internal_compiler_msl_get_is_rasterization_disabled(
                self.compiler.sc_compiler,
                &mut is_disabled
            ));
            Ok(!is_disabled)
        }
    }
}

// TODO: Generate with bindgen
pub const ARGUMENT_BUFFER_BINDING: u32 = !3;
