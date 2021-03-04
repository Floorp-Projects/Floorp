// AUTOMATICALLY GENERATED from the SPIR-V JSON grammar:
//   external/spirv.core.grammar.json.
// DO NOT MODIFY!

pub type Word = u32;
pub const MAGIC_NUMBER: u32 = 0x07230203;
pub const MAJOR_VERSION: u8 = 1u8;
pub const MINOR_VERSION: u8 = 5u8;
pub const REVISION: u8 = 3u8;
bitflags! { # [ doc = "SPIR-V operand kind: [ImageOperands](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_image_operands_a_image_operands)" ] # [ cfg_attr ( feature = "serialize" , derive ( serde :: Serialize ) ) ] # [ cfg_attr ( feature = "deserialize" , derive ( serde :: Deserialize ) ) ] pub struct ImageOperands : u32 { const NONE = 0u32 ; const BIAS = 1u32 ; const LOD = 2u32 ; const GRAD = 4u32 ; const CONST_OFFSET = 8u32 ; const OFFSET = 16u32 ; const CONST_OFFSETS = 32u32 ; const SAMPLE = 64u32 ; const MIN_LOD = 128u32 ; const MAKE_TEXEL_AVAILABLE = 256u32 ; const MAKE_TEXEL_AVAILABLE_KHR = 256u32 ; const MAKE_TEXEL_VISIBLE = 512u32 ; const MAKE_TEXEL_VISIBLE_KHR = 512u32 ; const NON_PRIVATE_TEXEL = 1024u32 ; const NON_PRIVATE_TEXEL_KHR = 1024u32 ; const VOLATILE_TEXEL = 2048u32 ; const VOLATILE_TEXEL_KHR = 2048u32 ; const SIGN_EXTEND = 4096u32 ; const ZERO_EXTEND = 8192u32 ; } }
bitflags! { # [ doc = "SPIR-V operand kind: [FPFastMathMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_fp_fast_math_mode_a_fp_fast_math_mode)" ] # [ cfg_attr ( feature = "serialize" , derive ( serde :: Serialize ) ) ] # [ cfg_attr ( feature = "deserialize" , derive ( serde :: Deserialize ) ) ] pub struct FPFastMathMode : u32 { const NONE = 0u32 ; const NOT_NAN = 1u32 ; const NOT_INF = 2u32 ; const NSZ = 4u32 ; const ALLOW_RECIP = 8u32 ; const FAST = 16u32 ; } }
bitflags! { # [ doc = "SPIR-V operand kind: [SelectionControl](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_selection_control_a_selection_control)" ] # [ cfg_attr ( feature = "serialize" , derive ( serde :: Serialize ) ) ] # [ cfg_attr ( feature = "deserialize" , derive ( serde :: Deserialize ) ) ] pub struct SelectionControl : u32 { const NONE = 0u32 ; const FLATTEN = 1u32 ; const DONT_FLATTEN = 2u32 ; } }
bitflags! { # [ doc = "SPIR-V operand kind: [LoopControl](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_loop_control_a_loop_control)" ] # [ cfg_attr ( feature = "serialize" , derive ( serde :: Serialize ) ) ] # [ cfg_attr ( feature = "deserialize" , derive ( serde :: Deserialize ) ) ] pub struct LoopControl : u32 { const NONE = 0u32 ; const UNROLL = 1u32 ; const DONT_UNROLL = 2u32 ; const DEPENDENCY_INFINITE = 4u32 ; const DEPENDENCY_LENGTH = 8u32 ; const MIN_ITERATIONS = 16u32 ; const MAX_ITERATIONS = 32u32 ; const ITERATION_MULTIPLE = 64u32 ; const PEEL_COUNT = 128u32 ; const PARTIAL_COUNT = 256u32 ; } }
bitflags! { # [ doc = "SPIR-V operand kind: [FunctionControl](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_function_control_a_function_control)" ] # [ cfg_attr ( feature = "serialize" , derive ( serde :: Serialize ) ) ] # [ cfg_attr ( feature = "deserialize" , derive ( serde :: Deserialize ) ) ] pub struct FunctionControl : u32 { const NONE = 0u32 ; const INLINE = 1u32 ; const DONT_INLINE = 2u32 ; const PURE = 4u32 ; const CONST = 8u32 ; } }
bitflags! { # [ doc = "SPIR-V operand kind: [MemorySemantics](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_memory_semantics_a_memory_semantics)" ] # [ cfg_attr ( feature = "serialize" , derive ( serde :: Serialize ) ) ] # [ cfg_attr ( feature = "deserialize" , derive ( serde :: Deserialize ) ) ] pub struct MemorySemantics : u32 { const RELAXED = 0u32 ; const NONE = 0u32 ; const ACQUIRE = 2u32 ; const RELEASE = 4u32 ; const ACQUIRE_RELEASE = 8u32 ; const SEQUENTIALLY_CONSISTENT = 16u32 ; const UNIFORM_MEMORY = 64u32 ; const SUBGROUP_MEMORY = 128u32 ; const WORKGROUP_MEMORY = 256u32 ; const CROSS_WORKGROUP_MEMORY = 512u32 ; const ATOMIC_COUNTER_MEMORY = 1024u32 ; const IMAGE_MEMORY = 2048u32 ; const OUTPUT_MEMORY = 4096u32 ; const OUTPUT_MEMORY_KHR = 4096u32 ; const MAKE_AVAILABLE = 8192u32 ; const MAKE_AVAILABLE_KHR = 8192u32 ; const MAKE_VISIBLE = 16384u32 ; const MAKE_VISIBLE_KHR = 16384u32 ; const VOLATILE = 32768u32 ; } }
bitflags! { # [ doc = "SPIR-V operand kind: [MemoryAccess](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_memory_access_a_memory_access)" ] # [ cfg_attr ( feature = "serialize" , derive ( serde :: Serialize ) ) ] # [ cfg_attr ( feature = "deserialize" , derive ( serde :: Deserialize ) ) ] pub struct MemoryAccess : u32 { const NONE = 0u32 ; const VOLATILE = 1u32 ; const ALIGNED = 2u32 ; const NONTEMPORAL = 4u32 ; const MAKE_POINTER_AVAILABLE = 8u32 ; const MAKE_POINTER_AVAILABLE_KHR = 8u32 ; const MAKE_POINTER_VISIBLE = 16u32 ; const MAKE_POINTER_VISIBLE_KHR = 16u32 ; const NON_PRIVATE_POINTER = 32u32 ; const NON_PRIVATE_POINTER_KHR = 32u32 ; } }
bitflags! { # [ doc = "SPIR-V operand kind: [KernelProfilingInfo](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_kernel_profiling_info_a_kernel_profiling_info)" ] # [ cfg_attr ( feature = "serialize" , derive ( serde :: Serialize ) ) ] # [ cfg_attr ( feature = "deserialize" , derive ( serde :: Deserialize ) ) ] pub struct KernelProfilingInfo : u32 { const NONE = 0u32 ; const CMD_EXEC_TIME = 1u32 ; } }
bitflags! { # [ doc = "SPIR-V operand kind: [RayFlags](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_ray_flags_a_ray_flags)" ] # [ cfg_attr ( feature = "serialize" , derive ( serde :: Serialize ) ) ] # [ cfg_attr ( feature = "deserialize" , derive ( serde :: Deserialize ) ) ] pub struct RayFlags : u32 { const NONE_KHR = 0u32 ; const OPAQUE_KHR = 1u32 ; const NO_OPAQUE_KHR = 2u32 ; const TERMINATE_ON_FIRST_HIT_KHR = 4u32 ; const SKIP_CLOSEST_HIT_SHADER_KHR = 8u32 ; const CULL_BACK_FACING_TRIANGLES_KHR = 16u32 ; const CULL_FRONT_FACING_TRIANGLES_KHR = 32u32 ; const CULL_OPAQUE_KHR = 64u32 ; const CULL_NO_OPAQUE_KHR = 128u32 ; const SKIP_TRIANGLES_KHR = 256u32 ; const SKIP_AAB_BS_KHR = 512u32 ; } }
#[doc = "/// SPIR-V operand kind: [SourceLanguage](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_source_language_a_source_language)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum SourceLanguage {
    Unknown = 0u32,
    ESSL = 1u32,
    GLSL = 2u32,
    OpenCL_C = 3u32,
    OpenCL_CPP = 4u32,
    HLSL = 5u32,
}
#[allow(non_upper_case_globals)]
impl SourceLanguage {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            SourceLanguage::Unknown
            | SourceLanguage::ESSL
            | SourceLanguage::GLSL
            | SourceLanguage::OpenCL_C
            | SourceLanguage::OpenCL_CPP
            | SourceLanguage::HLSL => &[],
        }
    }
}
impl num_traits::FromPrimitive for SourceLanguage {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => SourceLanguage::Unknown,
            1u32 => SourceLanguage::ESSL,
            2u32 => SourceLanguage::GLSL,
            3u32 => SourceLanguage::OpenCL_C,
            4u32 => SourceLanguage::OpenCL_CPP,
            5u32 => SourceLanguage::HLSL,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [ExecutionModel](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_execution_model_a_execution_model)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum ExecutionModel {
    Vertex = 0u32,
    TessellationControl = 1u32,
    TessellationEvaluation = 2u32,
    Geometry = 3u32,
    Fragment = 4u32,
    GLCompute = 5u32,
    Kernel = 6u32,
    TaskNV = 5267u32,
    MeshNV = 5268u32,
    RayGenerationNV = 5313u32,
    IntersectionNV = 5314u32,
    AnyHitNV = 5315u32,
    ClosestHitNV = 5316u32,
    MissNV = 5317u32,
    CallableNV = 5318u32,
}
#[allow(non_upper_case_globals)]
impl ExecutionModel {
    pub const RayGenerationKHR: ExecutionModel = ExecutionModel::RayGenerationNV;
    pub const IntersectionKHR: ExecutionModel = ExecutionModel::IntersectionNV;
    pub const AnyHitKHR: ExecutionModel = ExecutionModel::AnyHitNV;
    pub const ClosestHitKHR: ExecutionModel = ExecutionModel::ClosestHitNV;
    pub const MissKHR: ExecutionModel = ExecutionModel::MissNV;
    pub const CallableKHR: ExecutionModel = ExecutionModel::CallableNV;
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            ExecutionModel::Geometry => &[Capability::Geometry],
            ExecutionModel::Kernel => &[Capability::Kernel],
            ExecutionModel::TaskNV | ExecutionModel::MeshNV => &[Capability::MeshShadingNV],
            ExecutionModel::RayGenerationNV
            | ExecutionModel::IntersectionNV
            | ExecutionModel::AnyHitNV
            | ExecutionModel::ClosestHitNV
            | ExecutionModel::MissNV
            | ExecutionModel::CallableNV => &[
                Capability::RayTracingNV,
                Capability::RayTracingProvisionalKHR,
            ],
            ExecutionModel::Vertex | ExecutionModel::Fragment | ExecutionModel::GLCompute => {
                &[Capability::Shader]
            }
            ExecutionModel::TessellationControl | ExecutionModel::TessellationEvaluation => {
                &[Capability::Tessellation]
            }
        }
    }
}
impl num_traits::FromPrimitive for ExecutionModel {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => ExecutionModel::Vertex,
            1u32 => ExecutionModel::TessellationControl,
            2u32 => ExecutionModel::TessellationEvaluation,
            3u32 => ExecutionModel::Geometry,
            4u32 => ExecutionModel::Fragment,
            5u32 => ExecutionModel::GLCompute,
            6u32 => ExecutionModel::Kernel,
            5267u32 => ExecutionModel::TaskNV,
            5268u32 => ExecutionModel::MeshNV,
            5313u32 => ExecutionModel::RayGenerationNV,
            5314u32 => ExecutionModel::IntersectionNV,
            5315u32 => ExecutionModel::AnyHitNV,
            5316u32 => ExecutionModel::ClosestHitNV,
            5317u32 => ExecutionModel::MissNV,
            5318u32 => ExecutionModel::CallableNV,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [AddressingModel](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_addressing_model_a_addressing_model)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum AddressingModel {
    Logical = 0u32,
    Physical32 = 1u32,
    Physical64 = 2u32,
    PhysicalStorageBuffer64 = 5348u32,
}
#[allow(non_upper_case_globals)]
impl AddressingModel {
    pub const PhysicalStorageBuffer64EXT: AddressingModel =
        AddressingModel::PhysicalStorageBuffer64;
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            AddressingModel::Logical => &[],
            AddressingModel::Physical32 | AddressingModel::Physical64 => &[Capability::Addresses],
            AddressingModel::PhysicalStorageBuffer64 => {
                &[Capability::PhysicalStorageBufferAddresses]
            }
        }
    }
}
impl num_traits::FromPrimitive for AddressingModel {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => AddressingModel::Logical,
            1u32 => AddressingModel::Physical32,
            2u32 => AddressingModel::Physical64,
            5348u32 => AddressingModel::PhysicalStorageBuffer64,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [MemoryModel](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_memory_model_a_memory_model)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum MemoryModel {
    Simple = 0u32,
    GLSL450 = 1u32,
    OpenCL = 2u32,
    Vulkan = 3u32,
}
#[allow(non_upper_case_globals)]
impl MemoryModel {
    pub const VulkanKHR: MemoryModel = MemoryModel::Vulkan;
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            MemoryModel::OpenCL => &[Capability::Kernel],
            MemoryModel::Simple | MemoryModel::GLSL450 => &[Capability::Shader],
            MemoryModel::Vulkan => &[Capability::VulkanMemoryModel],
        }
    }
}
impl num_traits::FromPrimitive for MemoryModel {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => MemoryModel::Simple,
            1u32 => MemoryModel::GLSL450,
            2u32 => MemoryModel::OpenCL,
            3u32 => MemoryModel::Vulkan,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [ExecutionMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_execution_mode_a_execution_mode)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum ExecutionMode {
    Invocations = 0u32,
    SpacingEqual = 1u32,
    SpacingFractionalEven = 2u32,
    SpacingFractionalOdd = 3u32,
    VertexOrderCw = 4u32,
    VertexOrderCcw = 5u32,
    PixelCenterInteger = 6u32,
    OriginUpperLeft = 7u32,
    OriginLowerLeft = 8u32,
    EarlyFragmentTests = 9u32,
    PointMode = 10u32,
    Xfb = 11u32,
    DepthReplacing = 12u32,
    DepthGreater = 14u32,
    DepthLess = 15u32,
    DepthUnchanged = 16u32,
    LocalSize = 17u32,
    LocalSizeHint = 18u32,
    InputPoints = 19u32,
    InputLines = 20u32,
    InputLinesAdjacency = 21u32,
    Triangles = 22u32,
    InputTrianglesAdjacency = 23u32,
    Quads = 24u32,
    Isolines = 25u32,
    OutputVertices = 26u32,
    OutputPoints = 27u32,
    OutputLineStrip = 28u32,
    OutputTriangleStrip = 29u32,
    VecTypeHint = 30u32,
    ContractionOff = 31u32,
    Initializer = 33u32,
    Finalizer = 34u32,
    SubgroupSize = 35u32,
    SubgroupsPerWorkgroup = 36u32,
    SubgroupsPerWorkgroupId = 37u32,
    LocalSizeId = 38u32,
    LocalSizeHintId = 39u32,
    PostDepthCoverage = 4446u32,
    DenormPreserve = 4459u32,
    DenormFlushToZero = 4460u32,
    SignedZeroInfNanPreserve = 4461u32,
    RoundingModeRTE = 4462u32,
    RoundingModeRTZ = 4463u32,
    StencilRefReplacingEXT = 5027u32,
    OutputLinesNV = 5269u32,
    OutputPrimitivesNV = 5270u32,
    DerivativeGroupQuadsNV = 5289u32,
    DerivativeGroupLinearNV = 5290u32,
    OutputTrianglesNV = 5298u32,
    PixelInterlockOrderedEXT = 5366u32,
    PixelInterlockUnorderedEXT = 5367u32,
    SampleInterlockOrderedEXT = 5368u32,
    SampleInterlockUnorderedEXT = 5369u32,
    ShadingRateInterlockOrderedEXT = 5370u32,
    ShadingRateInterlockUnorderedEXT = 5371u32,
}
#[allow(non_upper_case_globals)]
impl ExecutionMode {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            ExecutionMode::LocalSize | ExecutionMode::LocalSizeId => &[],
            ExecutionMode::DerivativeGroupLinearNV => &[Capability::ComputeDerivativeGroupLinearNV],
            ExecutionMode::DerivativeGroupQuadsNV => &[Capability::ComputeDerivativeGroupQuadsNV],
            ExecutionMode::DenormFlushToZero => &[Capability::DenormFlushToZero],
            ExecutionMode::DenormPreserve => &[Capability::DenormPreserve],
            ExecutionMode::PixelInterlockOrderedEXT | ExecutionMode::PixelInterlockUnorderedEXT => {
                &[Capability::FragmentShaderPixelInterlockEXT]
            }
            ExecutionMode::SampleInterlockOrderedEXT
            | ExecutionMode::SampleInterlockUnorderedEXT => {
                &[Capability::FragmentShaderSampleInterlockEXT]
            }
            ExecutionMode::ShadingRateInterlockOrderedEXT
            | ExecutionMode::ShadingRateInterlockUnorderedEXT => {
                &[Capability::FragmentShaderShadingRateInterlockEXT]
            }
            ExecutionMode::Invocations
            | ExecutionMode::InputPoints
            | ExecutionMode::InputLines
            | ExecutionMode::InputLinesAdjacency
            | ExecutionMode::InputTrianglesAdjacency
            | ExecutionMode::OutputLineStrip
            | ExecutionMode::OutputTriangleStrip => &[Capability::Geometry],
            ExecutionMode::OutputPoints => &[Capability::Geometry, Capability::MeshShadingNV],
            ExecutionMode::Triangles => &[Capability::Geometry, Capability::Tessellation],
            ExecutionMode::OutputVertices => &[
                Capability::Geometry,
                Capability::Tessellation,
                Capability::MeshShadingNV,
            ],
            ExecutionMode::LocalSizeHint
            | ExecutionMode::VecTypeHint
            | ExecutionMode::ContractionOff
            | ExecutionMode::Initializer
            | ExecutionMode::Finalizer
            | ExecutionMode::LocalSizeHintId => &[Capability::Kernel],
            ExecutionMode::OutputLinesNV
            | ExecutionMode::OutputPrimitivesNV
            | ExecutionMode::OutputTrianglesNV => &[Capability::MeshShadingNV],
            ExecutionMode::RoundingModeRTE => &[Capability::RoundingModeRTE],
            ExecutionMode::RoundingModeRTZ => &[Capability::RoundingModeRTZ],
            ExecutionMode::PostDepthCoverage => &[Capability::SampleMaskPostDepthCoverage],
            ExecutionMode::PixelCenterInteger
            | ExecutionMode::OriginUpperLeft
            | ExecutionMode::OriginLowerLeft
            | ExecutionMode::EarlyFragmentTests
            | ExecutionMode::DepthReplacing
            | ExecutionMode::DepthGreater
            | ExecutionMode::DepthLess
            | ExecutionMode::DepthUnchanged => &[Capability::Shader],
            ExecutionMode::SignedZeroInfNanPreserve => &[Capability::SignedZeroInfNanPreserve],
            ExecutionMode::StencilRefReplacingEXT => &[Capability::StencilExportEXT],
            ExecutionMode::SubgroupSize
            | ExecutionMode::SubgroupsPerWorkgroup
            | ExecutionMode::SubgroupsPerWorkgroupId => &[Capability::SubgroupDispatch],
            ExecutionMode::SpacingEqual
            | ExecutionMode::SpacingFractionalEven
            | ExecutionMode::SpacingFractionalOdd
            | ExecutionMode::VertexOrderCw
            | ExecutionMode::VertexOrderCcw
            | ExecutionMode::PointMode
            | ExecutionMode::Quads
            | ExecutionMode::Isolines => &[Capability::Tessellation],
            ExecutionMode::Xfb => &[Capability::TransformFeedback],
        }
    }
}
impl num_traits::FromPrimitive for ExecutionMode {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => ExecutionMode::Invocations,
            1u32 => ExecutionMode::SpacingEqual,
            2u32 => ExecutionMode::SpacingFractionalEven,
            3u32 => ExecutionMode::SpacingFractionalOdd,
            4u32 => ExecutionMode::VertexOrderCw,
            5u32 => ExecutionMode::VertexOrderCcw,
            6u32 => ExecutionMode::PixelCenterInteger,
            7u32 => ExecutionMode::OriginUpperLeft,
            8u32 => ExecutionMode::OriginLowerLeft,
            9u32 => ExecutionMode::EarlyFragmentTests,
            10u32 => ExecutionMode::PointMode,
            11u32 => ExecutionMode::Xfb,
            12u32 => ExecutionMode::DepthReplacing,
            14u32 => ExecutionMode::DepthGreater,
            15u32 => ExecutionMode::DepthLess,
            16u32 => ExecutionMode::DepthUnchanged,
            17u32 => ExecutionMode::LocalSize,
            18u32 => ExecutionMode::LocalSizeHint,
            19u32 => ExecutionMode::InputPoints,
            20u32 => ExecutionMode::InputLines,
            21u32 => ExecutionMode::InputLinesAdjacency,
            22u32 => ExecutionMode::Triangles,
            23u32 => ExecutionMode::InputTrianglesAdjacency,
            24u32 => ExecutionMode::Quads,
            25u32 => ExecutionMode::Isolines,
            26u32 => ExecutionMode::OutputVertices,
            27u32 => ExecutionMode::OutputPoints,
            28u32 => ExecutionMode::OutputLineStrip,
            29u32 => ExecutionMode::OutputTriangleStrip,
            30u32 => ExecutionMode::VecTypeHint,
            31u32 => ExecutionMode::ContractionOff,
            33u32 => ExecutionMode::Initializer,
            34u32 => ExecutionMode::Finalizer,
            35u32 => ExecutionMode::SubgroupSize,
            36u32 => ExecutionMode::SubgroupsPerWorkgroup,
            37u32 => ExecutionMode::SubgroupsPerWorkgroupId,
            38u32 => ExecutionMode::LocalSizeId,
            39u32 => ExecutionMode::LocalSizeHintId,
            4446u32 => ExecutionMode::PostDepthCoverage,
            4459u32 => ExecutionMode::DenormPreserve,
            4460u32 => ExecutionMode::DenormFlushToZero,
            4461u32 => ExecutionMode::SignedZeroInfNanPreserve,
            4462u32 => ExecutionMode::RoundingModeRTE,
            4463u32 => ExecutionMode::RoundingModeRTZ,
            5027u32 => ExecutionMode::StencilRefReplacingEXT,
            5269u32 => ExecutionMode::OutputLinesNV,
            5270u32 => ExecutionMode::OutputPrimitivesNV,
            5289u32 => ExecutionMode::DerivativeGroupQuadsNV,
            5290u32 => ExecutionMode::DerivativeGroupLinearNV,
            5298u32 => ExecutionMode::OutputTrianglesNV,
            5366u32 => ExecutionMode::PixelInterlockOrderedEXT,
            5367u32 => ExecutionMode::PixelInterlockUnorderedEXT,
            5368u32 => ExecutionMode::SampleInterlockOrderedEXT,
            5369u32 => ExecutionMode::SampleInterlockUnorderedEXT,
            5370u32 => ExecutionMode::ShadingRateInterlockOrderedEXT,
            5371u32 => ExecutionMode::ShadingRateInterlockUnorderedEXT,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [StorageClass](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_storage_class_a_storage_class)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum StorageClass {
    UniformConstant = 0u32,
    Input = 1u32,
    Uniform = 2u32,
    Output = 3u32,
    Workgroup = 4u32,
    CrossWorkgroup = 5u32,
    Private = 6u32,
    Function = 7u32,
    Generic = 8u32,
    PushConstant = 9u32,
    AtomicCounter = 10u32,
    Image = 11u32,
    StorageBuffer = 12u32,
    CallableDataNV = 5328u32,
    IncomingCallableDataNV = 5329u32,
    RayPayloadNV = 5338u32,
    HitAttributeNV = 5339u32,
    IncomingRayPayloadNV = 5342u32,
    ShaderRecordBufferNV = 5343u32,
    PhysicalStorageBuffer = 5349u32,
}
#[allow(non_upper_case_globals)]
impl StorageClass {
    pub const CallableDataKHR: StorageClass = StorageClass::CallableDataNV;
    pub const IncomingCallableDataKHR: StorageClass = StorageClass::IncomingCallableDataNV;
    pub const RayPayloadKHR: StorageClass = StorageClass::RayPayloadNV;
    pub const HitAttributeKHR: StorageClass = StorageClass::HitAttributeNV;
    pub const IncomingRayPayloadKHR: StorageClass = StorageClass::IncomingRayPayloadNV;
    pub const ShaderRecordBufferKHR: StorageClass = StorageClass::ShaderRecordBufferNV;
    pub const PhysicalStorageBufferEXT: StorageClass = StorageClass::PhysicalStorageBuffer;
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            StorageClass::UniformConstant
            | StorageClass::Input
            | StorageClass::Workgroup
            | StorageClass::CrossWorkgroup
            | StorageClass::Function
            | StorageClass::Image => &[],
            StorageClass::AtomicCounter => &[Capability::AtomicStorage],
            StorageClass::Generic => &[Capability::GenericPointer],
            StorageClass::PhysicalStorageBuffer => &[Capability::PhysicalStorageBufferAddresses],
            StorageClass::CallableDataNV
            | StorageClass::IncomingCallableDataNV
            | StorageClass::RayPayloadNV
            | StorageClass::HitAttributeNV
            | StorageClass::IncomingRayPayloadNV
            | StorageClass::ShaderRecordBufferNV => &[
                Capability::RayTracingNV,
                Capability::RayTracingProvisionalKHR,
            ],
            StorageClass::Uniform
            | StorageClass::Output
            | StorageClass::Private
            | StorageClass::PushConstant
            | StorageClass::StorageBuffer => &[Capability::Shader],
        }
    }
}
impl num_traits::FromPrimitive for StorageClass {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => StorageClass::UniformConstant,
            1u32 => StorageClass::Input,
            2u32 => StorageClass::Uniform,
            3u32 => StorageClass::Output,
            4u32 => StorageClass::Workgroup,
            5u32 => StorageClass::CrossWorkgroup,
            6u32 => StorageClass::Private,
            7u32 => StorageClass::Function,
            8u32 => StorageClass::Generic,
            9u32 => StorageClass::PushConstant,
            10u32 => StorageClass::AtomicCounter,
            11u32 => StorageClass::Image,
            12u32 => StorageClass::StorageBuffer,
            5328u32 => StorageClass::CallableDataNV,
            5329u32 => StorageClass::IncomingCallableDataNV,
            5338u32 => StorageClass::RayPayloadNV,
            5339u32 => StorageClass::HitAttributeNV,
            5342u32 => StorageClass::IncomingRayPayloadNV,
            5343u32 => StorageClass::ShaderRecordBufferNV,
            5349u32 => StorageClass::PhysicalStorageBuffer,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [Dim](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_dim_a_dim)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum Dim {
    Dim1D = 0u32,
    Dim2D = 1u32,
    Dim3D = 2u32,
    DimCube = 3u32,
    DimRect = 4u32,
    DimBuffer = 5u32,
    DimSubpassData = 6u32,
}
#[allow(non_upper_case_globals)]
impl Dim {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            Dim::Dim3D => &[],
            Dim::DimSubpassData => &[Capability::InputAttachment],
            Dim::Dim1D => &[Capability::Sampled1D, Capability::Image1D],
            Dim::DimBuffer => &[Capability::SampledBuffer, Capability::ImageBuffer],
            Dim::DimRect => &[Capability::SampledRect, Capability::ImageRect],
            Dim::DimCube => &[Capability::Shader, Capability::ImageCubeArray],
            Dim::Dim2D => &[
                Capability::Shader,
                Capability::Kernel,
                Capability::ImageMSArray,
            ],
        }
    }
}
impl num_traits::FromPrimitive for Dim {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => Dim::Dim1D,
            1u32 => Dim::Dim2D,
            2u32 => Dim::Dim3D,
            3u32 => Dim::DimCube,
            4u32 => Dim::DimRect,
            5u32 => Dim::DimBuffer,
            6u32 => Dim::DimSubpassData,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [SamplerAddressingMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_sampler_addressing_mode_a_sampler_addressing_mode)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum SamplerAddressingMode {
    None = 0u32,
    ClampToEdge = 1u32,
    Clamp = 2u32,
    Repeat = 3u32,
    RepeatMirrored = 4u32,
}
#[allow(non_upper_case_globals)]
impl SamplerAddressingMode {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            SamplerAddressingMode::None
            | SamplerAddressingMode::ClampToEdge
            | SamplerAddressingMode::Clamp
            | SamplerAddressingMode::Repeat
            | SamplerAddressingMode::RepeatMirrored => &[Capability::Kernel],
        }
    }
}
impl num_traits::FromPrimitive for SamplerAddressingMode {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => SamplerAddressingMode::None,
            1u32 => SamplerAddressingMode::ClampToEdge,
            2u32 => SamplerAddressingMode::Clamp,
            3u32 => SamplerAddressingMode::Repeat,
            4u32 => SamplerAddressingMode::RepeatMirrored,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [SamplerFilterMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_sampler_filter_mode_a_sampler_filter_mode)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum SamplerFilterMode {
    Nearest = 0u32,
    Linear = 1u32,
}
#[allow(non_upper_case_globals)]
impl SamplerFilterMode {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            SamplerFilterMode::Nearest | SamplerFilterMode::Linear => &[Capability::Kernel],
        }
    }
}
impl num_traits::FromPrimitive for SamplerFilterMode {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => SamplerFilterMode::Nearest,
            1u32 => SamplerFilterMode::Linear,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [ImageFormat](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_image_format_a_image_format)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum ImageFormat {
    Unknown = 0u32,
    Rgba32f = 1u32,
    Rgba16f = 2u32,
    R32f = 3u32,
    Rgba8 = 4u32,
    Rgba8Snorm = 5u32,
    Rg32f = 6u32,
    Rg16f = 7u32,
    R11fG11fB10f = 8u32,
    R16f = 9u32,
    Rgba16 = 10u32,
    Rgb10A2 = 11u32,
    Rg16 = 12u32,
    Rg8 = 13u32,
    R16 = 14u32,
    R8 = 15u32,
    Rgba16Snorm = 16u32,
    Rg16Snorm = 17u32,
    Rg8Snorm = 18u32,
    R16Snorm = 19u32,
    R8Snorm = 20u32,
    Rgba32i = 21u32,
    Rgba16i = 22u32,
    Rgba8i = 23u32,
    R32i = 24u32,
    Rg32i = 25u32,
    Rg16i = 26u32,
    Rg8i = 27u32,
    R16i = 28u32,
    R8i = 29u32,
    Rgba32ui = 30u32,
    Rgba16ui = 31u32,
    Rgba8ui = 32u32,
    R32ui = 33u32,
    Rgb10a2ui = 34u32,
    Rg32ui = 35u32,
    Rg16ui = 36u32,
    Rg8ui = 37u32,
    R16ui = 38u32,
    R8ui = 39u32,
}
#[allow(non_upper_case_globals)]
impl ImageFormat {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            ImageFormat::Unknown => &[],
            ImageFormat::Rgba32f
            | ImageFormat::Rgba16f
            | ImageFormat::R32f
            | ImageFormat::Rgba8
            | ImageFormat::Rgba8Snorm
            | ImageFormat::Rgba32i
            | ImageFormat::Rgba16i
            | ImageFormat::Rgba8i
            | ImageFormat::R32i
            | ImageFormat::Rgba32ui
            | ImageFormat::Rgba16ui
            | ImageFormat::Rgba8ui
            | ImageFormat::R32ui => &[Capability::Shader],
            ImageFormat::Rg32f
            | ImageFormat::Rg16f
            | ImageFormat::R11fG11fB10f
            | ImageFormat::R16f
            | ImageFormat::Rgba16
            | ImageFormat::Rgb10A2
            | ImageFormat::Rg16
            | ImageFormat::Rg8
            | ImageFormat::R16
            | ImageFormat::R8
            | ImageFormat::Rgba16Snorm
            | ImageFormat::Rg16Snorm
            | ImageFormat::Rg8Snorm
            | ImageFormat::R16Snorm
            | ImageFormat::R8Snorm
            | ImageFormat::Rg32i
            | ImageFormat::Rg16i
            | ImageFormat::Rg8i
            | ImageFormat::R16i
            | ImageFormat::R8i
            | ImageFormat::Rgb10a2ui
            | ImageFormat::Rg32ui
            | ImageFormat::Rg16ui
            | ImageFormat::Rg8ui
            | ImageFormat::R16ui
            | ImageFormat::R8ui => &[Capability::StorageImageExtendedFormats],
        }
    }
}
impl num_traits::FromPrimitive for ImageFormat {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => ImageFormat::Unknown,
            1u32 => ImageFormat::Rgba32f,
            2u32 => ImageFormat::Rgba16f,
            3u32 => ImageFormat::R32f,
            4u32 => ImageFormat::Rgba8,
            5u32 => ImageFormat::Rgba8Snorm,
            6u32 => ImageFormat::Rg32f,
            7u32 => ImageFormat::Rg16f,
            8u32 => ImageFormat::R11fG11fB10f,
            9u32 => ImageFormat::R16f,
            10u32 => ImageFormat::Rgba16,
            11u32 => ImageFormat::Rgb10A2,
            12u32 => ImageFormat::Rg16,
            13u32 => ImageFormat::Rg8,
            14u32 => ImageFormat::R16,
            15u32 => ImageFormat::R8,
            16u32 => ImageFormat::Rgba16Snorm,
            17u32 => ImageFormat::Rg16Snorm,
            18u32 => ImageFormat::Rg8Snorm,
            19u32 => ImageFormat::R16Snorm,
            20u32 => ImageFormat::R8Snorm,
            21u32 => ImageFormat::Rgba32i,
            22u32 => ImageFormat::Rgba16i,
            23u32 => ImageFormat::Rgba8i,
            24u32 => ImageFormat::R32i,
            25u32 => ImageFormat::Rg32i,
            26u32 => ImageFormat::Rg16i,
            27u32 => ImageFormat::Rg8i,
            28u32 => ImageFormat::R16i,
            29u32 => ImageFormat::R8i,
            30u32 => ImageFormat::Rgba32ui,
            31u32 => ImageFormat::Rgba16ui,
            32u32 => ImageFormat::Rgba8ui,
            33u32 => ImageFormat::R32ui,
            34u32 => ImageFormat::Rgb10a2ui,
            35u32 => ImageFormat::Rg32ui,
            36u32 => ImageFormat::Rg16ui,
            37u32 => ImageFormat::Rg8ui,
            38u32 => ImageFormat::R16ui,
            39u32 => ImageFormat::R8ui,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [ImageChannelOrder](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_image_channel_order_a_image_channel_order)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum ImageChannelOrder {
    R = 0u32,
    A = 1u32,
    RG = 2u32,
    RA = 3u32,
    RGB = 4u32,
    RGBA = 5u32,
    BGRA = 6u32,
    ARGB = 7u32,
    Intensity = 8u32,
    Luminance = 9u32,
    Rx = 10u32,
    RGx = 11u32,
    RGBx = 12u32,
    Depth = 13u32,
    DepthStencil = 14u32,
    sRGB = 15u32,
    sRGBx = 16u32,
    sRGBA = 17u32,
    sBGRA = 18u32,
    ABGR = 19u32,
}
#[allow(non_upper_case_globals)]
impl ImageChannelOrder {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            ImageChannelOrder::R
            | ImageChannelOrder::A
            | ImageChannelOrder::RG
            | ImageChannelOrder::RA
            | ImageChannelOrder::RGB
            | ImageChannelOrder::RGBA
            | ImageChannelOrder::BGRA
            | ImageChannelOrder::ARGB
            | ImageChannelOrder::Intensity
            | ImageChannelOrder::Luminance
            | ImageChannelOrder::Rx
            | ImageChannelOrder::RGx
            | ImageChannelOrder::RGBx
            | ImageChannelOrder::Depth
            | ImageChannelOrder::DepthStencil
            | ImageChannelOrder::sRGB
            | ImageChannelOrder::sRGBx
            | ImageChannelOrder::sRGBA
            | ImageChannelOrder::sBGRA
            | ImageChannelOrder::ABGR => &[Capability::Kernel],
        }
    }
}
impl num_traits::FromPrimitive for ImageChannelOrder {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => ImageChannelOrder::R,
            1u32 => ImageChannelOrder::A,
            2u32 => ImageChannelOrder::RG,
            3u32 => ImageChannelOrder::RA,
            4u32 => ImageChannelOrder::RGB,
            5u32 => ImageChannelOrder::RGBA,
            6u32 => ImageChannelOrder::BGRA,
            7u32 => ImageChannelOrder::ARGB,
            8u32 => ImageChannelOrder::Intensity,
            9u32 => ImageChannelOrder::Luminance,
            10u32 => ImageChannelOrder::Rx,
            11u32 => ImageChannelOrder::RGx,
            12u32 => ImageChannelOrder::RGBx,
            13u32 => ImageChannelOrder::Depth,
            14u32 => ImageChannelOrder::DepthStencil,
            15u32 => ImageChannelOrder::sRGB,
            16u32 => ImageChannelOrder::sRGBx,
            17u32 => ImageChannelOrder::sRGBA,
            18u32 => ImageChannelOrder::sBGRA,
            19u32 => ImageChannelOrder::ABGR,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [ImageChannelDataType](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_image_channel_data_type_a_image_channel_data_type)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum ImageChannelDataType {
    SnormInt8 = 0u32,
    SnormInt16 = 1u32,
    UnormInt8 = 2u32,
    UnormInt16 = 3u32,
    UnormShort565 = 4u32,
    UnormShort555 = 5u32,
    UnormInt101010 = 6u32,
    SignedInt8 = 7u32,
    SignedInt16 = 8u32,
    SignedInt32 = 9u32,
    UnsignedInt8 = 10u32,
    UnsignedInt16 = 11u32,
    UnsignedInt32 = 12u32,
    HalfFloat = 13u32,
    Float = 14u32,
    UnormInt24 = 15u32,
    UnormInt101010_2 = 16u32,
}
#[allow(non_upper_case_globals)]
impl ImageChannelDataType {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            ImageChannelDataType::SnormInt8
            | ImageChannelDataType::SnormInt16
            | ImageChannelDataType::UnormInt8
            | ImageChannelDataType::UnormInt16
            | ImageChannelDataType::UnormShort565
            | ImageChannelDataType::UnormShort555
            | ImageChannelDataType::UnormInt101010
            | ImageChannelDataType::SignedInt8
            | ImageChannelDataType::SignedInt16
            | ImageChannelDataType::SignedInt32
            | ImageChannelDataType::UnsignedInt8
            | ImageChannelDataType::UnsignedInt16
            | ImageChannelDataType::UnsignedInt32
            | ImageChannelDataType::HalfFloat
            | ImageChannelDataType::Float
            | ImageChannelDataType::UnormInt24
            | ImageChannelDataType::UnormInt101010_2 => &[Capability::Kernel],
        }
    }
}
impl num_traits::FromPrimitive for ImageChannelDataType {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => ImageChannelDataType::SnormInt8,
            1u32 => ImageChannelDataType::SnormInt16,
            2u32 => ImageChannelDataType::UnormInt8,
            3u32 => ImageChannelDataType::UnormInt16,
            4u32 => ImageChannelDataType::UnormShort565,
            5u32 => ImageChannelDataType::UnormShort555,
            6u32 => ImageChannelDataType::UnormInt101010,
            7u32 => ImageChannelDataType::SignedInt8,
            8u32 => ImageChannelDataType::SignedInt16,
            9u32 => ImageChannelDataType::SignedInt32,
            10u32 => ImageChannelDataType::UnsignedInt8,
            11u32 => ImageChannelDataType::UnsignedInt16,
            12u32 => ImageChannelDataType::UnsignedInt32,
            13u32 => ImageChannelDataType::HalfFloat,
            14u32 => ImageChannelDataType::Float,
            15u32 => ImageChannelDataType::UnormInt24,
            16u32 => ImageChannelDataType::UnormInt101010_2,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [FPRoundingMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_fp_rounding_mode_a_fp_rounding_mode)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum FPRoundingMode {
    RTE = 0u32,
    RTZ = 1u32,
    RTP = 2u32,
    RTN = 3u32,
}
#[allow(non_upper_case_globals)]
impl FPRoundingMode {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            FPRoundingMode::RTE
            | FPRoundingMode::RTZ
            | FPRoundingMode::RTP
            | FPRoundingMode::RTN => &[],
        }
    }
}
impl num_traits::FromPrimitive for FPRoundingMode {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => FPRoundingMode::RTE,
            1u32 => FPRoundingMode::RTZ,
            2u32 => FPRoundingMode::RTP,
            3u32 => FPRoundingMode::RTN,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [LinkageType](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_linkage_type_a_linkage_type)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum LinkageType {
    Export = 0u32,
    Import = 1u32,
}
#[allow(non_upper_case_globals)]
impl LinkageType {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            LinkageType::Export | LinkageType::Import => &[Capability::Linkage],
        }
    }
}
impl num_traits::FromPrimitive for LinkageType {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => LinkageType::Export,
            1u32 => LinkageType::Import,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [AccessQualifier](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_access_qualifier_a_access_qualifier)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum AccessQualifier {
    ReadOnly = 0u32,
    WriteOnly = 1u32,
    ReadWrite = 2u32,
}
#[allow(non_upper_case_globals)]
impl AccessQualifier {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            AccessQualifier::ReadOnly | AccessQualifier::WriteOnly | AccessQualifier::ReadWrite => {
                &[Capability::Kernel]
            }
        }
    }
}
impl num_traits::FromPrimitive for AccessQualifier {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => AccessQualifier::ReadOnly,
            1u32 => AccessQualifier::WriteOnly,
            2u32 => AccessQualifier::ReadWrite,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [FunctionParameterAttribute](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_function_parameter_attribute_a_function_parameter_attribute)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum FunctionParameterAttribute {
    Zext = 0u32,
    Sext = 1u32,
    ByVal = 2u32,
    Sret = 3u32,
    NoAlias = 4u32,
    NoCapture = 5u32,
    NoWrite = 6u32,
    NoReadWrite = 7u32,
}
#[allow(non_upper_case_globals)]
impl FunctionParameterAttribute {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            FunctionParameterAttribute::Zext
            | FunctionParameterAttribute::Sext
            | FunctionParameterAttribute::ByVal
            | FunctionParameterAttribute::Sret
            | FunctionParameterAttribute::NoAlias
            | FunctionParameterAttribute::NoCapture
            | FunctionParameterAttribute::NoWrite
            | FunctionParameterAttribute::NoReadWrite => &[Capability::Kernel],
        }
    }
}
impl num_traits::FromPrimitive for FunctionParameterAttribute {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => FunctionParameterAttribute::Zext,
            1u32 => FunctionParameterAttribute::Sext,
            2u32 => FunctionParameterAttribute::ByVal,
            3u32 => FunctionParameterAttribute::Sret,
            4u32 => FunctionParameterAttribute::NoAlias,
            5u32 => FunctionParameterAttribute::NoCapture,
            6u32 => FunctionParameterAttribute::NoWrite,
            7u32 => FunctionParameterAttribute::NoReadWrite,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [Decoration](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_decoration_a_decoration)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum Decoration {
    RelaxedPrecision = 0u32,
    SpecId = 1u32,
    Block = 2u32,
    BufferBlock = 3u32,
    RowMajor = 4u32,
    ColMajor = 5u32,
    ArrayStride = 6u32,
    MatrixStride = 7u32,
    GLSLShared = 8u32,
    GLSLPacked = 9u32,
    CPacked = 10u32,
    BuiltIn = 11u32,
    NoPerspective = 13u32,
    Flat = 14u32,
    Patch = 15u32,
    Centroid = 16u32,
    Sample = 17u32,
    Invariant = 18u32,
    Restrict = 19u32,
    Aliased = 20u32,
    Volatile = 21u32,
    Constant = 22u32,
    Coherent = 23u32,
    NonWritable = 24u32,
    NonReadable = 25u32,
    Uniform = 26u32,
    UniformId = 27u32,
    SaturatedConversion = 28u32,
    Stream = 29u32,
    Location = 30u32,
    Component = 31u32,
    Index = 32u32,
    Binding = 33u32,
    DescriptorSet = 34u32,
    Offset = 35u32,
    XfbBuffer = 36u32,
    XfbStride = 37u32,
    FuncParamAttr = 38u32,
    FPRoundingMode = 39u32,
    FPFastMathMode = 40u32,
    LinkageAttributes = 41u32,
    NoContraction = 42u32,
    InputAttachmentIndex = 43u32,
    Alignment = 44u32,
    MaxByteOffset = 45u32,
    AlignmentId = 46u32,
    MaxByteOffsetId = 47u32,
    NoSignedWrap = 4469u32,
    NoUnsignedWrap = 4470u32,
    ExplicitInterpAMD = 4999u32,
    OverrideCoverageNV = 5248u32,
    PassthroughNV = 5250u32,
    ViewportRelativeNV = 5252u32,
    SecondaryViewportRelativeNV = 5256u32,
    PerPrimitiveNV = 5271u32,
    PerViewNV = 5272u32,
    PerTaskNV = 5273u32,
    PerVertexNV = 5285u32,
    NonUniform = 5300u32,
    RestrictPointer = 5355u32,
    AliasedPointer = 5356u32,
    CounterBuffer = 5634u32,
    UserSemantic = 5635u32,
    UserTypeGOOGLE = 5636u32,
}
#[allow(non_upper_case_globals)]
impl Decoration {
    pub const NonUniformEXT: Decoration = Decoration::NonUniform;
    pub const RestrictPointerEXT: Decoration = Decoration::RestrictPointer;
    pub const AliasedPointerEXT: Decoration = Decoration::AliasedPointer;
    pub const HlslCounterBufferGOOGLE: Decoration = Decoration::CounterBuffer;
    pub const HlslSemanticGOOGLE: Decoration = Decoration::UserSemantic;
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            Decoration::BuiltIn
            | Decoration::Restrict
            | Decoration::Aliased
            | Decoration::Volatile
            | Decoration::Coherent
            | Decoration::NonWritable
            | Decoration::NonReadable
            | Decoration::FPRoundingMode
            | Decoration::NoSignedWrap
            | Decoration::NoUnsignedWrap
            | Decoration::ExplicitInterpAMD
            | Decoration::CounterBuffer
            | Decoration::UserSemantic
            | Decoration::UserTypeGOOGLE => &[],
            Decoration::MaxByteOffset | Decoration::MaxByteOffsetId => &[Capability::Addresses],
            Decoration::PerVertexNV => &[Capability::FragmentBarycentricNV],
            Decoration::PassthroughNV => &[Capability::GeometryShaderPassthroughNV],
            Decoration::Stream => &[Capability::GeometryStreams],
            Decoration::InputAttachmentIndex => &[Capability::InputAttachment],
            Decoration::CPacked
            | Decoration::Constant
            | Decoration::SaturatedConversion
            | Decoration::FuncParamAttr
            | Decoration::FPFastMathMode
            | Decoration::Alignment
            | Decoration::AlignmentId => &[Capability::Kernel],
            Decoration::LinkageAttributes => &[Capability::Linkage],
            Decoration::RowMajor | Decoration::ColMajor | Decoration::MatrixStride => {
                &[Capability::Matrix]
            }
            Decoration::PerPrimitiveNV | Decoration::PerViewNV | Decoration::PerTaskNV => {
                &[Capability::MeshShadingNV]
            }
            Decoration::RestrictPointer | Decoration::AliasedPointer => {
                &[Capability::PhysicalStorageBufferAddresses]
            }
            Decoration::OverrideCoverageNV => &[Capability::SampleMaskOverrideCoverageNV],
            Decoration::Sample => &[Capability::SampleRateShading],
            Decoration::RelaxedPrecision
            | Decoration::Block
            | Decoration::BufferBlock
            | Decoration::ArrayStride
            | Decoration::GLSLShared
            | Decoration::GLSLPacked
            | Decoration::NoPerspective
            | Decoration::Flat
            | Decoration::Centroid
            | Decoration::Invariant
            | Decoration::Uniform
            | Decoration::UniformId
            | Decoration::Location
            | Decoration::Component
            | Decoration::Index
            | Decoration::Binding
            | Decoration::DescriptorSet
            | Decoration::Offset
            | Decoration::NoContraction => &[Capability::Shader],
            Decoration::SpecId => &[Capability::Shader, Capability::Kernel],
            Decoration::NonUniform => &[Capability::ShaderNonUniform],
            Decoration::SecondaryViewportRelativeNV => &[Capability::ShaderStereoViewNV],
            Decoration::ViewportRelativeNV => &[Capability::ShaderViewportMaskNV],
            Decoration::Patch => &[Capability::Tessellation],
            Decoration::XfbBuffer | Decoration::XfbStride => &[Capability::TransformFeedback],
        }
    }
}
impl num_traits::FromPrimitive for Decoration {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => Decoration::RelaxedPrecision,
            1u32 => Decoration::SpecId,
            2u32 => Decoration::Block,
            3u32 => Decoration::BufferBlock,
            4u32 => Decoration::RowMajor,
            5u32 => Decoration::ColMajor,
            6u32 => Decoration::ArrayStride,
            7u32 => Decoration::MatrixStride,
            8u32 => Decoration::GLSLShared,
            9u32 => Decoration::GLSLPacked,
            10u32 => Decoration::CPacked,
            11u32 => Decoration::BuiltIn,
            13u32 => Decoration::NoPerspective,
            14u32 => Decoration::Flat,
            15u32 => Decoration::Patch,
            16u32 => Decoration::Centroid,
            17u32 => Decoration::Sample,
            18u32 => Decoration::Invariant,
            19u32 => Decoration::Restrict,
            20u32 => Decoration::Aliased,
            21u32 => Decoration::Volatile,
            22u32 => Decoration::Constant,
            23u32 => Decoration::Coherent,
            24u32 => Decoration::NonWritable,
            25u32 => Decoration::NonReadable,
            26u32 => Decoration::Uniform,
            27u32 => Decoration::UniformId,
            28u32 => Decoration::SaturatedConversion,
            29u32 => Decoration::Stream,
            30u32 => Decoration::Location,
            31u32 => Decoration::Component,
            32u32 => Decoration::Index,
            33u32 => Decoration::Binding,
            34u32 => Decoration::DescriptorSet,
            35u32 => Decoration::Offset,
            36u32 => Decoration::XfbBuffer,
            37u32 => Decoration::XfbStride,
            38u32 => Decoration::FuncParamAttr,
            39u32 => Decoration::FPRoundingMode,
            40u32 => Decoration::FPFastMathMode,
            41u32 => Decoration::LinkageAttributes,
            42u32 => Decoration::NoContraction,
            43u32 => Decoration::InputAttachmentIndex,
            44u32 => Decoration::Alignment,
            45u32 => Decoration::MaxByteOffset,
            46u32 => Decoration::AlignmentId,
            47u32 => Decoration::MaxByteOffsetId,
            4469u32 => Decoration::NoSignedWrap,
            4470u32 => Decoration::NoUnsignedWrap,
            4999u32 => Decoration::ExplicitInterpAMD,
            5248u32 => Decoration::OverrideCoverageNV,
            5250u32 => Decoration::PassthroughNV,
            5252u32 => Decoration::ViewportRelativeNV,
            5256u32 => Decoration::SecondaryViewportRelativeNV,
            5271u32 => Decoration::PerPrimitiveNV,
            5272u32 => Decoration::PerViewNV,
            5273u32 => Decoration::PerTaskNV,
            5285u32 => Decoration::PerVertexNV,
            5300u32 => Decoration::NonUniform,
            5355u32 => Decoration::RestrictPointer,
            5356u32 => Decoration::AliasedPointer,
            5634u32 => Decoration::CounterBuffer,
            5635u32 => Decoration::UserSemantic,
            5636u32 => Decoration::UserTypeGOOGLE,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [BuiltIn](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_built_in_a_built_in)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum BuiltIn {
    Position = 0u32,
    PointSize = 1u32,
    ClipDistance = 3u32,
    CullDistance = 4u32,
    VertexId = 5u32,
    InstanceId = 6u32,
    PrimitiveId = 7u32,
    InvocationId = 8u32,
    Layer = 9u32,
    ViewportIndex = 10u32,
    TessLevelOuter = 11u32,
    TessLevelInner = 12u32,
    TessCoord = 13u32,
    PatchVertices = 14u32,
    FragCoord = 15u32,
    PointCoord = 16u32,
    FrontFacing = 17u32,
    SampleId = 18u32,
    SamplePosition = 19u32,
    SampleMask = 20u32,
    FragDepth = 22u32,
    HelperInvocation = 23u32,
    NumWorkgroups = 24u32,
    WorkgroupSize = 25u32,
    WorkgroupId = 26u32,
    LocalInvocationId = 27u32,
    GlobalInvocationId = 28u32,
    LocalInvocationIndex = 29u32,
    WorkDim = 30u32,
    GlobalSize = 31u32,
    EnqueuedWorkgroupSize = 32u32,
    GlobalOffset = 33u32,
    GlobalLinearId = 34u32,
    SubgroupSize = 36u32,
    SubgroupMaxSize = 37u32,
    NumSubgroups = 38u32,
    NumEnqueuedSubgroups = 39u32,
    SubgroupId = 40u32,
    SubgroupLocalInvocationId = 41u32,
    VertexIndex = 42u32,
    InstanceIndex = 43u32,
    SubgroupEqMask = 4416u32,
    SubgroupGeMask = 4417u32,
    SubgroupGtMask = 4418u32,
    SubgroupLeMask = 4419u32,
    SubgroupLtMask = 4420u32,
    BaseVertex = 4424u32,
    BaseInstance = 4425u32,
    DrawIndex = 4426u32,
    DeviceIndex = 4438u32,
    ViewIndex = 4440u32,
    BaryCoordNoPerspAMD = 4992u32,
    BaryCoordNoPerspCentroidAMD = 4993u32,
    BaryCoordNoPerspSampleAMD = 4994u32,
    BaryCoordSmoothAMD = 4995u32,
    BaryCoordSmoothCentroidAMD = 4996u32,
    BaryCoordSmoothSampleAMD = 4997u32,
    BaryCoordPullModelAMD = 4998u32,
    FragStencilRefEXT = 5014u32,
    ViewportMaskNV = 5253u32,
    SecondaryPositionNV = 5257u32,
    SecondaryViewportMaskNV = 5258u32,
    PositionPerViewNV = 5261u32,
    ViewportMaskPerViewNV = 5262u32,
    FullyCoveredEXT = 5264u32,
    TaskCountNV = 5274u32,
    PrimitiveCountNV = 5275u32,
    PrimitiveIndicesNV = 5276u32,
    ClipDistancePerViewNV = 5277u32,
    CullDistancePerViewNV = 5278u32,
    LayerPerViewNV = 5279u32,
    MeshViewCountNV = 5280u32,
    MeshViewIndicesNV = 5281u32,
    BaryCoordNV = 5286u32,
    BaryCoordNoPerspNV = 5287u32,
    FragSizeEXT = 5292u32,
    FragInvocationCountEXT = 5293u32,
    LaunchIdNV = 5319u32,
    LaunchSizeNV = 5320u32,
    WorldRayOriginNV = 5321u32,
    WorldRayDirectionNV = 5322u32,
    ObjectRayOriginNV = 5323u32,
    ObjectRayDirectionNV = 5324u32,
    RayTminNV = 5325u32,
    RayTmaxNV = 5326u32,
    InstanceCustomIndexNV = 5327u32,
    ObjectToWorldNV = 5330u32,
    WorldToObjectNV = 5331u32,
    HitTNV = 5332u32,
    HitKindNV = 5333u32,
    IncomingRayFlagsNV = 5351u32,
    RayGeometryIndexKHR = 5352u32,
    WarpsPerSMNV = 5374u32,
    SMCountNV = 5375u32,
    WarpIDNV = 5376u32,
    SMIDNV = 5377u32,
}
#[allow(non_upper_case_globals)]
impl BuiltIn {
    pub const SubgroupEqMaskKHR: BuiltIn = BuiltIn::SubgroupEqMask;
    pub const SubgroupGeMaskKHR: BuiltIn = BuiltIn::SubgroupGeMask;
    pub const SubgroupGtMaskKHR: BuiltIn = BuiltIn::SubgroupGtMask;
    pub const SubgroupLeMaskKHR: BuiltIn = BuiltIn::SubgroupLeMask;
    pub const SubgroupLtMaskKHR: BuiltIn = BuiltIn::SubgroupLtMask;
    pub const FragmentSizeNV: BuiltIn = BuiltIn::FragSizeEXT;
    pub const InvocationsPerPixelNV: BuiltIn = BuiltIn::FragInvocationCountEXT;
    pub const LaunchIdKHR: BuiltIn = BuiltIn::LaunchIdNV;
    pub const LaunchSizeKHR: BuiltIn = BuiltIn::LaunchSizeNV;
    pub const WorldRayOriginKHR: BuiltIn = BuiltIn::WorldRayOriginNV;
    pub const WorldRayDirectionKHR: BuiltIn = BuiltIn::WorldRayDirectionNV;
    pub const ObjectRayOriginKHR: BuiltIn = BuiltIn::ObjectRayOriginNV;
    pub const ObjectRayDirectionKHR: BuiltIn = BuiltIn::ObjectRayDirectionNV;
    pub const RayTminKHR: BuiltIn = BuiltIn::RayTminNV;
    pub const RayTmaxKHR: BuiltIn = BuiltIn::RayTmaxNV;
    pub const InstanceCustomIndexKHR: BuiltIn = BuiltIn::InstanceCustomIndexNV;
    pub const ObjectToWorldKHR: BuiltIn = BuiltIn::ObjectToWorldNV;
    pub const WorldToObjectKHR: BuiltIn = BuiltIn::WorldToObjectNV;
    pub const HitTKHR: BuiltIn = BuiltIn::HitTNV;
    pub const HitKindKHR: BuiltIn = BuiltIn::HitKindNV;
    pub const IncomingRayFlagsKHR: BuiltIn = BuiltIn::IncomingRayFlagsNV;
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            BuiltIn::NumWorkgroups
            | BuiltIn::WorkgroupSize
            | BuiltIn::WorkgroupId
            | BuiltIn::LocalInvocationId
            | BuiltIn::GlobalInvocationId
            | BuiltIn::LocalInvocationIndex
            | BuiltIn::BaryCoordNoPerspAMD
            | BuiltIn::BaryCoordNoPerspCentroidAMD
            | BuiltIn::BaryCoordNoPerspSampleAMD
            | BuiltIn::BaryCoordSmoothAMD
            | BuiltIn::BaryCoordSmoothCentroidAMD
            | BuiltIn::BaryCoordSmoothSampleAMD
            | BuiltIn::BaryCoordPullModelAMD => &[],
            BuiltIn::ClipDistance => &[Capability::ClipDistance],
            BuiltIn::CullDistance => &[Capability::CullDistance],
            BuiltIn::DeviceIndex => &[Capability::DeviceGroup],
            BuiltIn::BaseVertex | BuiltIn::BaseInstance => &[Capability::DrawParameters],
            BuiltIn::DrawIndex => &[Capability::DrawParameters, Capability::MeshShadingNV],
            BuiltIn::BaryCoordNV | BuiltIn::BaryCoordNoPerspNV => {
                &[Capability::FragmentBarycentricNV]
            }
            BuiltIn::FragSizeEXT | BuiltIn::FragInvocationCountEXT => {
                &[Capability::FragmentDensityEXT, Capability::ShadingRateNV]
            }
            BuiltIn::FullyCoveredEXT => &[Capability::FragmentFullyCoveredEXT],
            BuiltIn::Layer => &[
                Capability::Geometry,
                Capability::ShaderLayer,
                Capability::ShaderViewportIndexLayerEXT,
            ],
            BuiltIn::InvocationId => &[Capability::Geometry, Capability::Tessellation],
            BuiltIn::PrimitiveId => &[
                Capability::Geometry,
                Capability::Tessellation,
                Capability::RayTracingNV,
                Capability::RayTracingProvisionalKHR,
            ],
            BuiltIn::WorkDim
            | BuiltIn::GlobalSize
            | BuiltIn::EnqueuedWorkgroupSize
            | BuiltIn::GlobalOffset
            | BuiltIn::GlobalLinearId
            | BuiltIn::SubgroupMaxSize
            | BuiltIn::NumEnqueuedSubgroups => &[Capability::Kernel],
            BuiltIn::NumSubgroups | BuiltIn::SubgroupId => {
                &[Capability::Kernel, Capability::GroupNonUniform]
            }
            BuiltIn::SubgroupSize | BuiltIn::SubgroupLocalInvocationId => &[
                Capability::Kernel,
                Capability::GroupNonUniform,
                Capability::SubgroupBallotKHR,
            ],
            BuiltIn::TaskCountNV
            | BuiltIn::PrimitiveCountNV
            | BuiltIn::PrimitiveIndicesNV
            | BuiltIn::ClipDistancePerViewNV
            | BuiltIn::CullDistancePerViewNV
            | BuiltIn::LayerPerViewNV
            | BuiltIn::MeshViewCountNV
            | BuiltIn::MeshViewIndicesNV => &[Capability::MeshShadingNV],
            BuiltIn::ViewIndex => &[Capability::MultiView],
            BuiltIn::ViewportIndex => &[
                Capability::MultiViewport,
                Capability::ShaderViewportIndex,
                Capability::ShaderViewportIndexLayerEXT,
            ],
            BuiltIn::PositionPerViewNV | BuiltIn::ViewportMaskPerViewNV => {
                &[Capability::PerViewAttributesNV, Capability::MeshShadingNV]
            }
            BuiltIn::LaunchIdNV
            | BuiltIn::LaunchSizeNV
            | BuiltIn::WorldRayOriginNV
            | BuiltIn::WorldRayDirectionNV
            | BuiltIn::ObjectRayOriginNV
            | BuiltIn::ObjectRayDirectionNV
            | BuiltIn::RayTminNV
            | BuiltIn::RayTmaxNV
            | BuiltIn::InstanceCustomIndexNV
            | BuiltIn::ObjectToWorldNV
            | BuiltIn::WorldToObjectNV
            | BuiltIn::HitTNV
            | BuiltIn::HitKindNV
            | BuiltIn::IncomingRayFlagsNV => &[
                Capability::RayTracingNV,
                Capability::RayTracingProvisionalKHR,
            ],
            BuiltIn::RayGeometryIndexKHR => &[Capability::RayTracingProvisionalKHR],
            BuiltIn::SampleId | BuiltIn::SamplePosition => &[Capability::SampleRateShading],
            BuiltIn::Position
            | BuiltIn::PointSize
            | BuiltIn::VertexId
            | BuiltIn::InstanceId
            | BuiltIn::FragCoord
            | BuiltIn::PointCoord
            | BuiltIn::FrontFacing
            | BuiltIn::SampleMask
            | BuiltIn::FragDepth
            | BuiltIn::HelperInvocation
            | BuiltIn::VertexIndex
            | BuiltIn::InstanceIndex => &[Capability::Shader],
            BuiltIn::WarpsPerSMNV | BuiltIn::SMCountNV | BuiltIn::WarpIDNV | BuiltIn::SMIDNV => {
                &[Capability::ShaderSMBuiltinsNV]
            }
            BuiltIn::SecondaryPositionNV | BuiltIn::SecondaryViewportMaskNV => {
                &[Capability::ShaderStereoViewNV]
            }
            BuiltIn::ViewportMaskNV => {
                &[Capability::ShaderViewportMaskNV, Capability::MeshShadingNV]
            }
            BuiltIn::FragStencilRefEXT => &[Capability::StencilExportEXT],
            BuiltIn::SubgroupEqMask
            | BuiltIn::SubgroupGeMask
            | BuiltIn::SubgroupGtMask
            | BuiltIn::SubgroupLeMask
            | BuiltIn::SubgroupLtMask => &[
                Capability::SubgroupBallotKHR,
                Capability::GroupNonUniformBallot,
            ],
            BuiltIn::TessLevelOuter
            | BuiltIn::TessLevelInner
            | BuiltIn::TessCoord
            | BuiltIn::PatchVertices => &[Capability::Tessellation],
        }
    }
}
impl num_traits::FromPrimitive for BuiltIn {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => BuiltIn::Position,
            1u32 => BuiltIn::PointSize,
            3u32 => BuiltIn::ClipDistance,
            4u32 => BuiltIn::CullDistance,
            5u32 => BuiltIn::VertexId,
            6u32 => BuiltIn::InstanceId,
            7u32 => BuiltIn::PrimitiveId,
            8u32 => BuiltIn::InvocationId,
            9u32 => BuiltIn::Layer,
            10u32 => BuiltIn::ViewportIndex,
            11u32 => BuiltIn::TessLevelOuter,
            12u32 => BuiltIn::TessLevelInner,
            13u32 => BuiltIn::TessCoord,
            14u32 => BuiltIn::PatchVertices,
            15u32 => BuiltIn::FragCoord,
            16u32 => BuiltIn::PointCoord,
            17u32 => BuiltIn::FrontFacing,
            18u32 => BuiltIn::SampleId,
            19u32 => BuiltIn::SamplePosition,
            20u32 => BuiltIn::SampleMask,
            22u32 => BuiltIn::FragDepth,
            23u32 => BuiltIn::HelperInvocation,
            24u32 => BuiltIn::NumWorkgroups,
            25u32 => BuiltIn::WorkgroupSize,
            26u32 => BuiltIn::WorkgroupId,
            27u32 => BuiltIn::LocalInvocationId,
            28u32 => BuiltIn::GlobalInvocationId,
            29u32 => BuiltIn::LocalInvocationIndex,
            30u32 => BuiltIn::WorkDim,
            31u32 => BuiltIn::GlobalSize,
            32u32 => BuiltIn::EnqueuedWorkgroupSize,
            33u32 => BuiltIn::GlobalOffset,
            34u32 => BuiltIn::GlobalLinearId,
            36u32 => BuiltIn::SubgroupSize,
            37u32 => BuiltIn::SubgroupMaxSize,
            38u32 => BuiltIn::NumSubgroups,
            39u32 => BuiltIn::NumEnqueuedSubgroups,
            40u32 => BuiltIn::SubgroupId,
            41u32 => BuiltIn::SubgroupLocalInvocationId,
            42u32 => BuiltIn::VertexIndex,
            43u32 => BuiltIn::InstanceIndex,
            4416u32 => BuiltIn::SubgroupEqMask,
            4417u32 => BuiltIn::SubgroupGeMask,
            4418u32 => BuiltIn::SubgroupGtMask,
            4419u32 => BuiltIn::SubgroupLeMask,
            4420u32 => BuiltIn::SubgroupLtMask,
            4424u32 => BuiltIn::BaseVertex,
            4425u32 => BuiltIn::BaseInstance,
            4426u32 => BuiltIn::DrawIndex,
            4438u32 => BuiltIn::DeviceIndex,
            4440u32 => BuiltIn::ViewIndex,
            4992u32 => BuiltIn::BaryCoordNoPerspAMD,
            4993u32 => BuiltIn::BaryCoordNoPerspCentroidAMD,
            4994u32 => BuiltIn::BaryCoordNoPerspSampleAMD,
            4995u32 => BuiltIn::BaryCoordSmoothAMD,
            4996u32 => BuiltIn::BaryCoordSmoothCentroidAMD,
            4997u32 => BuiltIn::BaryCoordSmoothSampleAMD,
            4998u32 => BuiltIn::BaryCoordPullModelAMD,
            5014u32 => BuiltIn::FragStencilRefEXT,
            5253u32 => BuiltIn::ViewportMaskNV,
            5257u32 => BuiltIn::SecondaryPositionNV,
            5258u32 => BuiltIn::SecondaryViewportMaskNV,
            5261u32 => BuiltIn::PositionPerViewNV,
            5262u32 => BuiltIn::ViewportMaskPerViewNV,
            5264u32 => BuiltIn::FullyCoveredEXT,
            5274u32 => BuiltIn::TaskCountNV,
            5275u32 => BuiltIn::PrimitiveCountNV,
            5276u32 => BuiltIn::PrimitiveIndicesNV,
            5277u32 => BuiltIn::ClipDistancePerViewNV,
            5278u32 => BuiltIn::CullDistancePerViewNV,
            5279u32 => BuiltIn::LayerPerViewNV,
            5280u32 => BuiltIn::MeshViewCountNV,
            5281u32 => BuiltIn::MeshViewIndicesNV,
            5286u32 => BuiltIn::BaryCoordNV,
            5287u32 => BuiltIn::BaryCoordNoPerspNV,
            5292u32 => BuiltIn::FragSizeEXT,
            5293u32 => BuiltIn::FragInvocationCountEXT,
            5319u32 => BuiltIn::LaunchIdNV,
            5320u32 => BuiltIn::LaunchSizeNV,
            5321u32 => BuiltIn::WorldRayOriginNV,
            5322u32 => BuiltIn::WorldRayDirectionNV,
            5323u32 => BuiltIn::ObjectRayOriginNV,
            5324u32 => BuiltIn::ObjectRayDirectionNV,
            5325u32 => BuiltIn::RayTminNV,
            5326u32 => BuiltIn::RayTmaxNV,
            5327u32 => BuiltIn::InstanceCustomIndexNV,
            5330u32 => BuiltIn::ObjectToWorldNV,
            5331u32 => BuiltIn::WorldToObjectNV,
            5332u32 => BuiltIn::HitTNV,
            5333u32 => BuiltIn::HitKindNV,
            5351u32 => BuiltIn::IncomingRayFlagsNV,
            5352u32 => BuiltIn::RayGeometryIndexKHR,
            5374u32 => BuiltIn::WarpsPerSMNV,
            5375u32 => BuiltIn::SMCountNV,
            5376u32 => BuiltIn::WarpIDNV,
            5377u32 => BuiltIn::SMIDNV,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [Scope](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_scope_a_scope)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum Scope {
    CrossDevice = 0u32,
    Device = 1u32,
    Workgroup = 2u32,
    Subgroup = 3u32,
    Invocation = 4u32,
    QueueFamily = 5u32,
    ShaderCallKHR = 6u32,
}
#[allow(non_upper_case_globals)]
impl Scope {
    pub const QueueFamilyKHR: Scope = Scope::QueueFamily;
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            Scope::CrossDevice
            | Scope::Device
            | Scope::Workgroup
            | Scope::Subgroup
            | Scope::Invocation => &[],
            Scope::ShaderCallKHR => &[Capability::RayTracingProvisionalKHR],
            Scope::QueueFamily => &[Capability::VulkanMemoryModel],
        }
    }
}
impl num_traits::FromPrimitive for Scope {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => Scope::CrossDevice,
            1u32 => Scope::Device,
            2u32 => Scope::Workgroup,
            3u32 => Scope::Subgroup,
            4u32 => Scope::Invocation,
            5u32 => Scope::QueueFamily,
            6u32 => Scope::ShaderCallKHR,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [GroupOperation](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_group_operation_a_group_operation)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum GroupOperation {
    Reduce = 0u32,
    InclusiveScan = 1u32,
    ExclusiveScan = 2u32,
    ClusteredReduce = 3u32,
    PartitionedReduceNV = 6u32,
    PartitionedInclusiveScanNV = 7u32,
    PartitionedExclusiveScanNV = 8u32,
}
#[allow(non_upper_case_globals)]
impl GroupOperation {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            GroupOperation::ClusteredReduce => &[Capability::GroupNonUniformClustered],
            GroupOperation::PartitionedReduceNV
            | GroupOperation::PartitionedInclusiveScanNV
            | GroupOperation::PartitionedExclusiveScanNV => {
                &[Capability::GroupNonUniformPartitionedNV]
            }
            GroupOperation::Reduce
            | GroupOperation::InclusiveScan
            | GroupOperation::ExclusiveScan => &[
                Capability::Kernel,
                Capability::GroupNonUniformArithmetic,
                Capability::GroupNonUniformBallot,
            ],
        }
    }
}
impl num_traits::FromPrimitive for GroupOperation {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => GroupOperation::Reduce,
            1u32 => GroupOperation::InclusiveScan,
            2u32 => GroupOperation::ExclusiveScan,
            3u32 => GroupOperation::ClusteredReduce,
            6u32 => GroupOperation::PartitionedReduceNV,
            7u32 => GroupOperation::PartitionedInclusiveScanNV,
            8u32 => GroupOperation::PartitionedExclusiveScanNV,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [KernelEnqueueFlags](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_kernel_enqueue_flags_a_kernel_enqueue_flags)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum KernelEnqueueFlags {
    NoWait = 0u32,
    WaitKernel = 1u32,
    WaitWorkGroup = 2u32,
}
#[allow(non_upper_case_globals)]
impl KernelEnqueueFlags {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            KernelEnqueueFlags::NoWait
            | KernelEnqueueFlags::WaitKernel
            | KernelEnqueueFlags::WaitWorkGroup => &[Capability::Kernel],
        }
    }
}
impl num_traits::FromPrimitive for KernelEnqueueFlags {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => KernelEnqueueFlags::NoWait,
            1u32 => KernelEnqueueFlags::WaitKernel,
            2u32 => KernelEnqueueFlags::WaitWorkGroup,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [Capability](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_capability_a_capability)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum Capability {
    Matrix = 0u32,
    Shader = 1u32,
    Geometry = 2u32,
    Tessellation = 3u32,
    Addresses = 4u32,
    Linkage = 5u32,
    Kernel = 6u32,
    Vector16 = 7u32,
    Float16Buffer = 8u32,
    Float16 = 9u32,
    Float64 = 10u32,
    Int64 = 11u32,
    Int64Atomics = 12u32,
    ImageBasic = 13u32,
    ImageReadWrite = 14u32,
    ImageMipmap = 15u32,
    Pipes = 17u32,
    Groups = 18u32,
    DeviceEnqueue = 19u32,
    LiteralSampler = 20u32,
    AtomicStorage = 21u32,
    Int16 = 22u32,
    TessellationPointSize = 23u32,
    GeometryPointSize = 24u32,
    ImageGatherExtended = 25u32,
    StorageImageMultisample = 27u32,
    UniformBufferArrayDynamicIndexing = 28u32,
    SampledImageArrayDynamicIndexing = 29u32,
    StorageBufferArrayDynamicIndexing = 30u32,
    StorageImageArrayDynamicIndexing = 31u32,
    ClipDistance = 32u32,
    CullDistance = 33u32,
    ImageCubeArray = 34u32,
    SampleRateShading = 35u32,
    ImageRect = 36u32,
    SampledRect = 37u32,
    GenericPointer = 38u32,
    Int8 = 39u32,
    InputAttachment = 40u32,
    SparseResidency = 41u32,
    MinLod = 42u32,
    Sampled1D = 43u32,
    Image1D = 44u32,
    SampledCubeArray = 45u32,
    SampledBuffer = 46u32,
    ImageBuffer = 47u32,
    ImageMSArray = 48u32,
    StorageImageExtendedFormats = 49u32,
    ImageQuery = 50u32,
    DerivativeControl = 51u32,
    InterpolationFunction = 52u32,
    TransformFeedback = 53u32,
    GeometryStreams = 54u32,
    StorageImageReadWithoutFormat = 55u32,
    StorageImageWriteWithoutFormat = 56u32,
    MultiViewport = 57u32,
    SubgroupDispatch = 58u32,
    NamedBarrier = 59u32,
    PipeStorage = 60u32,
    GroupNonUniform = 61u32,
    GroupNonUniformVote = 62u32,
    GroupNonUniformArithmetic = 63u32,
    GroupNonUniformBallot = 64u32,
    GroupNonUniformShuffle = 65u32,
    GroupNonUniformShuffleRelative = 66u32,
    GroupNonUniformClustered = 67u32,
    GroupNonUniformQuad = 68u32,
    ShaderLayer = 69u32,
    ShaderViewportIndex = 70u32,
    SubgroupBallotKHR = 4423u32,
    DrawParameters = 4427u32,
    SubgroupVoteKHR = 4431u32,
    StorageBuffer16BitAccess = 4433u32,
    UniformAndStorageBuffer16BitAccess = 4434u32,
    StoragePushConstant16 = 4435u32,
    StorageInputOutput16 = 4436u32,
    DeviceGroup = 4437u32,
    MultiView = 4439u32,
    VariablePointersStorageBuffer = 4441u32,
    VariablePointers = 4442u32,
    AtomicStorageOps = 4445u32,
    SampleMaskPostDepthCoverage = 4447u32,
    StorageBuffer8BitAccess = 4448u32,
    UniformAndStorageBuffer8BitAccess = 4449u32,
    StoragePushConstant8 = 4450u32,
    DenormPreserve = 4464u32,
    DenormFlushToZero = 4465u32,
    SignedZeroInfNanPreserve = 4466u32,
    RoundingModeRTE = 4467u32,
    RoundingModeRTZ = 4468u32,
    RayQueryProvisionalKHR = 4471u32,
    RayTraversalPrimitiveCullingProvisionalKHR = 4478u32,
    Float16ImageAMD = 5008u32,
    ImageGatherBiasLodAMD = 5009u32,
    FragmentMaskAMD = 5010u32,
    StencilExportEXT = 5013u32,
    ImageReadWriteLodAMD = 5015u32,
    ShaderClockKHR = 5055u32,
    SampleMaskOverrideCoverageNV = 5249u32,
    GeometryShaderPassthroughNV = 5251u32,
    ShaderViewportIndexLayerEXT = 5254u32,
    ShaderViewportMaskNV = 5255u32,
    ShaderStereoViewNV = 5259u32,
    PerViewAttributesNV = 5260u32,
    FragmentFullyCoveredEXT = 5265u32,
    MeshShadingNV = 5266u32,
    ImageFootprintNV = 5282u32,
    FragmentBarycentricNV = 5284u32,
    ComputeDerivativeGroupQuadsNV = 5288u32,
    FragmentDensityEXT = 5291u32,
    GroupNonUniformPartitionedNV = 5297u32,
    ShaderNonUniform = 5301u32,
    RuntimeDescriptorArray = 5302u32,
    InputAttachmentArrayDynamicIndexing = 5303u32,
    UniformTexelBufferArrayDynamicIndexing = 5304u32,
    StorageTexelBufferArrayDynamicIndexing = 5305u32,
    UniformBufferArrayNonUniformIndexing = 5306u32,
    SampledImageArrayNonUniformIndexing = 5307u32,
    StorageBufferArrayNonUniformIndexing = 5308u32,
    StorageImageArrayNonUniformIndexing = 5309u32,
    InputAttachmentArrayNonUniformIndexing = 5310u32,
    UniformTexelBufferArrayNonUniformIndexing = 5311u32,
    StorageTexelBufferArrayNonUniformIndexing = 5312u32,
    RayTracingNV = 5340u32,
    VulkanMemoryModel = 5345u32,
    VulkanMemoryModelDeviceScope = 5346u32,
    PhysicalStorageBufferAddresses = 5347u32,
    ComputeDerivativeGroupLinearNV = 5350u32,
    RayTracingProvisionalKHR = 5353u32,
    CooperativeMatrixNV = 5357u32,
    FragmentShaderSampleInterlockEXT = 5363u32,
    FragmentShaderShadingRateInterlockEXT = 5372u32,
    ShaderSMBuiltinsNV = 5373u32,
    FragmentShaderPixelInterlockEXT = 5378u32,
    DemoteToHelperInvocationEXT = 5379u32,
    SubgroupShuffleINTEL = 5568u32,
    SubgroupBufferBlockIOINTEL = 5569u32,
    SubgroupImageBlockIOINTEL = 5570u32,
    SubgroupImageMediaBlockIOINTEL = 5579u32,
    IntegerFunctions2INTEL = 5584u32,
    SubgroupAvcMotionEstimationINTEL = 5696u32,
    SubgroupAvcMotionEstimationIntraINTEL = 5697u32,
    SubgroupAvcMotionEstimationChromaINTEL = 5698u32,
}
#[allow(non_upper_case_globals)]
impl Capability {
    pub const StorageUniformBufferBlock16: Capability = Capability::StorageBuffer16BitAccess;
    pub const StorageUniform16: Capability = Capability::UniformAndStorageBuffer16BitAccess;
    pub const ShaderViewportIndexLayerNV: Capability = Capability::ShaderViewportIndexLayerEXT;
    pub const ShadingRateNV: Capability = Capability::FragmentDensityEXT;
    pub const ShaderNonUniformEXT: Capability = Capability::ShaderNonUniform;
    pub const RuntimeDescriptorArrayEXT: Capability = Capability::RuntimeDescriptorArray;
    pub const InputAttachmentArrayDynamicIndexingEXT: Capability =
        Capability::InputAttachmentArrayDynamicIndexing;
    pub const UniformTexelBufferArrayDynamicIndexingEXT: Capability =
        Capability::UniformTexelBufferArrayDynamicIndexing;
    pub const StorageTexelBufferArrayDynamicIndexingEXT: Capability =
        Capability::StorageTexelBufferArrayDynamicIndexing;
    pub const UniformBufferArrayNonUniformIndexingEXT: Capability =
        Capability::UniformBufferArrayNonUniformIndexing;
    pub const SampledImageArrayNonUniformIndexingEXT: Capability =
        Capability::SampledImageArrayNonUniformIndexing;
    pub const StorageBufferArrayNonUniformIndexingEXT: Capability =
        Capability::StorageBufferArrayNonUniformIndexing;
    pub const StorageImageArrayNonUniformIndexingEXT: Capability =
        Capability::StorageImageArrayNonUniformIndexing;
    pub const InputAttachmentArrayNonUniformIndexingEXT: Capability =
        Capability::InputAttachmentArrayNonUniformIndexing;
    pub const UniformTexelBufferArrayNonUniformIndexingEXT: Capability =
        Capability::UniformTexelBufferArrayNonUniformIndexing;
    pub const StorageTexelBufferArrayNonUniformIndexingEXT: Capability =
        Capability::StorageTexelBufferArrayNonUniformIndexing;
    pub const VulkanMemoryModelKHR: Capability = Capability::VulkanMemoryModel;
    pub const VulkanMemoryModelDeviceScopeKHR: Capability =
        Capability::VulkanMemoryModelDeviceScope;
    pub const PhysicalStorageBufferAddressesEXT: Capability =
        Capability::PhysicalStorageBufferAddresses;
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            Capability::Matrix
            | Capability::Addresses
            | Capability::Linkage
            | Capability::Kernel
            | Capability::Float16
            | Capability::Float64
            | Capability::Int64
            | Capability::Groups
            | Capability::Int16
            | Capability::Int8
            | Capability::Sampled1D
            | Capability::SampledBuffer
            | Capability::GroupNonUniform
            | Capability::ShaderLayer
            | Capability::ShaderViewportIndex
            | Capability::SubgroupBallotKHR
            | Capability::SubgroupVoteKHR
            | Capability::StorageBuffer16BitAccess
            | Capability::StoragePushConstant16
            | Capability::StorageInputOutput16
            | Capability::DeviceGroup
            | Capability::AtomicStorageOps
            | Capability::SampleMaskPostDepthCoverage
            | Capability::StorageBuffer8BitAccess
            | Capability::StoragePushConstant8
            | Capability::DenormPreserve
            | Capability::DenormFlushToZero
            | Capability::SignedZeroInfNanPreserve
            | Capability::RoundingModeRTE
            | Capability::RoundingModeRTZ
            | Capability::ImageFootprintNV
            | Capability::FragmentBarycentricNV
            | Capability::ComputeDerivativeGroupQuadsNV
            | Capability::GroupNonUniformPartitionedNV
            | Capability::VulkanMemoryModel
            | Capability::VulkanMemoryModelDeviceScope
            | Capability::ComputeDerivativeGroupLinearNV
            | Capability::SubgroupShuffleINTEL
            | Capability::SubgroupBufferBlockIOINTEL
            | Capability::SubgroupImageBlockIOINTEL
            | Capability::SubgroupImageMediaBlockIOINTEL
            | Capability::SubgroupAvcMotionEstimationINTEL
            | Capability::SubgroupAvcMotionEstimationIntraINTEL
            | Capability::SubgroupAvcMotionEstimationChromaINTEL => &[],
            Capability::GenericPointer => &[Capability::Addresses],
            Capability::SubgroupDispatch => &[Capability::DeviceEnqueue],
            Capability::GeometryPointSize
            | Capability::GeometryStreams
            | Capability::MultiViewport
            | Capability::GeometryShaderPassthroughNV => &[Capability::Geometry],
            Capability::GroupNonUniformVote
            | Capability::GroupNonUniformArithmetic
            | Capability::GroupNonUniformBallot
            | Capability::GroupNonUniformShuffle
            | Capability::GroupNonUniformShuffleRelative
            | Capability::GroupNonUniformClustered
            | Capability::GroupNonUniformQuad => &[Capability::GroupNonUniform],
            Capability::ImageReadWrite | Capability::ImageMipmap => &[Capability::ImageBasic],
            Capability::StorageTexelBufferArrayDynamicIndexing => &[Capability::ImageBuffer],
            Capability::StorageTexelBufferArrayNonUniformIndexing => {
                &[Capability::ImageBuffer, Capability::ShaderNonUniform]
            }
            Capability::InputAttachmentArrayDynamicIndexing => &[Capability::InputAttachment],
            Capability::InputAttachmentArrayNonUniformIndexing => {
                &[Capability::InputAttachment, Capability::ShaderNonUniform]
            }
            Capability::Int64Atomics => &[Capability::Int64],
            Capability::Vector16
            | Capability::Float16Buffer
            | Capability::ImageBasic
            | Capability::Pipes
            | Capability::DeviceEnqueue
            | Capability::LiteralSampler
            | Capability::NamedBarrier => &[Capability::Kernel],
            Capability::Shader => &[Capability::Matrix],
            Capability::PerViewAttributesNV => &[Capability::MultiView],
            Capability::ShaderViewportIndexLayerEXT => &[Capability::MultiViewport],
            Capability::PipeStorage => &[Capability::Pipes],
            Capability::RayTraversalPrimitiveCullingProvisionalKHR => &[
                Capability::RayQueryProvisionalKHR,
                Capability::RayTracingProvisionalKHR,
            ],
            Capability::SampleMaskOverrideCoverageNV => &[Capability::SampleRateShading],
            Capability::Image1D => &[Capability::Sampled1D],
            Capability::ImageBuffer | Capability::UniformTexelBufferArrayDynamicIndexing => {
                &[Capability::SampledBuffer]
            }
            Capability::UniformTexelBufferArrayNonUniformIndexing => {
                &[Capability::SampledBuffer, Capability::ShaderNonUniform]
            }
            Capability::ImageCubeArray => &[Capability::SampledCubeArray],
            Capability::ImageRect => &[Capability::SampledRect],
            Capability::Geometry
            | Capability::Tessellation
            | Capability::AtomicStorage
            | Capability::ImageGatherExtended
            | Capability::StorageImageMultisample
            | Capability::UniformBufferArrayDynamicIndexing
            | Capability::SampledImageArrayDynamicIndexing
            | Capability::StorageBufferArrayDynamicIndexing
            | Capability::StorageImageArrayDynamicIndexing
            | Capability::ClipDistance
            | Capability::CullDistance
            | Capability::SampleRateShading
            | Capability::SampledRect
            | Capability::InputAttachment
            | Capability::SparseResidency
            | Capability::MinLod
            | Capability::SampledCubeArray
            | Capability::ImageMSArray
            | Capability::StorageImageExtendedFormats
            | Capability::ImageQuery
            | Capability::DerivativeControl
            | Capability::InterpolationFunction
            | Capability::TransformFeedback
            | Capability::StorageImageReadWithoutFormat
            | Capability::StorageImageWriteWithoutFormat
            | Capability::DrawParameters
            | Capability::MultiView
            | Capability::VariablePointersStorageBuffer
            | Capability::RayQueryProvisionalKHR
            | Capability::Float16ImageAMD
            | Capability::ImageGatherBiasLodAMD
            | Capability::FragmentMaskAMD
            | Capability::StencilExportEXT
            | Capability::ImageReadWriteLodAMD
            | Capability::ShaderClockKHR
            | Capability::FragmentFullyCoveredEXT
            | Capability::MeshShadingNV
            | Capability::FragmentDensityEXT
            | Capability::ShaderNonUniform
            | Capability::RuntimeDescriptorArray
            | Capability::RayTracingNV
            | Capability::PhysicalStorageBufferAddresses
            | Capability::RayTracingProvisionalKHR
            | Capability::CooperativeMatrixNV
            | Capability::FragmentShaderSampleInterlockEXT
            | Capability::FragmentShaderShadingRateInterlockEXT
            | Capability::ShaderSMBuiltinsNV
            | Capability::FragmentShaderPixelInterlockEXT
            | Capability::DemoteToHelperInvocationEXT
            | Capability::IntegerFunctions2INTEL => &[Capability::Shader],
            Capability::UniformBufferArrayNonUniformIndexing
            | Capability::SampledImageArrayNonUniformIndexing
            | Capability::StorageBufferArrayNonUniformIndexing
            | Capability::StorageImageArrayNonUniformIndexing => &[Capability::ShaderNonUniform],
            Capability::ShaderViewportMaskNV => &[Capability::ShaderViewportIndexLayerNV],
            Capability::ShaderStereoViewNV => &[Capability::ShaderViewportMaskNV],
            Capability::UniformAndStorageBuffer16BitAccess => &[
                Capability::StorageBuffer16BitAccess,
                Capability::StorageUniformBufferBlock16,
            ],
            Capability::UniformAndStorageBuffer8BitAccess => &[Capability::StorageBuffer8BitAccess],
            Capability::TessellationPointSize => &[Capability::Tessellation],
            Capability::VariablePointers => &[Capability::VariablePointersStorageBuffer],
        }
    }
}
impl num_traits::FromPrimitive for Capability {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => Capability::Matrix,
            1u32 => Capability::Shader,
            2u32 => Capability::Geometry,
            3u32 => Capability::Tessellation,
            4u32 => Capability::Addresses,
            5u32 => Capability::Linkage,
            6u32 => Capability::Kernel,
            7u32 => Capability::Vector16,
            8u32 => Capability::Float16Buffer,
            9u32 => Capability::Float16,
            10u32 => Capability::Float64,
            11u32 => Capability::Int64,
            12u32 => Capability::Int64Atomics,
            13u32 => Capability::ImageBasic,
            14u32 => Capability::ImageReadWrite,
            15u32 => Capability::ImageMipmap,
            17u32 => Capability::Pipes,
            18u32 => Capability::Groups,
            19u32 => Capability::DeviceEnqueue,
            20u32 => Capability::LiteralSampler,
            21u32 => Capability::AtomicStorage,
            22u32 => Capability::Int16,
            23u32 => Capability::TessellationPointSize,
            24u32 => Capability::GeometryPointSize,
            25u32 => Capability::ImageGatherExtended,
            27u32 => Capability::StorageImageMultisample,
            28u32 => Capability::UniformBufferArrayDynamicIndexing,
            29u32 => Capability::SampledImageArrayDynamicIndexing,
            30u32 => Capability::StorageBufferArrayDynamicIndexing,
            31u32 => Capability::StorageImageArrayDynamicIndexing,
            32u32 => Capability::ClipDistance,
            33u32 => Capability::CullDistance,
            34u32 => Capability::ImageCubeArray,
            35u32 => Capability::SampleRateShading,
            36u32 => Capability::ImageRect,
            37u32 => Capability::SampledRect,
            38u32 => Capability::GenericPointer,
            39u32 => Capability::Int8,
            40u32 => Capability::InputAttachment,
            41u32 => Capability::SparseResidency,
            42u32 => Capability::MinLod,
            43u32 => Capability::Sampled1D,
            44u32 => Capability::Image1D,
            45u32 => Capability::SampledCubeArray,
            46u32 => Capability::SampledBuffer,
            47u32 => Capability::ImageBuffer,
            48u32 => Capability::ImageMSArray,
            49u32 => Capability::StorageImageExtendedFormats,
            50u32 => Capability::ImageQuery,
            51u32 => Capability::DerivativeControl,
            52u32 => Capability::InterpolationFunction,
            53u32 => Capability::TransformFeedback,
            54u32 => Capability::GeometryStreams,
            55u32 => Capability::StorageImageReadWithoutFormat,
            56u32 => Capability::StorageImageWriteWithoutFormat,
            57u32 => Capability::MultiViewport,
            58u32 => Capability::SubgroupDispatch,
            59u32 => Capability::NamedBarrier,
            60u32 => Capability::PipeStorage,
            61u32 => Capability::GroupNonUniform,
            62u32 => Capability::GroupNonUniformVote,
            63u32 => Capability::GroupNonUniformArithmetic,
            64u32 => Capability::GroupNonUniformBallot,
            65u32 => Capability::GroupNonUniformShuffle,
            66u32 => Capability::GroupNonUniformShuffleRelative,
            67u32 => Capability::GroupNonUniformClustered,
            68u32 => Capability::GroupNonUniformQuad,
            69u32 => Capability::ShaderLayer,
            70u32 => Capability::ShaderViewportIndex,
            4423u32 => Capability::SubgroupBallotKHR,
            4427u32 => Capability::DrawParameters,
            4431u32 => Capability::SubgroupVoteKHR,
            4433u32 => Capability::StorageBuffer16BitAccess,
            4434u32 => Capability::UniformAndStorageBuffer16BitAccess,
            4435u32 => Capability::StoragePushConstant16,
            4436u32 => Capability::StorageInputOutput16,
            4437u32 => Capability::DeviceGroup,
            4439u32 => Capability::MultiView,
            4441u32 => Capability::VariablePointersStorageBuffer,
            4442u32 => Capability::VariablePointers,
            4445u32 => Capability::AtomicStorageOps,
            4447u32 => Capability::SampleMaskPostDepthCoverage,
            4448u32 => Capability::StorageBuffer8BitAccess,
            4449u32 => Capability::UniformAndStorageBuffer8BitAccess,
            4450u32 => Capability::StoragePushConstant8,
            4464u32 => Capability::DenormPreserve,
            4465u32 => Capability::DenormFlushToZero,
            4466u32 => Capability::SignedZeroInfNanPreserve,
            4467u32 => Capability::RoundingModeRTE,
            4468u32 => Capability::RoundingModeRTZ,
            4471u32 => Capability::RayQueryProvisionalKHR,
            4478u32 => Capability::RayTraversalPrimitiveCullingProvisionalKHR,
            5008u32 => Capability::Float16ImageAMD,
            5009u32 => Capability::ImageGatherBiasLodAMD,
            5010u32 => Capability::FragmentMaskAMD,
            5013u32 => Capability::StencilExportEXT,
            5015u32 => Capability::ImageReadWriteLodAMD,
            5055u32 => Capability::ShaderClockKHR,
            5249u32 => Capability::SampleMaskOverrideCoverageNV,
            5251u32 => Capability::GeometryShaderPassthroughNV,
            5254u32 => Capability::ShaderViewportIndexLayerEXT,
            5255u32 => Capability::ShaderViewportMaskNV,
            5259u32 => Capability::ShaderStereoViewNV,
            5260u32 => Capability::PerViewAttributesNV,
            5265u32 => Capability::FragmentFullyCoveredEXT,
            5266u32 => Capability::MeshShadingNV,
            5282u32 => Capability::ImageFootprintNV,
            5284u32 => Capability::FragmentBarycentricNV,
            5288u32 => Capability::ComputeDerivativeGroupQuadsNV,
            5291u32 => Capability::FragmentDensityEXT,
            5297u32 => Capability::GroupNonUniformPartitionedNV,
            5301u32 => Capability::ShaderNonUniform,
            5302u32 => Capability::RuntimeDescriptorArray,
            5303u32 => Capability::InputAttachmentArrayDynamicIndexing,
            5304u32 => Capability::UniformTexelBufferArrayDynamicIndexing,
            5305u32 => Capability::StorageTexelBufferArrayDynamicIndexing,
            5306u32 => Capability::UniformBufferArrayNonUniformIndexing,
            5307u32 => Capability::SampledImageArrayNonUniformIndexing,
            5308u32 => Capability::StorageBufferArrayNonUniformIndexing,
            5309u32 => Capability::StorageImageArrayNonUniformIndexing,
            5310u32 => Capability::InputAttachmentArrayNonUniformIndexing,
            5311u32 => Capability::UniformTexelBufferArrayNonUniformIndexing,
            5312u32 => Capability::StorageTexelBufferArrayNonUniformIndexing,
            5340u32 => Capability::RayTracingNV,
            5345u32 => Capability::VulkanMemoryModel,
            5346u32 => Capability::VulkanMemoryModelDeviceScope,
            5347u32 => Capability::PhysicalStorageBufferAddresses,
            5350u32 => Capability::ComputeDerivativeGroupLinearNV,
            5353u32 => Capability::RayTracingProvisionalKHR,
            5357u32 => Capability::CooperativeMatrixNV,
            5363u32 => Capability::FragmentShaderSampleInterlockEXT,
            5372u32 => Capability::FragmentShaderShadingRateInterlockEXT,
            5373u32 => Capability::ShaderSMBuiltinsNV,
            5378u32 => Capability::FragmentShaderPixelInterlockEXT,
            5379u32 => Capability::DemoteToHelperInvocationEXT,
            5568u32 => Capability::SubgroupShuffleINTEL,
            5569u32 => Capability::SubgroupBufferBlockIOINTEL,
            5570u32 => Capability::SubgroupImageBlockIOINTEL,
            5579u32 => Capability::SubgroupImageMediaBlockIOINTEL,
            5584u32 => Capability::IntegerFunctions2INTEL,
            5696u32 => Capability::SubgroupAvcMotionEstimationINTEL,
            5697u32 => Capability::SubgroupAvcMotionEstimationIntraINTEL,
            5698u32 => Capability::SubgroupAvcMotionEstimationChromaINTEL,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [RayQueryIntersection](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_ray_query_intersection_a_ray_query_intersection)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum RayQueryIntersection {
    RayQueryCandidateIntersectionKHR = 0u32,
    RayQueryCommittedIntersectionKHR = 1u32,
}
#[allow(non_upper_case_globals)]
impl RayQueryIntersection {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            RayQueryIntersection::RayQueryCandidateIntersectionKHR
            | RayQueryIntersection::RayQueryCommittedIntersectionKHR => {
                &[Capability::RayQueryProvisionalKHR]
            }
        }
    }
}
impl num_traits::FromPrimitive for RayQueryIntersection {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => RayQueryIntersection::RayQueryCandidateIntersectionKHR,
            1u32 => RayQueryIntersection::RayQueryCommittedIntersectionKHR,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [RayQueryCommittedIntersectionType](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_ray_query_committed_intersection_type_a_ray_query_committed_intersection_type)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum RayQueryCommittedIntersectionType {
    RayQueryCommittedIntersectionNoneKHR = 0u32,
    RayQueryCommittedIntersectionTriangleKHR = 1u32,
    RayQueryCommittedIntersectionGeneratedKHR = 2u32,
}
#[allow(non_upper_case_globals)]
impl RayQueryCommittedIntersectionType {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            RayQueryCommittedIntersectionType::RayQueryCommittedIntersectionNoneKHR
            | RayQueryCommittedIntersectionType::RayQueryCommittedIntersectionTriangleKHR
            | RayQueryCommittedIntersectionType::RayQueryCommittedIntersectionGeneratedKHR => {
                &[Capability::RayQueryProvisionalKHR]
            }
        }
    }
}
impl num_traits::FromPrimitive for RayQueryCommittedIntersectionType {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => RayQueryCommittedIntersectionType::RayQueryCommittedIntersectionNoneKHR,
            1u32 => RayQueryCommittedIntersectionType::RayQueryCommittedIntersectionTriangleKHR,
            2u32 => RayQueryCommittedIntersectionType::RayQueryCommittedIntersectionGeneratedKHR,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "/// SPIR-V operand kind: [RayQueryCandidateIntersectionType](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_ray_query_candidate_intersection_type_a_ray_query_candidate_intersection_type)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum RayQueryCandidateIntersectionType {
    RayQueryCandidateIntersectionTriangleKHR = 0u32,
    RayQueryCandidateIntersectionAABBKHR = 1u32,
}
#[allow(non_upper_case_globals)]
impl RayQueryCandidateIntersectionType {
    pub fn required_capabilities(self) -> &'static [Capability] {
        match self {
            RayQueryCandidateIntersectionType::RayQueryCandidateIntersectionTriangleKHR
            | RayQueryCandidateIntersectionType::RayQueryCandidateIntersectionAABBKHR => {
                &[Capability::RayQueryProvisionalKHR]
            }
        }
    }
}
impl num_traits::FromPrimitive for RayQueryCandidateIntersectionType {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => RayQueryCandidateIntersectionType::RayQueryCandidateIntersectionTriangleKHR,
            1u32 => RayQueryCandidateIntersectionType::RayQueryCandidateIntersectionAABBKHR,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "SPIR-V [instructions](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_instructions_a_instructions) opcodes"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum Op {
    Nop = 0u32,
    Undef = 1u32,
    SourceContinued = 2u32,
    Source = 3u32,
    SourceExtension = 4u32,
    Name = 5u32,
    MemberName = 6u32,
    String = 7u32,
    Line = 8u32,
    Extension = 10u32,
    ExtInstImport = 11u32,
    ExtInst = 12u32,
    MemoryModel = 14u32,
    EntryPoint = 15u32,
    ExecutionMode = 16u32,
    Capability = 17u32,
    TypeVoid = 19u32,
    TypeBool = 20u32,
    TypeInt = 21u32,
    TypeFloat = 22u32,
    TypeVector = 23u32,
    TypeMatrix = 24u32,
    TypeImage = 25u32,
    TypeSampler = 26u32,
    TypeSampledImage = 27u32,
    TypeArray = 28u32,
    TypeRuntimeArray = 29u32,
    TypeStruct = 30u32,
    TypeOpaque = 31u32,
    TypePointer = 32u32,
    TypeFunction = 33u32,
    TypeEvent = 34u32,
    TypeDeviceEvent = 35u32,
    TypeReserveId = 36u32,
    TypeQueue = 37u32,
    TypePipe = 38u32,
    TypeForwardPointer = 39u32,
    ConstantTrue = 41u32,
    ConstantFalse = 42u32,
    Constant = 43u32,
    ConstantComposite = 44u32,
    ConstantSampler = 45u32,
    ConstantNull = 46u32,
    SpecConstantTrue = 48u32,
    SpecConstantFalse = 49u32,
    SpecConstant = 50u32,
    SpecConstantComposite = 51u32,
    SpecConstantOp = 52u32,
    Function = 54u32,
    FunctionParameter = 55u32,
    FunctionEnd = 56u32,
    FunctionCall = 57u32,
    Variable = 59u32,
    ImageTexelPointer = 60u32,
    Load = 61u32,
    Store = 62u32,
    CopyMemory = 63u32,
    CopyMemorySized = 64u32,
    AccessChain = 65u32,
    InBoundsAccessChain = 66u32,
    PtrAccessChain = 67u32,
    ArrayLength = 68u32,
    GenericPtrMemSemantics = 69u32,
    InBoundsPtrAccessChain = 70u32,
    Decorate = 71u32,
    MemberDecorate = 72u32,
    DecorationGroup = 73u32,
    GroupDecorate = 74u32,
    GroupMemberDecorate = 75u32,
    VectorExtractDynamic = 77u32,
    VectorInsertDynamic = 78u32,
    VectorShuffle = 79u32,
    CompositeConstruct = 80u32,
    CompositeExtract = 81u32,
    CompositeInsert = 82u32,
    CopyObject = 83u32,
    Transpose = 84u32,
    SampledImage = 86u32,
    ImageSampleImplicitLod = 87u32,
    ImageSampleExplicitLod = 88u32,
    ImageSampleDrefImplicitLod = 89u32,
    ImageSampleDrefExplicitLod = 90u32,
    ImageSampleProjImplicitLod = 91u32,
    ImageSampleProjExplicitLod = 92u32,
    ImageSampleProjDrefImplicitLod = 93u32,
    ImageSampleProjDrefExplicitLod = 94u32,
    ImageFetch = 95u32,
    ImageGather = 96u32,
    ImageDrefGather = 97u32,
    ImageRead = 98u32,
    ImageWrite = 99u32,
    Image = 100u32,
    ImageQueryFormat = 101u32,
    ImageQueryOrder = 102u32,
    ImageQuerySizeLod = 103u32,
    ImageQuerySize = 104u32,
    ImageQueryLod = 105u32,
    ImageQueryLevels = 106u32,
    ImageQuerySamples = 107u32,
    ConvertFToU = 109u32,
    ConvertFToS = 110u32,
    ConvertSToF = 111u32,
    ConvertUToF = 112u32,
    UConvert = 113u32,
    SConvert = 114u32,
    FConvert = 115u32,
    QuantizeToF16 = 116u32,
    ConvertPtrToU = 117u32,
    SatConvertSToU = 118u32,
    SatConvertUToS = 119u32,
    ConvertUToPtr = 120u32,
    PtrCastToGeneric = 121u32,
    GenericCastToPtr = 122u32,
    GenericCastToPtrExplicit = 123u32,
    Bitcast = 124u32,
    SNegate = 126u32,
    FNegate = 127u32,
    IAdd = 128u32,
    FAdd = 129u32,
    ISub = 130u32,
    FSub = 131u32,
    IMul = 132u32,
    FMul = 133u32,
    UDiv = 134u32,
    SDiv = 135u32,
    FDiv = 136u32,
    UMod = 137u32,
    SRem = 138u32,
    SMod = 139u32,
    FRem = 140u32,
    FMod = 141u32,
    VectorTimesScalar = 142u32,
    MatrixTimesScalar = 143u32,
    VectorTimesMatrix = 144u32,
    MatrixTimesVector = 145u32,
    MatrixTimesMatrix = 146u32,
    OuterProduct = 147u32,
    Dot = 148u32,
    IAddCarry = 149u32,
    ISubBorrow = 150u32,
    UMulExtended = 151u32,
    SMulExtended = 152u32,
    Any = 154u32,
    All = 155u32,
    IsNan = 156u32,
    IsInf = 157u32,
    IsFinite = 158u32,
    IsNormal = 159u32,
    SignBitSet = 160u32,
    LessOrGreater = 161u32,
    Ordered = 162u32,
    Unordered = 163u32,
    LogicalEqual = 164u32,
    LogicalNotEqual = 165u32,
    LogicalOr = 166u32,
    LogicalAnd = 167u32,
    LogicalNot = 168u32,
    Select = 169u32,
    IEqual = 170u32,
    INotEqual = 171u32,
    UGreaterThan = 172u32,
    SGreaterThan = 173u32,
    UGreaterThanEqual = 174u32,
    SGreaterThanEqual = 175u32,
    ULessThan = 176u32,
    SLessThan = 177u32,
    ULessThanEqual = 178u32,
    SLessThanEqual = 179u32,
    FOrdEqual = 180u32,
    FUnordEqual = 181u32,
    FOrdNotEqual = 182u32,
    FUnordNotEqual = 183u32,
    FOrdLessThan = 184u32,
    FUnordLessThan = 185u32,
    FOrdGreaterThan = 186u32,
    FUnordGreaterThan = 187u32,
    FOrdLessThanEqual = 188u32,
    FUnordLessThanEqual = 189u32,
    FOrdGreaterThanEqual = 190u32,
    FUnordGreaterThanEqual = 191u32,
    ShiftRightLogical = 194u32,
    ShiftRightArithmetic = 195u32,
    ShiftLeftLogical = 196u32,
    BitwiseOr = 197u32,
    BitwiseXor = 198u32,
    BitwiseAnd = 199u32,
    Not = 200u32,
    BitFieldInsert = 201u32,
    BitFieldSExtract = 202u32,
    BitFieldUExtract = 203u32,
    BitReverse = 204u32,
    BitCount = 205u32,
    DPdx = 207u32,
    DPdy = 208u32,
    Fwidth = 209u32,
    DPdxFine = 210u32,
    DPdyFine = 211u32,
    FwidthFine = 212u32,
    DPdxCoarse = 213u32,
    DPdyCoarse = 214u32,
    FwidthCoarse = 215u32,
    EmitVertex = 218u32,
    EndPrimitive = 219u32,
    EmitStreamVertex = 220u32,
    EndStreamPrimitive = 221u32,
    ControlBarrier = 224u32,
    MemoryBarrier = 225u32,
    AtomicLoad = 227u32,
    AtomicStore = 228u32,
    AtomicExchange = 229u32,
    AtomicCompareExchange = 230u32,
    AtomicCompareExchangeWeak = 231u32,
    AtomicIIncrement = 232u32,
    AtomicIDecrement = 233u32,
    AtomicIAdd = 234u32,
    AtomicISub = 235u32,
    AtomicSMin = 236u32,
    AtomicUMin = 237u32,
    AtomicSMax = 238u32,
    AtomicUMax = 239u32,
    AtomicAnd = 240u32,
    AtomicOr = 241u32,
    AtomicXor = 242u32,
    Phi = 245u32,
    LoopMerge = 246u32,
    SelectionMerge = 247u32,
    Label = 248u32,
    Branch = 249u32,
    BranchConditional = 250u32,
    Switch = 251u32,
    Kill = 252u32,
    Return = 253u32,
    ReturnValue = 254u32,
    Unreachable = 255u32,
    LifetimeStart = 256u32,
    LifetimeStop = 257u32,
    GroupAsyncCopy = 259u32,
    GroupWaitEvents = 260u32,
    GroupAll = 261u32,
    GroupAny = 262u32,
    GroupBroadcast = 263u32,
    GroupIAdd = 264u32,
    GroupFAdd = 265u32,
    GroupFMin = 266u32,
    GroupUMin = 267u32,
    GroupSMin = 268u32,
    GroupFMax = 269u32,
    GroupUMax = 270u32,
    GroupSMax = 271u32,
    ReadPipe = 274u32,
    WritePipe = 275u32,
    ReservedReadPipe = 276u32,
    ReservedWritePipe = 277u32,
    ReserveReadPipePackets = 278u32,
    ReserveWritePipePackets = 279u32,
    CommitReadPipe = 280u32,
    CommitWritePipe = 281u32,
    IsValidReserveId = 282u32,
    GetNumPipePackets = 283u32,
    GetMaxPipePackets = 284u32,
    GroupReserveReadPipePackets = 285u32,
    GroupReserveWritePipePackets = 286u32,
    GroupCommitReadPipe = 287u32,
    GroupCommitWritePipe = 288u32,
    EnqueueMarker = 291u32,
    EnqueueKernel = 292u32,
    GetKernelNDrangeSubGroupCount = 293u32,
    GetKernelNDrangeMaxSubGroupSize = 294u32,
    GetKernelWorkGroupSize = 295u32,
    GetKernelPreferredWorkGroupSizeMultiple = 296u32,
    RetainEvent = 297u32,
    ReleaseEvent = 298u32,
    CreateUserEvent = 299u32,
    IsValidEvent = 300u32,
    SetUserEventStatus = 301u32,
    CaptureEventProfilingInfo = 302u32,
    GetDefaultQueue = 303u32,
    BuildNDRange = 304u32,
    ImageSparseSampleImplicitLod = 305u32,
    ImageSparseSampleExplicitLod = 306u32,
    ImageSparseSampleDrefImplicitLod = 307u32,
    ImageSparseSampleDrefExplicitLod = 308u32,
    ImageSparseSampleProjImplicitLod = 309u32,
    ImageSparseSampleProjExplicitLod = 310u32,
    ImageSparseSampleProjDrefImplicitLod = 311u32,
    ImageSparseSampleProjDrefExplicitLod = 312u32,
    ImageSparseFetch = 313u32,
    ImageSparseGather = 314u32,
    ImageSparseDrefGather = 315u32,
    ImageSparseTexelsResident = 316u32,
    NoLine = 317u32,
    AtomicFlagTestAndSet = 318u32,
    AtomicFlagClear = 319u32,
    ImageSparseRead = 320u32,
    SizeOf = 321u32,
    TypePipeStorage = 322u32,
    ConstantPipeStorage = 323u32,
    CreatePipeFromPipeStorage = 324u32,
    GetKernelLocalSizeForSubgroupCount = 325u32,
    GetKernelMaxNumSubgroups = 326u32,
    TypeNamedBarrier = 327u32,
    NamedBarrierInitialize = 328u32,
    MemoryNamedBarrier = 329u32,
    ModuleProcessed = 330u32,
    ExecutionModeId = 331u32,
    DecorateId = 332u32,
    GroupNonUniformElect = 333u32,
    GroupNonUniformAll = 334u32,
    GroupNonUniformAny = 335u32,
    GroupNonUniformAllEqual = 336u32,
    GroupNonUniformBroadcast = 337u32,
    GroupNonUniformBroadcastFirst = 338u32,
    GroupNonUniformBallot = 339u32,
    GroupNonUniformInverseBallot = 340u32,
    GroupNonUniformBallotBitExtract = 341u32,
    GroupNonUniformBallotBitCount = 342u32,
    GroupNonUniformBallotFindLSB = 343u32,
    GroupNonUniformBallotFindMSB = 344u32,
    GroupNonUniformShuffle = 345u32,
    GroupNonUniformShuffleXor = 346u32,
    GroupNonUniformShuffleUp = 347u32,
    GroupNonUniformShuffleDown = 348u32,
    GroupNonUniformIAdd = 349u32,
    GroupNonUniformFAdd = 350u32,
    GroupNonUniformIMul = 351u32,
    GroupNonUniformFMul = 352u32,
    GroupNonUniformSMin = 353u32,
    GroupNonUniformUMin = 354u32,
    GroupNonUniformFMin = 355u32,
    GroupNonUniformSMax = 356u32,
    GroupNonUniformUMax = 357u32,
    GroupNonUniformFMax = 358u32,
    GroupNonUniformBitwiseAnd = 359u32,
    GroupNonUniformBitwiseOr = 360u32,
    GroupNonUniformBitwiseXor = 361u32,
    GroupNonUniformLogicalAnd = 362u32,
    GroupNonUniformLogicalOr = 363u32,
    GroupNonUniformLogicalXor = 364u32,
    GroupNonUniformQuadBroadcast = 365u32,
    GroupNonUniformQuadSwap = 366u32,
    CopyLogical = 400u32,
    PtrEqual = 401u32,
    PtrNotEqual = 402u32,
    PtrDiff = 403u32,
    SubgroupBallotKHR = 4421u32,
    SubgroupFirstInvocationKHR = 4422u32,
    SubgroupAllKHR = 4428u32,
    SubgroupAnyKHR = 4429u32,
    SubgroupAllEqualKHR = 4430u32,
    SubgroupReadInvocationKHR = 4432u32,
    TypeRayQueryProvisionalKHR = 4472u32,
    RayQueryInitializeKHR = 4473u32,
    RayQueryTerminateKHR = 4474u32,
    RayQueryGenerateIntersectionKHR = 4475u32,
    RayQueryConfirmIntersectionKHR = 4476u32,
    RayQueryProceedKHR = 4477u32,
    RayQueryGetIntersectionTypeKHR = 4479u32,
    GroupIAddNonUniformAMD = 5000u32,
    GroupFAddNonUniformAMD = 5001u32,
    GroupFMinNonUniformAMD = 5002u32,
    GroupUMinNonUniformAMD = 5003u32,
    GroupSMinNonUniformAMD = 5004u32,
    GroupFMaxNonUniformAMD = 5005u32,
    GroupUMaxNonUniformAMD = 5006u32,
    GroupSMaxNonUniformAMD = 5007u32,
    FragmentMaskFetchAMD = 5011u32,
    FragmentFetchAMD = 5012u32,
    ReadClockKHR = 5056u32,
    ImageSampleFootprintNV = 5283u32,
    GroupNonUniformPartitionNV = 5296u32,
    WritePackedPrimitiveIndices4x8NV = 5299u32,
    ReportIntersectionNV = 5334u32,
    IgnoreIntersectionNV = 5335u32,
    TerminateRayNV = 5336u32,
    TraceNV = 5337u32,
    TypeAccelerationStructureNV = 5341u32,
    ExecuteCallableNV = 5344u32,
    TypeCooperativeMatrixNV = 5358u32,
    CooperativeMatrixLoadNV = 5359u32,
    CooperativeMatrixStoreNV = 5360u32,
    CooperativeMatrixMulAddNV = 5361u32,
    CooperativeMatrixLengthNV = 5362u32,
    BeginInvocationInterlockEXT = 5364u32,
    EndInvocationInterlockEXT = 5365u32,
    DemoteToHelperInvocationEXT = 5380u32,
    IsHelperInvocationEXT = 5381u32,
    SubgroupShuffleINTEL = 5571u32,
    SubgroupShuffleDownINTEL = 5572u32,
    SubgroupShuffleUpINTEL = 5573u32,
    SubgroupShuffleXorINTEL = 5574u32,
    SubgroupBlockReadINTEL = 5575u32,
    SubgroupBlockWriteINTEL = 5576u32,
    SubgroupImageBlockReadINTEL = 5577u32,
    SubgroupImageBlockWriteINTEL = 5578u32,
    SubgroupImageMediaBlockReadINTEL = 5580u32,
    SubgroupImageMediaBlockWriteINTEL = 5581u32,
    UCountLeadingZerosINTEL = 5585u32,
    UCountTrailingZerosINTEL = 5586u32,
    AbsISubINTEL = 5587u32,
    AbsUSubINTEL = 5588u32,
    IAddSatINTEL = 5589u32,
    UAddSatINTEL = 5590u32,
    IAverageINTEL = 5591u32,
    UAverageINTEL = 5592u32,
    IAverageRoundedINTEL = 5593u32,
    UAverageRoundedINTEL = 5594u32,
    ISubSatINTEL = 5595u32,
    USubSatINTEL = 5596u32,
    IMul32x16INTEL = 5597u32,
    UMul32x16INTEL = 5598u32,
    DecorateString = 5632u32,
    MemberDecorateString = 5633u32,
    VmeImageINTEL = 5699u32,
    TypeVmeImageINTEL = 5700u32,
    TypeAvcImePayloadINTEL = 5701u32,
    TypeAvcRefPayloadINTEL = 5702u32,
    TypeAvcSicPayloadINTEL = 5703u32,
    TypeAvcMcePayloadINTEL = 5704u32,
    TypeAvcMceResultINTEL = 5705u32,
    TypeAvcImeResultINTEL = 5706u32,
    TypeAvcImeResultSingleReferenceStreamoutINTEL = 5707u32,
    TypeAvcImeResultDualReferenceStreamoutINTEL = 5708u32,
    TypeAvcImeSingleReferenceStreaminINTEL = 5709u32,
    TypeAvcImeDualReferenceStreaminINTEL = 5710u32,
    TypeAvcRefResultINTEL = 5711u32,
    TypeAvcSicResultINTEL = 5712u32,
    SubgroupAvcMceGetDefaultInterBaseMultiReferencePenaltyINTEL = 5713u32,
    SubgroupAvcMceSetInterBaseMultiReferencePenaltyINTEL = 5714u32,
    SubgroupAvcMceGetDefaultInterShapePenaltyINTEL = 5715u32,
    SubgroupAvcMceSetInterShapePenaltyINTEL = 5716u32,
    SubgroupAvcMceGetDefaultInterDirectionPenaltyINTEL = 5717u32,
    SubgroupAvcMceSetInterDirectionPenaltyINTEL = 5718u32,
    SubgroupAvcMceGetDefaultIntraLumaShapePenaltyINTEL = 5719u32,
    SubgroupAvcMceGetDefaultInterMotionVectorCostTableINTEL = 5720u32,
    SubgroupAvcMceGetDefaultHighPenaltyCostTableINTEL = 5721u32,
    SubgroupAvcMceGetDefaultMediumPenaltyCostTableINTEL = 5722u32,
    SubgroupAvcMceGetDefaultLowPenaltyCostTableINTEL = 5723u32,
    SubgroupAvcMceSetMotionVectorCostFunctionINTEL = 5724u32,
    SubgroupAvcMceGetDefaultIntraLumaModePenaltyINTEL = 5725u32,
    SubgroupAvcMceGetDefaultNonDcLumaIntraPenaltyINTEL = 5726u32,
    SubgroupAvcMceGetDefaultIntraChromaModeBasePenaltyINTEL = 5727u32,
    SubgroupAvcMceSetAcOnlyHaarINTEL = 5728u32,
    SubgroupAvcMceSetSourceInterlacedFieldPolarityINTEL = 5729u32,
    SubgroupAvcMceSetSingleReferenceInterlacedFieldPolarityINTEL = 5730u32,
    SubgroupAvcMceSetDualReferenceInterlacedFieldPolaritiesINTEL = 5731u32,
    SubgroupAvcMceConvertToImePayloadINTEL = 5732u32,
    SubgroupAvcMceConvertToImeResultINTEL = 5733u32,
    SubgroupAvcMceConvertToRefPayloadINTEL = 5734u32,
    SubgroupAvcMceConvertToRefResultINTEL = 5735u32,
    SubgroupAvcMceConvertToSicPayloadINTEL = 5736u32,
    SubgroupAvcMceConvertToSicResultINTEL = 5737u32,
    SubgroupAvcMceGetMotionVectorsINTEL = 5738u32,
    SubgroupAvcMceGetInterDistortionsINTEL = 5739u32,
    SubgroupAvcMceGetBestInterDistortionsINTEL = 5740u32,
    SubgroupAvcMceGetInterMajorShapeINTEL = 5741u32,
    SubgroupAvcMceGetInterMinorShapeINTEL = 5742u32,
    SubgroupAvcMceGetInterDirectionsINTEL = 5743u32,
    SubgroupAvcMceGetInterMotionVectorCountINTEL = 5744u32,
    SubgroupAvcMceGetInterReferenceIdsINTEL = 5745u32,
    SubgroupAvcMceGetInterReferenceInterlacedFieldPolaritiesINTEL = 5746u32,
    SubgroupAvcImeInitializeINTEL = 5747u32,
    SubgroupAvcImeSetSingleReferenceINTEL = 5748u32,
    SubgroupAvcImeSetDualReferenceINTEL = 5749u32,
    SubgroupAvcImeRefWindowSizeINTEL = 5750u32,
    SubgroupAvcImeAdjustRefOffsetINTEL = 5751u32,
    SubgroupAvcImeConvertToMcePayloadINTEL = 5752u32,
    SubgroupAvcImeSetMaxMotionVectorCountINTEL = 5753u32,
    SubgroupAvcImeSetUnidirectionalMixDisableINTEL = 5754u32,
    SubgroupAvcImeSetEarlySearchTerminationThresholdINTEL = 5755u32,
    SubgroupAvcImeSetWeightedSadINTEL = 5756u32,
    SubgroupAvcImeEvaluateWithSingleReferenceINTEL = 5757u32,
    SubgroupAvcImeEvaluateWithDualReferenceINTEL = 5758u32,
    SubgroupAvcImeEvaluateWithSingleReferenceStreaminINTEL = 5759u32,
    SubgroupAvcImeEvaluateWithDualReferenceStreaminINTEL = 5760u32,
    SubgroupAvcImeEvaluateWithSingleReferenceStreamoutINTEL = 5761u32,
    SubgroupAvcImeEvaluateWithDualReferenceStreamoutINTEL = 5762u32,
    SubgroupAvcImeEvaluateWithSingleReferenceStreaminoutINTEL = 5763u32,
    SubgroupAvcImeEvaluateWithDualReferenceStreaminoutINTEL = 5764u32,
    SubgroupAvcImeConvertToMceResultINTEL = 5765u32,
    SubgroupAvcImeGetSingleReferenceStreaminINTEL = 5766u32,
    SubgroupAvcImeGetDualReferenceStreaminINTEL = 5767u32,
    SubgroupAvcImeStripSingleReferenceStreamoutINTEL = 5768u32,
    SubgroupAvcImeStripDualReferenceStreamoutINTEL = 5769u32,
    SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeMotionVectorsINTEL = 5770u32,
    SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeDistortionsINTEL = 5771u32,
    SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeReferenceIdsINTEL = 5772u32,
    SubgroupAvcImeGetStreamoutDualReferenceMajorShapeMotionVectorsINTEL = 5773u32,
    SubgroupAvcImeGetStreamoutDualReferenceMajorShapeDistortionsINTEL = 5774u32,
    SubgroupAvcImeGetStreamoutDualReferenceMajorShapeReferenceIdsINTEL = 5775u32,
    SubgroupAvcImeGetBorderReachedINTEL = 5776u32,
    SubgroupAvcImeGetTruncatedSearchIndicationINTEL = 5777u32,
    SubgroupAvcImeGetUnidirectionalEarlySearchTerminationINTEL = 5778u32,
    SubgroupAvcImeGetWeightingPatternMinimumMotionVectorINTEL = 5779u32,
    SubgroupAvcImeGetWeightingPatternMinimumDistortionINTEL = 5780u32,
    SubgroupAvcFmeInitializeINTEL = 5781u32,
    SubgroupAvcBmeInitializeINTEL = 5782u32,
    SubgroupAvcRefConvertToMcePayloadINTEL = 5783u32,
    SubgroupAvcRefSetBidirectionalMixDisableINTEL = 5784u32,
    SubgroupAvcRefSetBilinearFilterEnableINTEL = 5785u32,
    SubgroupAvcRefEvaluateWithSingleReferenceINTEL = 5786u32,
    SubgroupAvcRefEvaluateWithDualReferenceINTEL = 5787u32,
    SubgroupAvcRefEvaluateWithMultiReferenceINTEL = 5788u32,
    SubgroupAvcRefEvaluateWithMultiReferenceInterlacedINTEL = 5789u32,
    SubgroupAvcRefConvertToMceResultINTEL = 5790u32,
    SubgroupAvcSicInitializeINTEL = 5791u32,
    SubgroupAvcSicConfigureSkcINTEL = 5792u32,
    SubgroupAvcSicConfigureIpeLumaINTEL = 5793u32,
    SubgroupAvcSicConfigureIpeLumaChromaINTEL = 5794u32,
    SubgroupAvcSicGetMotionVectorMaskINTEL = 5795u32,
    SubgroupAvcSicConvertToMcePayloadINTEL = 5796u32,
    SubgroupAvcSicSetIntraLumaShapePenaltyINTEL = 5797u32,
    SubgroupAvcSicSetIntraLumaModeCostFunctionINTEL = 5798u32,
    SubgroupAvcSicSetIntraChromaModeCostFunctionINTEL = 5799u32,
    SubgroupAvcSicSetBilinearFilterEnableINTEL = 5800u32,
    SubgroupAvcSicSetSkcForwardTransformEnableINTEL = 5801u32,
    SubgroupAvcSicSetBlockBasedRawSkipSadINTEL = 5802u32,
    SubgroupAvcSicEvaluateIpeINTEL = 5803u32,
    SubgroupAvcSicEvaluateWithSingleReferenceINTEL = 5804u32,
    SubgroupAvcSicEvaluateWithDualReferenceINTEL = 5805u32,
    SubgroupAvcSicEvaluateWithMultiReferenceINTEL = 5806u32,
    SubgroupAvcSicEvaluateWithMultiReferenceInterlacedINTEL = 5807u32,
    SubgroupAvcSicConvertToMceResultINTEL = 5808u32,
    SubgroupAvcSicGetIpeLumaShapeINTEL = 5809u32,
    SubgroupAvcSicGetBestIpeLumaDistortionINTEL = 5810u32,
    SubgroupAvcSicGetBestIpeChromaDistortionINTEL = 5811u32,
    SubgroupAvcSicGetPackedIpeLumaModesINTEL = 5812u32,
    SubgroupAvcSicGetIpeChromaModeINTEL = 5813u32,
    SubgroupAvcSicGetPackedSkcLumaCountThresholdINTEL = 5814u32,
    SubgroupAvcSicGetPackedSkcLumaSumThresholdINTEL = 5815u32,
    SubgroupAvcSicGetInterRawSadsINTEL = 5816u32,
    RayQueryGetRayTMinKHR = 6016u32,
    RayQueryGetRayFlagsKHR = 6017u32,
    RayQueryGetIntersectionTKHR = 6018u32,
    RayQueryGetIntersectionInstanceCustomIndexKHR = 6019u32,
    RayQueryGetIntersectionInstanceIdKHR = 6020u32,
    RayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetKHR = 6021u32,
    RayQueryGetIntersectionGeometryIndexKHR = 6022u32,
    RayQueryGetIntersectionPrimitiveIndexKHR = 6023u32,
    RayQueryGetIntersectionBarycentricsKHR = 6024u32,
    RayQueryGetIntersectionFrontFaceKHR = 6025u32,
    RayQueryGetIntersectionCandidateAABBOpaqueKHR = 6026u32,
    RayQueryGetIntersectionObjectRayDirectionKHR = 6027u32,
    RayQueryGetIntersectionObjectRayOriginKHR = 6028u32,
    RayQueryGetWorldRayDirectionKHR = 6029u32,
    RayQueryGetWorldRayOriginKHR = 6030u32,
    RayQueryGetIntersectionObjectToWorldKHR = 6031u32,
    RayQueryGetIntersectionWorldToObjectKHR = 6032u32,
}
#[allow(non_upper_case_globals)]
impl Op {
    pub const ReportIntersectionKHR: Op = Op::ReportIntersectionNV;
    pub const IgnoreIntersectionKHR: Op = Op::IgnoreIntersectionNV;
    pub const TerminateRayKHR: Op = Op::TerminateRayNV;
    pub const TraceRayKHR: Op = Op::TraceNV;
    pub const TypeAccelerationStructureKHR: Op = Op::TypeAccelerationStructureNV;
    pub const ExecuteCallableKHR: Op = Op::ExecuteCallableNV;
    pub const DecorateStringGOOGLE: Op = Op::DecorateString;
    pub const MemberDecorateStringGOOGLE: Op = Op::MemberDecorateString;
}
impl num_traits::FromPrimitive for Op {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => Op::Nop,
            1u32 => Op::Undef,
            2u32 => Op::SourceContinued,
            3u32 => Op::Source,
            4u32 => Op::SourceExtension,
            5u32 => Op::Name,
            6u32 => Op::MemberName,
            7u32 => Op::String,
            8u32 => Op::Line,
            10u32 => Op::Extension,
            11u32 => Op::ExtInstImport,
            12u32 => Op::ExtInst,
            14u32 => Op::MemoryModel,
            15u32 => Op::EntryPoint,
            16u32 => Op::ExecutionMode,
            17u32 => Op::Capability,
            19u32 => Op::TypeVoid,
            20u32 => Op::TypeBool,
            21u32 => Op::TypeInt,
            22u32 => Op::TypeFloat,
            23u32 => Op::TypeVector,
            24u32 => Op::TypeMatrix,
            25u32 => Op::TypeImage,
            26u32 => Op::TypeSampler,
            27u32 => Op::TypeSampledImage,
            28u32 => Op::TypeArray,
            29u32 => Op::TypeRuntimeArray,
            30u32 => Op::TypeStruct,
            31u32 => Op::TypeOpaque,
            32u32 => Op::TypePointer,
            33u32 => Op::TypeFunction,
            34u32 => Op::TypeEvent,
            35u32 => Op::TypeDeviceEvent,
            36u32 => Op::TypeReserveId,
            37u32 => Op::TypeQueue,
            38u32 => Op::TypePipe,
            39u32 => Op::TypeForwardPointer,
            41u32 => Op::ConstantTrue,
            42u32 => Op::ConstantFalse,
            43u32 => Op::Constant,
            44u32 => Op::ConstantComposite,
            45u32 => Op::ConstantSampler,
            46u32 => Op::ConstantNull,
            48u32 => Op::SpecConstantTrue,
            49u32 => Op::SpecConstantFalse,
            50u32 => Op::SpecConstant,
            51u32 => Op::SpecConstantComposite,
            52u32 => Op::SpecConstantOp,
            54u32 => Op::Function,
            55u32 => Op::FunctionParameter,
            56u32 => Op::FunctionEnd,
            57u32 => Op::FunctionCall,
            59u32 => Op::Variable,
            60u32 => Op::ImageTexelPointer,
            61u32 => Op::Load,
            62u32 => Op::Store,
            63u32 => Op::CopyMemory,
            64u32 => Op::CopyMemorySized,
            65u32 => Op::AccessChain,
            66u32 => Op::InBoundsAccessChain,
            67u32 => Op::PtrAccessChain,
            68u32 => Op::ArrayLength,
            69u32 => Op::GenericPtrMemSemantics,
            70u32 => Op::InBoundsPtrAccessChain,
            71u32 => Op::Decorate,
            72u32 => Op::MemberDecorate,
            73u32 => Op::DecorationGroup,
            74u32 => Op::GroupDecorate,
            75u32 => Op::GroupMemberDecorate,
            77u32 => Op::VectorExtractDynamic,
            78u32 => Op::VectorInsertDynamic,
            79u32 => Op::VectorShuffle,
            80u32 => Op::CompositeConstruct,
            81u32 => Op::CompositeExtract,
            82u32 => Op::CompositeInsert,
            83u32 => Op::CopyObject,
            84u32 => Op::Transpose,
            86u32 => Op::SampledImage,
            87u32 => Op::ImageSampleImplicitLod,
            88u32 => Op::ImageSampleExplicitLod,
            89u32 => Op::ImageSampleDrefImplicitLod,
            90u32 => Op::ImageSampleDrefExplicitLod,
            91u32 => Op::ImageSampleProjImplicitLod,
            92u32 => Op::ImageSampleProjExplicitLod,
            93u32 => Op::ImageSampleProjDrefImplicitLod,
            94u32 => Op::ImageSampleProjDrefExplicitLod,
            95u32 => Op::ImageFetch,
            96u32 => Op::ImageGather,
            97u32 => Op::ImageDrefGather,
            98u32 => Op::ImageRead,
            99u32 => Op::ImageWrite,
            100u32 => Op::Image,
            101u32 => Op::ImageQueryFormat,
            102u32 => Op::ImageQueryOrder,
            103u32 => Op::ImageQuerySizeLod,
            104u32 => Op::ImageQuerySize,
            105u32 => Op::ImageQueryLod,
            106u32 => Op::ImageQueryLevels,
            107u32 => Op::ImageQuerySamples,
            109u32 => Op::ConvertFToU,
            110u32 => Op::ConvertFToS,
            111u32 => Op::ConvertSToF,
            112u32 => Op::ConvertUToF,
            113u32 => Op::UConvert,
            114u32 => Op::SConvert,
            115u32 => Op::FConvert,
            116u32 => Op::QuantizeToF16,
            117u32 => Op::ConvertPtrToU,
            118u32 => Op::SatConvertSToU,
            119u32 => Op::SatConvertUToS,
            120u32 => Op::ConvertUToPtr,
            121u32 => Op::PtrCastToGeneric,
            122u32 => Op::GenericCastToPtr,
            123u32 => Op::GenericCastToPtrExplicit,
            124u32 => Op::Bitcast,
            126u32 => Op::SNegate,
            127u32 => Op::FNegate,
            128u32 => Op::IAdd,
            129u32 => Op::FAdd,
            130u32 => Op::ISub,
            131u32 => Op::FSub,
            132u32 => Op::IMul,
            133u32 => Op::FMul,
            134u32 => Op::UDiv,
            135u32 => Op::SDiv,
            136u32 => Op::FDiv,
            137u32 => Op::UMod,
            138u32 => Op::SRem,
            139u32 => Op::SMod,
            140u32 => Op::FRem,
            141u32 => Op::FMod,
            142u32 => Op::VectorTimesScalar,
            143u32 => Op::MatrixTimesScalar,
            144u32 => Op::VectorTimesMatrix,
            145u32 => Op::MatrixTimesVector,
            146u32 => Op::MatrixTimesMatrix,
            147u32 => Op::OuterProduct,
            148u32 => Op::Dot,
            149u32 => Op::IAddCarry,
            150u32 => Op::ISubBorrow,
            151u32 => Op::UMulExtended,
            152u32 => Op::SMulExtended,
            154u32 => Op::Any,
            155u32 => Op::All,
            156u32 => Op::IsNan,
            157u32 => Op::IsInf,
            158u32 => Op::IsFinite,
            159u32 => Op::IsNormal,
            160u32 => Op::SignBitSet,
            161u32 => Op::LessOrGreater,
            162u32 => Op::Ordered,
            163u32 => Op::Unordered,
            164u32 => Op::LogicalEqual,
            165u32 => Op::LogicalNotEqual,
            166u32 => Op::LogicalOr,
            167u32 => Op::LogicalAnd,
            168u32 => Op::LogicalNot,
            169u32 => Op::Select,
            170u32 => Op::IEqual,
            171u32 => Op::INotEqual,
            172u32 => Op::UGreaterThan,
            173u32 => Op::SGreaterThan,
            174u32 => Op::UGreaterThanEqual,
            175u32 => Op::SGreaterThanEqual,
            176u32 => Op::ULessThan,
            177u32 => Op::SLessThan,
            178u32 => Op::ULessThanEqual,
            179u32 => Op::SLessThanEqual,
            180u32 => Op::FOrdEqual,
            181u32 => Op::FUnordEqual,
            182u32 => Op::FOrdNotEqual,
            183u32 => Op::FUnordNotEqual,
            184u32 => Op::FOrdLessThan,
            185u32 => Op::FUnordLessThan,
            186u32 => Op::FOrdGreaterThan,
            187u32 => Op::FUnordGreaterThan,
            188u32 => Op::FOrdLessThanEqual,
            189u32 => Op::FUnordLessThanEqual,
            190u32 => Op::FOrdGreaterThanEqual,
            191u32 => Op::FUnordGreaterThanEqual,
            194u32 => Op::ShiftRightLogical,
            195u32 => Op::ShiftRightArithmetic,
            196u32 => Op::ShiftLeftLogical,
            197u32 => Op::BitwiseOr,
            198u32 => Op::BitwiseXor,
            199u32 => Op::BitwiseAnd,
            200u32 => Op::Not,
            201u32 => Op::BitFieldInsert,
            202u32 => Op::BitFieldSExtract,
            203u32 => Op::BitFieldUExtract,
            204u32 => Op::BitReverse,
            205u32 => Op::BitCount,
            207u32 => Op::DPdx,
            208u32 => Op::DPdy,
            209u32 => Op::Fwidth,
            210u32 => Op::DPdxFine,
            211u32 => Op::DPdyFine,
            212u32 => Op::FwidthFine,
            213u32 => Op::DPdxCoarse,
            214u32 => Op::DPdyCoarse,
            215u32 => Op::FwidthCoarse,
            218u32 => Op::EmitVertex,
            219u32 => Op::EndPrimitive,
            220u32 => Op::EmitStreamVertex,
            221u32 => Op::EndStreamPrimitive,
            224u32 => Op::ControlBarrier,
            225u32 => Op::MemoryBarrier,
            227u32 => Op::AtomicLoad,
            228u32 => Op::AtomicStore,
            229u32 => Op::AtomicExchange,
            230u32 => Op::AtomicCompareExchange,
            231u32 => Op::AtomicCompareExchangeWeak,
            232u32 => Op::AtomicIIncrement,
            233u32 => Op::AtomicIDecrement,
            234u32 => Op::AtomicIAdd,
            235u32 => Op::AtomicISub,
            236u32 => Op::AtomicSMin,
            237u32 => Op::AtomicUMin,
            238u32 => Op::AtomicSMax,
            239u32 => Op::AtomicUMax,
            240u32 => Op::AtomicAnd,
            241u32 => Op::AtomicOr,
            242u32 => Op::AtomicXor,
            245u32 => Op::Phi,
            246u32 => Op::LoopMerge,
            247u32 => Op::SelectionMerge,
            248u32 => Op::Label,
            249u32 => Op::Branch,
            250u32 => Op::BranchConditional,
            251u32 => Op::Switch,
            252u32 => Op::Kill,
            253u32 => Op::Return,
            254u32 => Op::ReturnValue,
            255u32 => Op::Unreachable,
            256u32 => Op::LifetimeStart,
            257u32 => Op::LifetimeStop,
            259u32 => Op::GroupAsyncCopy,
            260u32 => Op::GroupWaitEvents,
            261u32 => Op::GroupAll,
            262u32 => Op::GroupAny,
            263u32 => Op::GroupBroadcast,
            264u32 => Op::GroupIAdd,
            265u32 => Op::GroupFAdd,
            266u32 => Op::GroupFMin,
            267u32 => Op::GroupUMin,
            268u32 => Op::GroupSMin,
            269u32 => Op::GroupFMax,
            270u32 => Op::GroupUMax,
            271u32 => Op::GroupSMax,
            274u32 => Op::ReadPipe,
            275u32 => Op::WritePipe,
            276u32 => Op::ReservedReadPipe,
            277u32 => Op::ReservedWritePipe,
            278u32 => Op::ReserveReadPipePackets,
            279u32 => Op::ReserveWritePipePackets,
            280u32 => Op::CommitReadPipe,
            281u32 => Op::CommitWritePipe,
            282u32 => Op::IsValidReserveId,
            283u32 => Op::GetNumPipePackets,
            284u32 => Op::GetMaxPipePackets,
            285u32 => Op::GroupReserveReadPipePackets,
            286u32 => Op::GroupReserveWritePipePackets,
            287u32 => Op::GroupCommitReadPipe,
            288u32 => Op::GroupCommitWritePipe,
            291u32 => Op::EnqueueMarker,
            292u32 => Op::EnqueueKernel,
            293u32 => Op::GetKernelNDrangeSubGroupCount,
            294u32 => Op::GetKernelNDrangeMaxSubGroupSize,
            295u32 => Op::GetKernelWorkGroupSize,
            296u32 => Op::GetKernelPreferredWorkGroupSizeMultiple,
            297u32 => Op::RetainEvent,
            298u32 => Op::ReleaseEvent,
            299u32 => Op::CreateUserEvent,
            300u32 => Op::IsValidEvent,
            301u32 => Op::SetUserEventStatus,
            302u32 => Op::CaptureEventProfilingInfo,
            303u32 => Op::GetDefaultQueue,
            304u32 => Op::BuildNDRange,
            305u32 => Op::ImageSparseSampleImplicitLod,
            306u32 => Op::ImageSparseSampleExplicitLod,
            307u32 => Op::ImageSparseSampleDrefImplicitLod,
            308u32 => Op::ImageSparseSampleDrefExplicitLod,
            309u32 => Op::ImageSparseSampleProjImplicitLod,
            310u32 => Op::ImageSparseSampleProjExplicitLod,
            311u32 => Op::ImageSparseSampleProjDrefImplicitLod,
            312u32 => Op::ImageSparseSampleProjDrefExplicitLod,
            313u32 => Op::ImageSparseFetch,
            314u32 => Op::ImageSparseGather,
            315u32 => Op::ImageSparseDrefGather,
            316u32 => Op::ImageSparseTexelsResident,
            317u32 => Op::NoLine,
            318u32 => Op::AtomicFlagTestAndSet,
            319u32 => Op::AtomicFlagClear,
            320u32 => Op::ImageSparseRead,
            321u32 => Op::SizeOf,
            322u32 => Op::TypePipeStorage,
            323u32 => Op::ConstantPipeStorage,
            324u32 => Op::CreatePipeFromPipeStorage,
            325u32 => Op::GetKernelLocalSizeForSubgroupCount,
            326u32 => Op::GetKernelMaxNumSubgroups,
            327u32 => Op::TypeNamedBarrier,
            328u32 => Op::NamedBarrierInitialize,
            329u32 => Op::MemoryNamedBarrier,
            330u32 => Op::ModuleProcessed,
            331u32 => Op::ExecutionModeId,
            332u32 => Op::DecorateId,
            333u32 => Op::GroupNonUniformElect,
            334u32 => Op::GroupNonUniformAll,
            335u32 => Op::GroupNonUniformAny,
            336u32 => Op::GroupNonUniformAllEqual,
            337u32 => Op::GroupNonUniformBroadcast,
            338u32 => Op::GroupNonUniformBroadcastFirst,
            339u32 => Op::GroupNonUniformBallot,
            340u32 => Op::GroupNonUniformInverseBallot,
            341u32 => Op::GroupNonUniformBallotBitExtract,
            342u32 => Op::GroupNonUniformBallotBitCount,
            343u32 => Op::GroupNonUniformBallotFindLSB,
            344u32 => Op::GroupNonUniformBallotFindMSB,
            345u32 => Op::GroupNonUniformShuffle,
            346u32 => Op::GroupNonUniformShuffleXor,
            347u32 => Op::GroupNonUniformShuffleUp,
            348u32 => Op::GroupNonUniformShuffleDown,
            349u32 => Op::GroupNonUniformIAdd,
            350u32 => Op::GroupNonUniformFAdd,
            351u32 => Op::GroupNonUniformIMul,
            352u32 => Op::GroupNonUniformFMul,
            353u32 => Op::GroupNonUniformSMin,
            354u32 => Op::GroupNonUniformUMin,
            355u32 => Op::GroupNonUniformFMin,
            356u32 => Op::GroupNonUniformSMax,
            357u32 => Op::GroupNonUniformUMax,
            358u32 => Op::GroupNonUniformFMax,
            359u32 => Op::GroupNonUniformBitwiseAnd,
            360u32 => Op::GroupNonUniformBitwiseOr,
            361u32 => Op::GroupNonUniformBitwiseXor,
            362u32 => Op::GroupNonUniformLogicalAnd,
            363u32 => Op::GroupNonUniformLogicalOr,
            364u32 => Op::GroupNonUniformLogicalXor,
            365u32 => Op::GroupNonUniformQuadBroadcast,
            366u32 => Op::GroupNonUniformQuadSwap,
            400u32 => Op::CopyLogical,
            401u32 => Op::PtrEqual,
            402u32 => Op::PtrNotEqual,
            403u32 => Op::PtrDiff,
            4421u32 => Op::SubgroupBallotKHR,
            4422u32 => Op::SubgroupFirstInvocationKHR,
            4428u32 => Op::SubgroupAllKHR,
            4429u32 => Op::SubgroupAnyKHR,
            4430u32 => Op::SubgroupAllEqualKHR,
            4432u32 => Op::SubgroupReadInvocationKHR,
            4472u32 => Op::TypeRayQueryProvisionalKHR,
            4473u32 => Op::RayQueryInitializeKHR,
            4474u32 => Op::RayQueryTerminateKHR,
            4475u32 => Op::RayQueryGenerateIntersectionKHR,
            4476u32 => Op::RayQueryConfirmIntersectionKHR,
            4477u32 => Op::RayQueryProceedKHR,
            4479u32 => Op::RayQueryGetIntersectionTypeKHR,
            5000u32 => Op::GroupIAddNonUniformAMD,
            5001u32 => Op::GroupFAddNonUniformAMD,
            5002u32 => Op::GroupFMinNonUniformAMD,
            5003u32 => Op::GroupUMinNonUniformAMD,
            5004u32 => Op::GroupSMinNonUniformAMD,
            5005u32 => Op::GroupFMaxNonUniformAMD,
            5006u32 => Op::GroupUMaxNonUniformAMD,
            5007u32 => Op::GroupSMaxNonUniformAMD,
            5011u32 => Op::FragmentMaskFetchAMD,
            5012u32 => Op::FragmentFetchAMD,
            5056u32 => Op::ReadClockKHR,
            5283u32 => Op::ImageSampleFootprintNV,
            5296u32 => Op::GroupNonUniformPartitionNV,
            5299u32 => Op::WritePackedPrimitiveIndices4x8NV,
            5334u32 => Op::ReportIntersectionNV,
            5335u32 => Op::IgnoreIntersectionNV,
            5336u32 => Op::TerminateRayNV,
            5337u32 => Op::TraceNV,
            5341u32 => Op::TypeAccelerationStructureNV,
            5344u32 => Op::ExecuteCallableNV,
            5358u32 => Op::TypeCooperativeMatrixNV,
            5359u32 => Op::CooperativeMatrixLoadNV,
            5360u32 => Op::CooperativeMatrixStoreNV,
            5361u32 => Op::CooperativeMatrixMulAddNV,
            5362u32 => Op::CooperativeMatrixLengthNV,
            5364u32 => Op::BeginInvocationInterlockEXT,
            5365u32 => Op::EndInvocationInterlockEXT,
            5380u32 => Op::DemoteToHelperInvocationEXT,
            5381u32 => Op::IsHelperInvocationEXT,
            5571u32 => Op::SubgroupShuffleINTEL,
            5572u32 => Op::SubgroupShuffleDownINTEL,
            5573u32 => Op::SubgroupShuffleUpINTEL,
            5574u32 => Op::SubgroupShuffleXorINTEL,
            5575u32 => Op::SubgroupBlockReadINTEL,
            5576u32 => Op::SubgroupBlockWriteINTEL,
            5577u32 => Op::SubgroupImageBlockReadINTEL,
            5578u32 => Op::SubgroupImageBlockWriteINTEL,
            5580u32 => Op::SubgroupImageMediaBlockReadINTEL,
            5581u32 => Op::SubgroupImageMediaBlockWriteINTEL,
            5585u32 => Op::UCountLeadingZerosINTEL,
            5586u32 => Op::UCountTrailingZerosINTEL,
            5587u32 => Op::AbsISubINTEL,
            5588u32 => Op::AbsUSubINTEL,
            5589u32 => Op::IAddSatINTEL,
            5590u32 => Op::UAddSatINTEL,
            5591u32 => Op::IAverageINTEL,
            5592u32 => Op::UAverageINTEL,
            5593u32 => Op::IAverageRoundedINTEL,
            5594u32 => Op::UAverageRoundedINTEL,
            5595u32 => Op::ISubSatINTEL,
            5596u32 => Op::USubSatINTEL,
            5597u32 => Op::IMul32x16INTEL,
            5598u32 => Op::UMul32x16INTEL,
            5632u32 => Op::DecorateString,
            5633u32 => Op::MemberDecorateString,
            5699u32 => Op::VmeImageINTEL,
            5700u32 => Op::TypeVmeImageINTEL,
            5701u32 => Op::TypeAvcImePayloadINTEL,
            5702u32 => Op::TypeAvcRefPayloadINTEL,
            5703u32 => Op::TypeAvcSicPayloadINTEL,
            5704u32 => Op::TypeAvcMcePayloadINTEL,
            5705u32 => Op::TypeAvcMceResultINTEL,
            5706u32 => Op::TypeAvcImeResultINTEL,
            5707u32 => Op::TypeAvcImeResultSingleReferenceStreamoutINTEL,
            5708u32 => Op::TypeAvcImeResultDualReferenceStreamoutINTEL,
            5709u32 => Op::TypeAvcImeSingleReferenceStreaminINTEL,
            5710u32 => Op::TypeAvcImeDualReferenceStreaminINTEL,
            5711u32 => Op::TypeAvcRefResultINTEL,
            5712u32 => Op::TypeAvcSicResultINTEL,
            5713u32 => Op::SubgroupAvcMceGetDefaultInterBaseMultiReferencePenaltyINTEL,
            5714u32 => Op::SubgroupAvcMceSetInterBaseMultiReferencePenaltyINTEL,
            5715u32 => Op::SubgroupAvcMceGetDefaultInterShapePenaltyINTEL,
            5716u32 => Op::SubgroupAvcMceSetInterShapePenaltyINTEL,
            5717u32 => Op::SubgroupAvcMceGetDefaultInterDirectionPenaltyINTEL,
            5718u32 => Op::SubgroupAvcMceSetInterDirectionPenaltyINTEL,
            5719u32 => Op::SubgroupAvcMceGetDefaultIntraLumaShapePenaltyINTEL,
            5720u32 => Op::SubgroupAvcMceGetDefaultInterMotionVectorCostTableINTEL,
            5721u32 => Op::SubgroupAvcMceGetDefaultHighPenaltyCostTableINTEL,
            5722u32 => Op::SubgroupAvcMceGetDefaultMediumPenaltyCostTableINTEL,
            5723u32 => Op::SubgroupAvcMceGetDefaultLowPenaltyCostTableINTEL,
            5724u32 => Op::SubgroupAvcMceSetMotionVectorCostFunctionINTEL,
            5725u32 => Op::SubgroupAvcMceGetDefaultIntraLumaModePenaltyINTEL,
            5726u32 => Op::SubgroupAvcMceGetDefaultNonDcLumaIntraPenaltyINTEL,
            5727u32 => Op::SubgroupAvcMceGetDefaultIntraChromaModeBasePenaltyINTEL,
            5728u32 => Op::SubgroupAvcMceSetAcOnlyHaarINTEL,
            5729u32 => Op::SubgroupAvcMceSetSourceInterlacedFieldPolarityINTEL,
            5730u32 => Op::SubgroupAvcMceSetSingleReferenceInterlacedFieldPolarityINTEL,
            5731u32 => Op::SubgroupAvcMceSetDualReferenceInterlacedFieldPolaritiesINTEL,
            5732u32 => Op::SubgroupAvcMceConvertToImePayloadINTEL,
            5733u32 => Op::SubgroupAvcMceConvertToImeResultINTEL,
            5734u32 => Op::SubgroupAvcMceConvertToRefPayloadINTEL,
            5735u32 => Op::SubgroupAvcMceConvertToRefResultINTEL,
            5736u32 => Op::SubgroupAvcMceConvertToSicPayloadINTEL,
            5737u32 => Op::SubgroupAvcMceConvertToSicResultINTEL,
            5738u32 => Op::SubgroupAvcMceGetMotionVectorsINTEL,
            5739u32 => Op::SubgroupAvcMceGetInterDistortionsINTEL,
            5740u32 => Op::SubgroupAvcMceGetBestInterDistortionsINTEL,
            5741u32 => Op::SubgroupAvcMceGetInterMajorShapeINTEL,
            5742u32 => Op::SubgroupAvcMceGetInterMinorShapeINTEL,
            5743u32 => Op::SubgroupAvcMceGetInterDirectionsINTEL,
            5744u32 => Op::SubgroupAvcMceGetInterMotionVectorCountINTEL,
            5745u32 => Op::SubgroupAvcMceGetInterReferenceIdsINTEL,
            5746u32 => Op::SubgroupAvcMceGetInterReferenceInterlacedFieldPolaritiesINTEL,
            5747u32 => Op::SubgroupAvcImeInitializeINTEL,
            5748u32 => Op::SubgroupAvcImeSetSingleReferenceINTEL,
            5749u32 => Op::SubgroupAvcImeSetDualReferenceINTEL,
            5750u32 => Op::SubgroupAvcImeRefWindowSizeINTEL,
            5751u32 => Op::SubgroupAvcImeAdjustRefOffsetINTEL,
            5752u32 => Op::SubgroupAvcImeConvertToMcePayloadINTEL,
            5753u32 => Op::SubgroupAvcImeSetMaxMotionVectorCountINTEL,
            5754u32 => Op::SubgroupAvcImeSetUnidirectionalMixDisableINTEL,
            5755u32 => Op::SubgroupAvcImeSetEarlySearchTerminationThresholdINTEL,
            5756u32 => Op::SubgroupAvcImeSetWeightedSadINTEL,
            5757u32 => Op::SubgroupAvcImeEvaluateWithSingleReferenceINTEL,
            5758u32 => Op::SubgroupAvcImeEvaluateWithDualReferenceINTEL,
            5759u32 => Op::SubgroupAvcImeEvaluateWithSingleReferenceStreaminINTEL,
            5760u32 => Op::SubgroupAvcImeEvaluateWithDualReferenceStreaminINTEL,
            5761u32 => Op::SubgroupAvcImeEvaluateWithSingleReferenceStreamoutINTEL,
            5762u32 => Op::SubgroupAvcImeEvaluateWithDualReferenceStreamoutINTEL,
            5763u32 => Op::SubgroupAvcImeEvaluateWithSingleReferenceStreaminoutINTEL,
            5764u32 => Op::SubgroupAvcImeEvaluateWithDualReferenceStreaminoutINTEL,
            5765u32 => Op::SubgroupAvcImeConvertToMceResultINTEL,
            5766u32 => Op::SubgroupAvcImeGetSingleReferenceStreaminINTEL,
            5767u32 => Op::SubgroupAvcImeGetDualReferenceStreaminINTEL,
            5768u32 => Op::SubgroupAvcImeStripSingleReferenceStreamoutINTEL,
            5769u32 => Op::SubgroupAvcImeStripDualReferenceStreamoutINTEL,
            5770u32 => Op::SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeMotionVectorsINTEL,
            5771u32 => Op::SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeDistortionsINTEL,
            5772u32 => Op::SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeReferenceIdsINTEL,
            5773u32 => Op::SubgroupAvcImeGetStreamoutDualReferenceMajorShapeMotionVectorsINTEL,
            5774u32 => Op::SubgroupAvcImeGetStreamoutDualReferenceMajorShapeDistortionsINTEL,
            5775u32 => Op::SubgroupAvcImeGetStreamoutDualReferenceMajorShapeReferenceIdsINTEL,
            5776u32 => Op::SubgroupAvcImeGetBorderReachedINTEL,
            5777u32 => Op::SubgroupAvcImeGetTruncatedSearchIndicationINTEL,
            5778u32 => Op::SubgroupAvcImeGetUnidirectionalEarlySearchTerminationINTEL,
            5779u32 => Op::SubgroupAvcImeGetWeightingPatternMinimumMotionVectorINTEL,
            5780u32 => Op::SubgroupAvcImeGetWeightingPatternMinimumDistortionINTEL,
            5781u32 => Op::SubgroupAvcFmeInitializeINTEL,
            5782u32 => Op::SubgroupAvcBmeInitializeINTEL,
            5783u32 => Op::SubgroupAvcRefConvertToMcePayloadINTEL,
            5784u32 => Op::SubgroupAvcRefSetBidirectionalMixDisableINTEL,
            5785u32 => Op::SubgroupAvcRefSetBilinearFilterEnableINTEL,
            5786u32 => Op::SubgroupAvcRefEvaluateWithSingleReferenceINTEL,
            5787u32 => Op::SubgroupAvcRefEvaluateWithDualReferenceINTEL,
            5788u32 => Op::SubgroupAvcRefEvaluateWithMultiReferenceINTEL,
            5789u32 => Op::SubgroupAvcRefEvaluateWithMultiReferenceInterlacedINTEL,
            5790u32 => Op::SubgroupAvcRefConvertToMceResultINTEL,
            5791u32 => Op::SubgroupAvcSicInitializeINTEL,
            5792u32 => Op::SubgroupAvcSicConfigureSkcINTEL,
            5793u32 => Op::SubgroupAvcSicConfigureIpeLumaINTEL,
            5794u32 => Op::SubgroupAvcSicConfigureIpeLumaChromaINTEL,
            5795u32 => Op::SubgroupAvcSicGetMotionVectorMaskINTEL,
            5796u32 => Op::SubgroupAvcSicConvertToMcePayloadINTEL,
            5797u32 => Op::SubgroupAvcSicSetIntraLumaShapePenaltyINTEL,
            5798u32 => Op::SubgroupAvcSicSetIntraLumaModeCostFunctionINTEL,
            5799u32 => Op::SubgroupAvcSicSetIntraChromaModeCostFunctionINTEL,
            5800u32 => Op::SubgroupAvcSicSetBilinearFilterEnableINTEL,
            5801u32 => Op::SubgroupAvcSicSetSkcForwardTransformEnableINTEL,
            5802u32 => Op::SubgroupAvcSicSetBlockBasedRawSkipSadINTEL,
            5803u32 => Op::SubgroupAvcSicEvaluateIpeINTEL,
            5804u32 => Op::SubgroupAvcSicEvaluateWithSingleReferenceINTEL,
            5805u32 => Op::SubgroupAvcSicEvaluateWithDualReferenceINTEL,
            5806u32 => Op::SubgroupAvcSicEvaluateWithMultiReferenceINTEL,
            5807u32 => Op::SubgroupAvcSicEvaluateWithMultiReferenceInterlacedINTEL,
            5808u32 => Op::SubgroupAvcSicConvertToMceResultINTEL,
            5809u32 => Op::SubgroupAvcSicGetIpeLumaShapeINTEL,
            5810u32 => Op::SubgroupAvcSicGetBestIpeLumaDistortionINTEL,
            5811u32 => Op::SubgroupAvcSicGetBestIpeChromaDistortionINTEL,
            5812u32 => Op::SubgroupAvcSicGetPackedIpeLumaModesINTEL,
            5813u32 => Op::SubgroupAvcSicGetIpeChromaModeINTEL,
            5814u32 => Op::SubgroupAvcSicGetPackedSkcLumaCountThresholdINTEL,
            5815u32 => Op::SubgroupAvcSicGetPackedSkcLumaSumThresholdINTEL,
            5816u32 => Op::SubgroupAvcSicGetInterRawSadsINTEL,
            6016u32 => Op::RayQueryGetRayTMinKHR,
            6017u32 => Op::RayQueryGetRayFlagsKHR,
            6018u32 => Op::RayQueryGetIntersectionTKHR,
            6019u32 => Op::RayQueryGetIntersectionInstanceCustomIndexKHR,
            6020u32 => Op::RayQueryGetIntersectionInstanceIdKHR,
            6021u32 => Op::RayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetKHR,
            6022u32 => Op::RayQueryGetIntersectionGeometryIndexKHR,
            6023u32 => Op::RayQueryGetIntersectionPrimitiveIndexKHR,
            6024u32 => Op::RayQueryGetIntersectionBarycentricsKHR,
            6025u32 => Op::RayQueryGetIntersectionFrontFaceKHR,
            6026u32 => Op::RayQueryGetIntersectionCandidateAABBOpaqueKHR,
            6027u32 => Op::RayQueryGetIntersectionObjectRayDirectionKHR,
            6028u32 => Op::RayQueryGetIntersectionObjectRayOriginKHR,
            6029u32 => Op::RayQueryGetWorldRayDirectionKHR,
            6030u32 => Op::RayQueryGetWorldRayOriginKHR,
            6031u32 => Op::RayQueryGetIntersectionObjectToWorldKHR,
            6032u32 => Op::RayQueryGetIntersectionWorldToObjectKHR,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "[GLSL.std.450](https://www.khronos.org/registry/spir-v/specs/unified1/GLSL.std.450.html) extended instruction opcode"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum GLOp {
    Round = 1u32,
    RoundEven = 2u32,
    Trunc = 3u32,
    FAbs = 4u32,
    SAbs = 5u32,
    FSign = 6u32,
    SSign = 7u32,
    Floor = 8u32,
    Ceil = 9u32,
    Fract = 10u32,
    Radians = 11u32,
    Degrees = 12u32,
    Sin = 13u32,
    Cos = 14u32,
    Tan = 15u32,
    Asin = 16u32,
    Acos = 17u32,
    Atan = 18u32,
    Sinh = 19u32,
    Cosh = 20u32,
    Tanh = 21u32,
    Asinh = 22u32,
    Acosh = 23u32,
    Atanh = 24u32,
    Atan2 = 25u32,
    Pow = 26u32,
    Exp = 27u32,
    Log = 28u32,
    Exp2 = 29u32,
    Log2 = 30u32,
    Sqrt = 31u32,
    InverseSqrt = 32u32,
    Determinant = 33u32,
    MatrixInverse = 34u32,
    Modf = 35u32,
    ModfStruct = 36u32,
    FMin = 37u32,
    UMin = 38u32,
    SMin = 39u32,
    FMax = 40u32,
    UMax = 41u32,
    SMax = 42u32,
    FClamp = 43u32,
    UClamp = 44u32,
    SClamp = 45u32,
    FMix = 46u32,
    IMix = 47u32,
    Step = 48u32,
    SmoothStep = 49u32,
    Fma = 50u32,
    Frexp = 51u32,
    FrexpStruct = 52u32,
    Ldexp = 53u32,
    PackSnorm4x8 = 54u32,
    PackUnorm4x8 = 55u32,
    PackSnorm2x16 = 56u32,
    PackUnorm2x16 = 57u32,
    PackHalf2x16 = 58u32,
    PackDouble2x32 = 59u32,
    UnpackSnorm2x16 = 60u32,
    UnpackUnorm2x16 = 61u32,
    UnpackHalf2x16 = 62u32,
    UnpackSnorm4x8 = 63u32,
    UnpackUnorm4x8 = 64u32,
    UnpackDouble2x32 = 65u32,
    Length = 66u32,
    Distance = 67u32,
    Cross = 68u32,
    Normalize = 69u32,
    FaceForward = 70u32,
    Reflect = 71u32,
    Refract = 72u32,
    FindILsb = 73u32,
    FindSMsb = 74u32,
    FindUMsb = 75u32,
    InterpolateAtCentroid = 76u32,
    InterpolateAtSample = 77u32,
    InterpolateAtOffset = 78u32,
    NMin = 79u32,
    NMax = 80u32,
    NClamp = 81u32,
}
impl num_traits::FromPrimitive for GLOp {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            1u32 => GLOp::Round,
            2u32 => GLOp::RoundEven,
            3u32 => GLOp::Trunc,
            4u32 => GLOp::FAbs,
            5u32 => GLOp::SAbs,
            6u32 => GLOp::FSign,
            7u32 => GLOp::SSign,
            8u32 => GLOp::Floor,
            9u32 => GLOp::Ceil,
            10u32 => GLOp::Fract,
            11u32 => GLOp::Radians,
            12u32 => GLOp::Degrees,
            13u32 => GLOp::Sin,
            14u32 => GLOp::Cos,
            15u32 => GLOp::Tan,
            16u32 => GLOp::Asin,
            17u32 => GLOp::Acos,
            18u32 => GLOp::Atan,
            19u32 => GLOp::Sinh,
            20u32 => GLOp::Cosh,
            21u32 => GLOp::Tanh,
            22u32 => GLOp::Asinh,
            23u32 => GLOp::Acosh,
            24u32 => GLOp::Atanh,
            25u32 => GLOp::Atan2,
            26u32 => GLOp::Pow,
            27u32 => GLOp::Exp,
            28u32 => GLOp::Log,
            29u32 => GLOp::Exp2,
            30u32 => GLOp::Log2,
            31u32 => GLOp::Sqrt,
            32u32 => GLOp::InverseSqrt,
            33u32 => GLOp::Determinant,
            34u32 => GLOp::MatrixInverse,
            35u32 => GLOp::Modf,
            36u32 => GLOp::ModfStruct,
            37u32 => GLOp::FMin,
            38u32 => GLOp::UMin,
            39u32 => GLOp::SMin,
            40u32 => GLOp::FMax,
            41u32 => GLOp::UMax,
            42u32 => GLOp::SMax,
            43u32 => GLOp::FClamp,
            44u32 => GLOp::UClamp,
            45u32 => GLOp::SClamp,
            46u32 => GLOp::FMix,
            47u32 => GLOp::IMix,
            48u32 => GLOp::Step,
            49u32 => GLOp::SmoothStep,
            50u32 => GLOp::Fma,
            51u32 => GLOp::Frexp,
            52u32 => GLOp::FrexpStruct,
            53u32 => GLOp::Ldexp,
            54u32 => GLOp::PackSnorm4x8,
            55u32 => GLOp::PackUnorm4x8,
            56u32 => GLOp::PackSnorm2x16,
            57u32 => GLOp::PackUnorm2x16,
            58u32 => GLOp::PackHalf2x16,
            59u32 => GLOp::PackDouble2x32,
            60u32 => GLOp::UnpackSnorm2x16,
            61u32 => GLOp::UnpackUnorm2x16,
            62u32 => GLOp::UnpackHalf2x16,
            63u32 => GLOp::UnpackSnorm4x8,
            64u32 => GLOp::UnpackUnorm4x8,
            65u32 => GLOp::UnpackDouble2x32,
            66u32 => GLOp::Length,
            67u32 => GLOp::Distance,
            68u32 => GLOp::Cross,
            69u32 => GLOp::Normalize,
            70u32 => GLOp::FaceForward,
            71u32 => GLOp::Reflect,
            72u32 => GLOp::Refract,
            73u32 => GLOp::FindILsb,
            74u32 => GLOp::FindSMsb,
            75u32 => GLOp::FindUMsb,
            76u32 => GLOp::InterpolateAtCentroid,
            77u32 => GLOp::InterpolateAtSample,
            78u32 => GLOp::InterpolateAtOffset,
            79u32 => GLOp::NMin,
            80u32 => GLOp::NMax,
            81u32 => GLOp::NClamp,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
#[doc = "[OpenCL.std](https://www.khronos.org/registry/spir-v/specs/unified1/OpenCL.ExtendedInstructionSet.100.html) extended instruction opcode"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum CLOp {
    acos = 0u32,
    acosh = 1u32,
    acospi = 2u32,
    asin = 3u32,
    asinh = 4u32,
    asinpi = 5u32,
    atan = 6u32,
    atan2 = 7u32,
    atanh = 8u32,
    atanpi = 9u32,
    atan2pi = 10u32,
    cbrt = 11u32,
    ceil = 12u32,
    copysign = 13u32,
    cos = 14u32,
    cosh = 15u32,
    cospi = 16u32,
    erfc = 17u32,
    erf = 18u32,
    exp = 19u32,
    exp2 = 20u32,
    exp10 = 21u32,
    expm1 = 22u32,
    fabs = 23u32,
    fdim = 24u32,
    floor = 25u32,
    fma = 26u32,
    fmax = 27u32,
    fmin = 28u32,
    fmod = 29u32,
    fract = 30u32,
    frexp = 31u32,
    hypot = 32u32,
    ilogb = 33u32,
    ldexp = 34u32,
    lgamma = 35u32,
    lgamma_r = 36u32,
    log = 37u32,
    log2 = 38u32,
    log10 = 39u32,
    log1p = 40u32,
    logb = 41u32,
    mad = 42u32,
    maxmag = 43u32,
    minmag = 44u32,
    modf = 45u32,
    nan = 46u32,
    nextafter = 47u32,
    pow = 48u32,
    pown = 49u32,
    powr = 50u32,
    remainder = 51u32,
    remquo = 52u32,
    rint = 53u32,
    rootn = 54u32,
    round = 55u32,
    rsqrt = 56u32,
    sin = 57u32,
    sincos = 58u32,
    sinh = 59u32,
    sinpi = 60u32,
    sqrt = 61u32,
    tan = 62u32,
    tanh = 63u32,
    tanpi = 64u32,
    tgamma = 65u32,
    trunc = 66u32,
    half_cos = 67u32,
    half_divide = 68u32,
    half_exp = 69u32,
    half_exp2 = 70u32,
    half_exp10 = 71u32,
    half_log = 72u32,
    half_log2 = 73u32,
    half_log10 = 74u32,
    half_powr = 75u32,
    half_recip = 76u32,
    half_rsqrt = 77u32,
    half_sin = 78u32,
    half_sqrt = 79u32,
    half_tan = 80u32,
    native_cos = 81u32,
    native_divide = 82u32,
    native_exp = 83u32,
    native_exp2 = 84u32,
    native_exp10 = 85u32,
    native_log = 86u32,
    native_log2 = 87u32,
    native_log10 = 88u32,
    native_powr = 89u32,
    native_recip = 90u32,
    native_rsqrt = 91u32,
    native_sin = 92u32,
    native_sqrt = 93u32,
    native_tan = 94u32,
    s_abs = 141u32,
    s_abs_diff = 142u32,
    s_add_sat = 143u32,
    u_add_sat = 144u32,
    s_hadd = 145u32,
    u_hadd = 146u32,
    s_rhadd = 147u32,
    u_rhadd = 148u32,
    s_clamp = 149u32,
    u_clamp = 150u32,
    clz = 151u32,
    ctz = 152u32,
    s_mad_hi = 153u32,
    u_mad_sat = 154u32,
    s_mad_sat = 155u32,
    s_max = 156u32,
    u_max = 157u32,
    s_min = 158u32,
    u_min = 159u32,
    s_mul_hi = 160u32,
    rotate = 161u32,
    s_sub_sat = 162u32,
    u_sub_sat = 163u32,
    u_upsample = 164u32,
    s_upsample = 165u32,
    popcount = 166u32,
    s_mad24 = 167u32,
    u_mad24 = 168u32,
    s_mul24 = 169u32,
    u_mul24 = 170u32,
    u_abs = 201u32,
    u_abs_diff = 202u32,
    u_mul_hi = 203u32,
    u_mad_hi = 204u32,
    fclamp = 95u32,
    degrees = 96u32,
    fmax_common = 97u32,
    fmin_common = 98u32,
    mix = 99u32,
    radians = 100u32,
    step = 101u32,
    smoothstep = 102u32,
    sign = 103u32,
    cross = 104u32,
    distance = 105u32,
    length = 106u32,
    normalize = 107u32,
    fast_distance = 108u32,
    fast_length = 109u32,
    fast_normalize = 110u32,
    bitselect = 186u32,
    select = 187u32,
    vloadn = 171u32,
    vstoren = 172u32,
    vload_half = 173u32,
    vload_halfn = 174u32,
    vstore_half = 175u32,
    vstore_half_r = 176u32,
    vstore_halfn = 177u32,
    vstore_halfn_r = 178u32,
    vloada_halfn = 179u32,
    vstorea_halfn = 180u32,
    vstorea_halfn_r = 181u32,
    shuffle = 182u32,
    shuffle2 = 183u32,
    printf = 184u32,
    prefetch = 185u32,
}
impl num_traits::FromPrimitive for CLOp {
    #[allow(trivial_numeric_casts)]
    fn from_i64(n: i64) -> Option<Self> {
        Some(match n as u32 {
            0u32 => CLOp::acos,
            1u32 => CLOp::acosh,
            2u32 => CLOp::acospi,
            3u32 => CLOp::asin,
            4u32 => CLOp::asinh,
            5u32 => CLOp::asinpi,
            6u32 => CLOp::atan,
            7u32 => CLOp::atan2,
            8u32 => CLOp::atanh,
            9u32 => CLOp::atanpi,
            10u32 => CLOp::atan2pi,
            11u32 => CLOp::cbrt,
            12u32 => CLOp::ceil,
            13u32 => CLOp::copysign,
            14u32 => CLOp::cos,
            15u32 => CLOp::cosh,
            16u32 => CLOp::cospi,
            17u32 => CLOp::erfc,
            18u32 => CLOp::erf,
            19u32 => CLOp::exp,
            20u32 => CLOp::exp2,
            21u32 => CLOp::exp10,
            22u32 => CLOp::expm1,
            23u32 => CLOp::fabs,
            24u32 => CLOp::fdim,
            25u32 => CLOp::floor,
            26u32 => CLOp::fma,
            27u32 => CLOp::fmax,
            28u32 => CLOp::fmin,
            29u32 => CLOp::fmod,
            30u32 => CLOp::fract,
            31u32 => CLOp::frexp,
            32u32 => CLOp::hypot,
            33u32 => CLOp::ilogb,
            34u32 => CLOp::ldexp,
            35u32 => CLOp::lgamma,
            36u32 => CLOp::lgamma_r,
            37u32 => CLOp::log,
            38u32 => CLOp::log2,
            39u32 => CLOp::log10,
            40u32 => CLOp::log1p,
            41u32 => CLOp::logb,
            42u32 => CLOp::mad,
            43u32 => CLOp::maxmag,
            44u32 => CLOp::minmag,
            45u32 => CLOp::modf,
            46u32 => CLOp::nan,
            47u32 => CLOp::nextafter,
            48u32 => CLOp::pow,
            49u32 => CLOp::pown,
            50u32 => CLOp::powr,
            51u32 => CLOp::remainder,
            52u32 => CLOp::remquo,
            53u32 => CLOp::rint,
            54u32 => CLOp::rootn,
            55u32 => CLOp::round,
            56u32 => CLOp::rsqrt,
            57u32 => CLOp::sin,
            58u32 => CLOp::sincos,
            59u32 => CLOp::sinh,
            60u32 => CLOp::sinpi,
            61u32 => CLOp::sqrt,
            62u32 => CLOp::tan,
            63u32 => CLOp::tanh,
            64u32 => CLOp::tanpi,
            65u32 => CLOp::tgamma,
            66u32 => CLOp::trunc,
            67u32 => CLOp::half_cos,
            68u32 => CLOp::half_divide,
            69u32 => CLOp::half_exp,
            70u32 => CLOp::half_exp2,
            71u32 => CLOp::half_exp10,
            72u32 => CLOp::half_log,
            73u32 => CLOp::half_log2,
            74u32 => CLOp::half_log10,
            75u32 => CLOp::half_powr,
            76u32 => CLOp::half_recip,
            77u32 => CLOp::half_rsqrt,
            78u32 => CLOp::half_sin,
            79u32 => CLOp::half_sqrt,
            80u32 => CLOp::half_tan,
            81u32 => CLOp::native_cos,
            82u32 => CLOp::native_divide,
            83u32 => CLOp::native_exp,
            84u32 => CLOp::native_exp2,
            85u32 => CLOp::native_exp10,
            86u32 => CLOp::native_log,
            87u32 => CLOp::native_log2,
            88u32 => CLOp::native_log10,
            89u32 => CLOp::native_powr,
            90u32 => CLOp::native_recip,
            91u32 => CLOp::native_rsqrt,
            92u32 => CLOp::native_sin,
            93u32 => CLOp::native_sqrt,
            94u32 => CLOp::native_tan,
            141u32 => CLOp::s_abs,
            142u32 => CLOp::s_abs_diff,
            143u32 => CLOp::s_add_sat,
            144u32 => CLOp::u_add_sat,
            145u32 => CLOp::s_hadd,
            146u32 => CLOp::u_hadd,
            147u32 => CLOp::s_rhadd,
            148u32 => CLOp::u_rhadd,
            149u32 => CLOp::s_clamp,
            150u32 => CLOp::u_clamp,
            151u32 => CLOp::clz,
            152u32 => CLOp::ctz,
            153u32 => CLOp::s_mad_hi,
            154u32 => CLOp::u_mad_sat,
            155u32 => CLOp::s_mad_sat,
            156u32 => CLOp::s_max,
            157u32 => CLOp::u_max,
            158u32 => CLOp::s_min,
            159u32 => CLOp::u_min,
            160u32 => CLOp::s_mul_hi,
            161u32 => CLOp::rotate,
            162u32 => CLOp::s_sub_sat,
            163u32 => CLOp::u_sub_sat,
            164u32 => CLOp::u_upsample,
            165u32 => CLOp::s_upsample,
            166u32 => CLOp::popcount,
            167u32 => CLOp::s_mad24,
            168u32 => CLOp::u_mad24,
            169u32 => CLOp::s_mul24,
            170u32 => CLOp::u_mul24,
            201u32 => CLOp::u_abs,
            202u32 => CLOp::u_abs_diff,
            203u32 => CLOp::u_mul_hi,
            204u32 => CLOp::u_mad_hi,
            95u32 => CLOp::fclamp,
            96u32 => CLOp::degrees,
            97u32 => CLOp::fmax_common,
            98u32 => CLOp::fmin_common,
            99u32 => CLOp::mix,
            100u32 => CLOp::radians,
            101u32 => CLOp::step,
            102u32 => CLOp::smoothstep,
            103u32 => CLOp::sign,
            104u32 => CLOp::cross,
            105u32 => CLOp::distance,
            106u32 => CLOp::length,
            107u32 => CLOp::normalize,
            108u32 => CLOp::fast_distance,
            109u32 => CLOp::fast_length,
            110u32 => CLOp::fast_normalize,
            186u32 => CLOp::bitselect,
            187u32 => CLOp::select,
            171u32 => CLOp::vloadn,
            172u32 => CLOp::vstoren,
            173u32 => CLOp::vload_half,
            174u32 => CLOp::vload_halfn,
            175u32 => CLOp::vstore_half,
            176u32 => CLOp::vstore_half_r,
            177u32 => CLOp::vstore_halfn,
            178u32 => CLOp::vstore_halfn_r,
            179u32 => CLOp::vloada_halfn,
            180u32 => CLOp::vstorea_halfn,
            181u32 => CLOp::vstorea_halfn_r,
            182u32 => CLOp::shuffle,
            183u32 => CLOp::shuffle2,
            184u32 => CLOp::printf,
            185u32 => CLOp::prefetch,
            _ => return None,
        })
    }
    fn from_u64(n: u64) -> Option<Self> {
        Self::from_i64(n as i64)
    }
}
