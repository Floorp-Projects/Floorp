// AUTOMATICALLY GENERATED from the SPIR-V JSON grammar:
//   external/spirv.core.grammar.json.
// DO NOT MODIFY!

pub type Word = u32;
pub const MAGIC_NUMBER: u32 = 0x07230203;
pub const MAJOR_VERSION: u8 = 1u8;
pub const MINOR_VERSION: u8 = 6u8;
pub const REVISION: u8 = 1u8;
bitflags! { # [doc = "SPIR-V operand kind: [ImageOperands](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_image_operands_a_image_operands)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct ImageOperands : u32 { const NONE = 0u32 ; const BIAS = 1u32 ; const LOD = 2u32 ; const GRAD = 4u32 ; const CONST_OFFSET = 8u32 ; const OFFSET = 16u32 ; const CONST_OFFSETS = 32u32 ; const SAMPLE = 64u32 ; const MIN_LOD = 128u32 ; const MAKE_TEXEL_AVAILABLE = 256u32 ; const MAKE_TEXEL_AVAILABLE_KHR = 256u32 ; const MAKE_TEXEL_VISIBLE = 512u32 ; const MAKE_TEXEL_VISIBLE_KHR = 512u32 ; const NON_PRIVATE_TEXEL = 1024u32 ; const NON_PRIVATE_TEXEL_KHR = 1024u32 ; const VOLATILE_TEXEL = 2048u32 ; const VOLATILE_TEXEL_KHR = 2048u32 ; const SIGN_EXTEND = 4096u32 ; const ZERO_EXTEND = 8192u32 ; const NONTEMPORAL = 16384u32 ; const OFFSETS = 65536u32 ; } }
bitflags! { # [doc = "SPIR-V operand kind: [FPFastMathMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_fp_fast_math_mode_a_fp_fast_math_mode)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct FPFastMathMode : u32 { const NONE = 0u32 ; const NOT_NAN = 1u32 ; const NOT_INF = 2u32 ; const NSZ = 4u32 ; const ALLOW_RECIP = 8u32 ; const FAST = 16u32 ; const ALLOW_CONTRACT_FAST_INTEL = 65536u32 ; const ALLOW_REASSOC_INTEL = 131072u32 ; } }
bitflags! { # [doc = "SPIR-V operand kind: [SelectionControl](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_selection_control_a_selection_control)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct SelectionControl : u32 { const NONE = 0u32 ; const FLATTEN = 1u32 ; const DONT_FLATTEN = 2u32 ; } }
bitflags! { # [doc = "SPIR-V operand kind: [LoopControl](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_loop_control_a_loop_control)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct LoopControl : u32 { const NONE = 0u32 ; const UNROLL = 1u32 ; const DONT_UNROLL = 2u32 ; const DEPENDENCY_INFINITE = 4u32 ; const DEPENDENCY_LENGTH = 8u32 ; const MIN_ITERATIONS = 16u32 ; const MAX_ITERATIONS = 32u32 ; const ITERATION_MULTIPLE = 64u32 ; const PEEL_COUNT = 128u32 ; const PARTIAL_COUNT = 256u32 ; const INITIATION_INTERVAL_INTEL = 65536u32 ; const MAX_CONCURRENCY_INTEL = 131072u32 ; const DEPENDENCY_ARRAY_INTEL = 262144u32 ; const PIPELINE_ENABLE_INTEL = 524288u32 ; const LOOP_COALESCE_INTEL = 1048576u32 ; const MAX_INTERLEAVING_INTEL = 2097152u32 ; const SPECULATED_ITERATIONS_INTEL = 4194304u32 ; const NO_FUSION_INTEL = 8388608u32 ; const LOOP_COUNT_INTEL = 16777216u32 ; const MAX_REINVOCATION_DELAY_INTEL = 33554432u32 ; } }
bitflags! { # [doc = "SPIR-V operand kind: [FunctionControl](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_function_control_a_function_control)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct FunctionControl : u32 { const NONE = 0u32 ; const INLINE = 1u32 ; const DONT_INLINE = 2u32 ; const PURE = 4u32 ; const CONST = 8u32 ; const OPT_NONE_INTEL = 65536u32 ; } }
bitflags! { # [doc = "SPIR-V operand kind: [MemorySemantics](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_memory_semantics_a_memory_semantics)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct MemorySemantics : u32 { const RELAXED = 0u32 ; const NONE = 0u32 ; const ACQUIRE = 2u32 ; const RELEASE = 4u32 ; const ACQUIRE_RELEASE = 8u32 ; const SEQUENTIALLY_CONSISTENT = 16u32 ; const UNIFORM_MEMORY = 64u32 ; const SUBGROUP_MEMORY = 128u32 ; const WORKGROUP_MEMORY = 256u32 ; const CROSS_WORKGROUP_MEMORY = 512u32 ; const ATOMIC_COUNTER_MEMORY = 1024u32 ; const IMAGE_MEMORY = 2048u32 ; const OUTPUT_MEMORY = 4096u32 ; const OUTPUT_MEMORY_KHR = 4096u32 ; const MAKE_AVAILABLE = 8192u32 ; const MAKE_AVAILABLE_KHR = 8192u32 ; const MAKE_VISIBLE = 16384u32 ; const MAKE_VISIBLE_KHR = 16384u32 ; const VOLATILE = 32768u32 ; } }
bitflags! { # [doc = "SPIR-V operand kind: [MemoryAccess](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_memory_access_a_memory_access)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct MemoryAccess : u32 { const NONE = 0u32 ; const VOLATILE = 1u32 ; const ALIGNED = 2u32 ; const NONTEMPORAL = 4u32 ; const MAKE_POINTER_AVAILABLE = 8u32 ; const MAKE_POINTER_AVAILABLE_KHR = 8u32 ; const MAKE_POINTER_VISIBLE = 16u32 ; const MAKE_POINTER_VISIBLE_KHR = 16u32 ; const NON_PRIVATE_POINTER = 32u32 ; const NON_PRIVATE_POINTER_KHR = 32u32 ; const ALIAS_SCOPE_INTEL_MASK = 65536u32 ; const NO_ALIAS_INTEL_MASK = 131072u32 ; } }
bitflags! { # [doc = "SPIR-V operand kind: [KernelProfilingInfo](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_kernel_profiling_info_a_kernel_profiling_info)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct KernelProfilingInfo : u32 { const NONE = 0u32 ; const CMD_EXEC_TIME = 1u32 ; } }
bitflags! { # [doc = "SPIR-V operand kind: [RayFlags](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_ray_flags_a_ray_flags)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct RayFlags : u32 { const NONE_KHR = 0u32 ; const OPAQUE_KHR = 1u32 ; const NO_OPAQUE_KHR = 2u32 ; const TERMINATE_ON_FIRST_HIT_KHR = 4u32 ; const SKIP_CLOSEST_HIT_SHADER_KHR = 8u32 ; const CULL_BACK_FACING_TRIANGLES_KHR = 16u32 ; const CULL_FRONT_FACING_TRIANGLES_KHR = 32u32 ; const CULL_OPAQUE_KHR = 64u32 ; const CULL_NO_OPAQUE_KHR = 128u32 ; const SKIP_TRIANGLES_KHR = 256u32 ; const SKIP_AAB_BS_KHR = 512u32 ; const FORCE_OPACITY_MICROMAP2_STATE_EXT = 1024u32 ; } }
bitflags! { # [doc = "SPIR-V operand kind: [FragmentShadingRate](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_fragment_shading_rate_a_fragment_shading_rate)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct FragmentShadingRate : u32 { const VERTICAL2_PIXELS = 1u32 ; const VERTICAL4_PIXELS = 2u32 ; const HORIZONTAL2_PIXELS = 4u32 ; const HORIZONTAL4_PIXELS = 8u32 ; } }
#[doc = "SPIR-V operand kind: [SourceLanguage](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_source_language_a_source_language)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum SourceLanguage {
    Unknown = 0u32,
    ESSL = 1u32,
    GLSL = 2u32,
    OpenCL_C = 3u32,
    OpenCL_CPP = 4u32,
    HLSL = 5u32,
    CPP_for_OpenCL = 6u32,
    SYCL = 7u32,
    HERO_C = 8u32,
    NZSL = 9u32,
    WGSL = 10u32,
    Slang = 11u32,
}
impl SourceLanguage {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=11u32 => unsafe { core::mem::transmute::<u32, SourceLanguage>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl SourceLanguage {}
impl core::str::FromStr for SourceLanguage {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Unknown" => Ok(Self::Unknown),
            "ESSL" => Ok(Self::ESSL),
            "GLSL" => Ok(Self::GLSL),
            "OpenCL_C" => Ok(Self::OpenCL_C),
            "OpenCL_CPP" => Ok(Self::OpenCL_CPP),
            "HLSL" => Ok(Self::HLSL),
            "CPP_for_OpenCL" => Ok(Self::CPP_for_OpenCL),
            "SYCL" => Ok(Self::SYCL),
            "HERO_C" => Ok(Self::HERO_C),
            "NZSL" => Ok(Self::NZSL),
            "WGSL" => Ok(Self::WGSL),
            "Slang" => Ok(Self::Slang),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [ExecutionModel](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_execution_model_a_execution_model)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
    TaskEXT = 5364u32,
    MeshEXT = 5365u32,
}
impl ExecutionModel {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=6u32 => unsafe { core::mem::transmute::<u32, ExecutionModel>(n) },
            5267u32..=5268u32 => unsafe { core::mem::transmute::<u32, ExecutionModel>(n) },
            5313u32..=5318u32 => unsafe { core::mem::transmute::<u32, ExecutionModel>(n) },
            5364u32..=5365u32 => unsafe { core::mem::transmute::<u32, ExecutionModel>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl ExecutionModel {
    pub const RayGenerationKHR: Self = Self::RayGenerationNV;
    pub const IntersectionKHR: Self = Self::IntersectionNV;
    pub const AnyHitKHR: Self = Self::AnyHitNV;
    pub const ClosestHitKHR: Self = Self::ClosestHitNV;
    pub const MissKHR: Self = Self::MissNV;
    pub const CallableKHR: Self = Self::CallableNV;
}
impl core::str::FromStr for ExecutionModel {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Vertex" => Ok(Self::Vertex),
            "TessellationControl" => Ok(Self::TessellationControl),
            "TessellationEvaluation" => Ok(Self::TessellationEvaluation),
            "Geometry" => Ok(Self::Geometry),
            "Fragment" => Ok(Self::Fragment),
            "GLCompute" => Ok(Self::GLCompute),
            "Kernel" => Ok(Self::Kernel),
            "TaskNV" => Ok(Self::TaskNV),
            "MeshNV" => Ok(Self::MeshNV),
            "RayGenerationNV" => Ok(Self::RayGenerationNV),
            "RayGenerationKHR" => Ok(Self::RayGenerationNV),
            "IntersectionNV" => Ok(Self::IntersectionNV),
            "IntersectionKHR" => Ok(Self::IntersectionNV),
            "AnyHitNV" => Ok(Self::AnyHitNV),
            "AnyHitKHR" => Ok(Self::AnyHitNV),
            "ClosestHitNV" => Ok(Self::ClosestHitNV),
            "ClosestHitKHR" => Ok(Self::ClosestHitNV),
            "MissNV" => Ok(Self::MissNV),
            "MissKHR" => Ok(Self::MissNV),
            "CallableNV" => Ok(Self::CallableNV),
            "CallableKHR" => Ok(Self::CallableNV),
            "TaskEXT" => Ok(Self::TaskEXT),
            "MeshEXT" => Ok(Self::MeshEXT),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [AddressingModel](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_addressing_model_a_addressing_model)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum AddressingModel {
    Logical = 0u32,
    Physical32 = 1u32,
    Physical64 = 2u32,
    PhysicalStorageBuffer64 = 5348u32,
}
impl AddressingModel {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=2u32 => unsafe { core::mem::transmute::<u32, AddressingModel>(n) },
            5348u32 => unsafe { core::mem::transmute::<u32, AddressingModel>(5348u32) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl AddressingModel {
    pub const PhysicalStorageBuffer64EXT: Self = Self::PhysicalStorageBuffer64;
}
impl core::str::FromStr for AddressingModel {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Logical" => Ok(Self::Logical),
            "Physical32" => Ok(Self::Physical32),
            "Physical64" => Ok(Self::Physical64),
            "PhysicalStorageBuffer64" => Ok(Self::PhysicalStorageBuffer64),
            "PhysicalStorageBuffer64EXT" => Ok(Self::PhysicalStorageBuffer64),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [MemoryModel](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_memory_model_a_memory_model)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum MemoryModel {
    Simple = 0u32,
    GLSL450 = 1u32,
    OpenCL = 2u32,
    Vulkan = 3u32,
}
impl MemoryModel {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=3u32 => unsafe { core::mem::transmute::<u32, MemoryModel>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl MemoryModel {
    pub const VulkanKHR: Self = Self::Vulkan;
}
impl core::str::FromStr for MemoryModel {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Simple" => Ok(Self::Simple),
            "GLSL450" => Ok(Self::GLSL450),
            "OpenCL" => Ok(Self::OpenCL),
            "Vulkan" => Ok(Self::Vulkan),
            "VulkanKHR" => Ok(Self::Vulkan),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [ExecutionMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_execution_mode_a_execution_mode)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
    NonCoherentColorAttachmentReadEXT = 4169u32,
    NonCoherentDepthAttachmentReadEXT = 4170u32,
    NonCoherentStencilAttachmentReadEXT = 4171u32,
    SubgroupUniformControlFlowKHR = 4421u32,
    PostDepthCoverage = 4446u32,
    DenormPreserve = 4459u32,
    DenormFlushToZero = 4460u32,
    SignedZeroInfNanPreserve = 4461u32,
    RoundingModeRTE = 4462u32,
    RoundingModeRTZ = 4463u32,
    EarlyAndLateFragmentTestsAMD = 5017u32,
    StencilRefReplacingEXT = 5027u32,
    CoalescingAMDX = 5069u32,
    MaxNodeRecursionAMDX = 5071u32,
    StaticNumWorkgroupsAMDX = 5072u32,
    ShaderIndexAMDX = 5073u32,
    MaxNumWorkgroupsAMDX = 5077u32,
    StencilRefUnchangedFrontAMD = 5079u32,
    StencilRefGreaterFrontAMD = 5080u32,
    StencilRefLessFrontAMD = 5081u32,
    StencilRefUnchangedBackAMD = 5082u32,
    StencilRefGreaterBackAMD = 5083u32,
    StencilRefLessBackAMD = 5084u32,
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
    SharedLocalMemorySizeINTEL = 5618u32,
    RoundingModeRTPINTEL = 5620u32,
    RoundingModeRTNINTEL = 5621u32,
    FloatingPointModeALTINTEL = 5622u32,
    FloatingPointModeIEEEINTEL = 5623u32,
    MaxWorkgroupSizeINTEL = 5893u32,
    MaxWorkDimINTEL = 5894u32,
    NoGlobalOffsetINTEL = 5895u32,
    NumSIMDWorkitemsINTEL = 5896u32,
    SchedulerTargetFmaxMhzINTEL = 5903u32,
    StreamingInterfaceINTEL = 6154u32,
    RegisterMapInterfaceINTEL = 6160u32,
    NamedBarrierCountINTEL = 6417u32,
}
impl ExecutionMode {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=12u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            14u32..=31u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            33u32..=39u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            4169u32..=4171u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            4421u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(4421u32) },
            4446u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(4446u32) },
            4459u32..=4463u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            5017u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(5017u32) },
            5027u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(5027u32) },
            5069u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(5069u32) },
            5071u32..=5073u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            5077u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(5077u32) },
            5079u32..=5084u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            5269u32..=5270u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            5289u32..=5290u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            5298u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(5298u32) },
            5366u32..=5371u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            5618u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(5618u32) },
            5620u32..=5623u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            5893u32..=5896u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(n) },
            5903u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(5903u32) },
            6154u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(6154u32) },
            6160u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(6160u32) },
            6417u32 => unsafe { core::mem::transmute::<u32, ExecutionMode>(6417u32) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl ExecutionMode {
    pub const OutputLinesEXT: Self = Self::OutputLinesNV;
    pub const OutputPrimitivesEXT: Self = Self::OutputPrimitivesNV;
    pub const OutputTrianglesEXT: Self = Self::OutputTrianglesNV;
}
impl core::str::FromStr for ExecutionMode {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Invocations" => Ok(Self::Invocations),
            "SpacingEqual" => Ok(Self::SpacingEqual),
            "SpacingFractionalEven" => Ok(Self::SpacingFractionalEven),
            "SpacingFractionalOdd" => Ok(Self::SpacingFractionalOdd),
            "VertexOrderCw" => Ok(Self::VertexOrderCw),
            "VertexOrderCcw" => Ok(Self::VertexOrderCcw),
            "PixelCenterInteger" => Ok(Self::PixelCenterInteger),
            "OriginUpperLeft" => Ok(Self::OriginUpperLeft),
            "OriginLowerLeft" => Ok(Self::OriginLowerLeft),
            "EarlyFragmentTests" => Ok(Self::EarlyFragmentTests),
            "PointMode" => Ok(Self::PointMode),
            "Xfb" => Ok(Self::Xfb),
            "DepthReplacing" => Ok(Self::DepthReplacing),
            "DepthGreater" => Ok(Self::DepthGreater),
            "DepthLess" => Ok(Self::DepthLess),
            "DepthUnchanged" => Ok(Self::DepthUnchanged),
            "LocalSize" => Ok(Self::LocalSize),
            "LocalSizeHint" => Ok(Self::LocalSizeHint),
            "InputPoints" => Ok(Self::InputPoints),
            "InputLines" => Ok(Self::InputLines),
            "InputLinesAdjacency" => Ok(Self::InputLinesAdjacency),
            "Triangles" => Ok(Self::Triangles),
            "InputTrianglesAdjacency" => Ok(Self::InputTrianglesAdjacency),
            "Quads" => Ok(Self::Quads),
            "Isolines" => Ok(Self::Isolines),
            "OutputVertices" => Ok(Self::OutputVertices),
            "OutputPoints" => Ok(Self::OutputPoints),
            "OutputLineStrip" => Ok(Self::OutputLineStrip),
            "OutputTriangleStrip" => Ok(Self::OutputTriangleStrip),
            "VecTypeHint" => Ok(Self::VecTypeHint),
            "ContractionOff" => Ok(Self::ContractionOff),
            "Initializer" => Ok(Self::Initializer),
            "Finalizer" => Ok(Self::Finalizer),
            "SubgroupSize" => Ok(Self::SubgroupSize),
            "SubgroupsPerWorkgroup" => Ok(Self::SubgroupsPerWorkgroup),
            "SubgroupsPerWorkgroupId" => Ok(Self::SubgroupsPerWorkgroupId),
            "LocalSizeId" => Ok(Self::LocalSizeId),
            "LocalSizeHintId" => Ok(Self::LocalSizeHintId),
            "NonCoherentColorAttachmentReadEXT" => Ok(Self::NonCoherentColorAttachmentReadEXT),
            "NonCoherentDepthAttachmentReadEXT" => Ok(Self::NonCoherentDepthAttachmentReadEXT),
            "NonCoherentStencilAttachmentReadEXT" => Ok(Self::NonCoherentStencilAttachmentReadEXT),
            "SubgroupUniformControlFlowKHR" => Ok(Self::SubgroupUniformControlFlowKHR),
            "PostDepthCoverage" => Ok(Self::PostDepthCoverage),
            "DenormPreserve" => Ok(Self::DenormPreserve),
            "DenormFlushToZero" => Ok(Self::DenormFlushToZero),
            "SignedZeroInfNanPreserve" => Ok(Self::SignedZeroInfNanPreserve),
            "RoundingModeRTE" => Ok(Self::RoundingModeRTE),
            "RoundingModeRTZ" => Ok(Self::RoundingModeRTZ),
            "EarlyAndLateFragmentTestsAMD" => Ok(Self::EarlyAndLateFragmentTestsAMD),
            "StencilRefReplacingEXT" => Ok(Self::StencilRefReplacingEXT),
            "CoalescingAMDX" => Ok(Self::CoalescingAMDX),
            "MaxNodeRecursionAMDX" => Ok(Self::MaxNodeRecursionAMDX),
            "StaticNumWorkgroupsAMDX" => Ok(Self::StaticNumWorkgroupsAMDX),
            "ShaderIndexAMDX" => Ok(Self::ShaderIndexAMDX),
            "MaxNumWorkgroupsAMDX" => Ok(Self::MaxNumWorkgroupsAMDX),
            "StencilRefUnchangedFrontAMD" => Ok(Self::StencilRefUnchangedFrontAMD),
            "StencilRefGreaterFrontAMD" => Ok(Self::StencilRefGreaterFrontAMD),
            "StencilRefLessFrontAMD" => Ok(Self::StencilRefLessFrontAMD),
            "StencilRefUnchangedBackAMD" => Ok(Self::StencilRefUnchangedBackAMD),
            "StencilRefGreaterBackAMD" => Ok(Self::StencilRefGreaterBackAMD),
            "StencilRefLessBackAMD" => Ok(Self::StencilRefLessBackAMD),
            "OutputLinesNV" => Ok(Self::OutputLinesNV),
            "OutputLinesEXT" => Ok(Self::OutputLinesNV),
            "OutputPrimitivesNV" => Ok(Self::OutputPrimitivesNV),
            "OutputPrimitivesEXT" => Ok(Self::OutputPrimitivesNV),
            "DerivativeGroupQuadsNV" => Ok(Self::DerivativeGroupQuadsNV),
            "DerivativeGroupLinearNV" => Ok(Self::DerivativeGroupLinearNV),
            "OutputTrianglesNV" => Ok(Self::OutputTrianglesNV),
            "OutputTrianglesEXT" => Ok(Self::OutputTrianglesNV),
            "PixelInterlockOrderedEXT" => Ok(Self::PixelInterlockOrderedEXT),
            "PixelInterlockUnorderedEXT" => Ok(Self::PixelInterlockUnorderedEXT),
            "SampleInterlockOrderedEXT" => Ok(Self::SampleInterlockOrderedEXT),
            "SampleInterlockUnorderedEXT" => Ok(Self::SampleInterlockUnorderedEXT),
            "ShadingRateInterlockOrderedEXT" => Ok(Self::ShadingRateInterlockOrderedEXT),
            "ShadingRateInterlockUnorderedEXT" => Ok(Self::ShadingRateInterlockUnorderedEXT),
            "SharedLocalMemorySizeINTEL" => Ok(Self::SharedLocalMemorySizeINTEL),
            "RoundingModeRTPINTEL" => Ok(Self::RoundingModeRTPINTEL),
            "RoundingModeRTNINTEL" => Ok(Self::RoundingModeRTNINTEL),
            "FloatingPointModeALTINTEL" => Ok(Self::FloatingPointModeALTINTEL),
            "FloatingPointModeIEEEINTEL" => Ok(Self::FloatingPointModeIEEEINTEL),
            "MaxWorkgroupSizeINTEL" => Ok(Self::MaxWorkgroupSizeINTEL),
            "MaxWorkDimINTEL" => Ok(Self::MaxWorkDimINTEL),
            "NoGlobalOffsetINTEL" => Ok(Self::NoGlobalOffsetINTEL),
            "NumSIMDWorkitemsINTEL" => Ok(Self::NumSIMDWorkitemsINTEL),
            "SchedulerTargetFmaxMhzINTEL" => Ok(Self::SchedulerTargetFmaxMhzINTEL),
            "StreamingInterfaceINTEL" => Ok(Self::StreamingInterfaceINTEL),
            "RegisterMapInterfaceINTEL" => Ok(Self::RegisterMapInterfaceINTEL),
            "NamedBarrierCountINTEL" => Ok(Self::NamedBarrierCountINTEL),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [StorageClass](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_storage_class_a_storage_class)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
    TileImageEXT = 4172u32,
    NodePayloadAMDX = 5068u32,
    NodeOutputPayloadAMDX = 5076u32,
    CallableDataNV = 5328u32,
    IncomingCallableDataNV = 5329u32,
    RayPayloadNV = 5338u32,
    HitAttributeNV = 5339u32,
    IncomingRayPayloadNV = 5342u32,
    ShaderRecordBufferNV = 5343u32,
    PhysicalStorageBuffer = 5349u32,
    HitObjectAttributeNV = 5385u32,
    TaskPayloadWorkgroupEXT = 5402u32,
    CodeSectionINTEL = 5605u32,
    DeviceOnlyINTEL = 5936u32,
    HostOnlyINTEL = 5937u32,
}
impl StorageClass {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=12u32 => unsafe { core::mem::transmute::<u32, StorageClass>(n) },
            4172u32 => unsafe { core::mem::transmute::<u32, StorageClass>(4172u32) },
            5068u32 => unsafe { core::mem::transmute::<u32, StorageClass>(5068u32) },
            5076u32 => unsafe { core::mem::transmute::<u32, StorageClass>(5076u32) },
            5328u32..=5329u32 => unsafe { core::mem::transmute::<u32, StorageClass>(n) },
            5338u32..=5339u32 => unsafe { core::mem::transmute::<u32, StorageClass>(n) },
            5342u32..=5343u32 => unsafe { core::mem::transmute::<u32, StorageClass>(n) },
            5349u32 => unsafe { core::mem::transmute::<u32, StorageClass>(5349u32) },
            5385u32 => unsafe { core::mem::transmute::<u32, StorageClass>(5385u32) },
            5402u32 => unsafe { core::mem::transmute::<u32, StorageClass>(5402u32) },
            5605u32 => unsafe { core::mem::transmute::<u32, StorageClass>(5605u32) },
            5936u32..=5937u32 => unsafe { core::mem::transmute::<u32, StorageClass>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl StorageClass {
    pub const CallableDataKHR: Self = Self::CallableDataNV;
    pub const IncomingCallableDataKHR: Self = Self::IncomingCallableDataNV;
    pub const RayPayloadKHR: Self = Self::RayPayloadNV;
    pub const HitAttributeKHR: Self = Self::HitAttributeNV;
    pub const IncomingRayPayloadKHR: Self = Self::IncomingRayPayloadNV;
    pub const ShaderRecordBufferKHR: Self = Self::ShaderRecordBufferNV;
    pub const PhysicalStorageBufferEXT: Self = Self::PhysicalStorageBuffer;
}
impl core::str::FromStr for StorageClass {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "UniformConstant" => Ok(Self::UniformConstant),
            "Input" => Ok(Self::Input),
            "Uniform" => Ok(Self::Uniform),
            "Output" => Ok(Self::Output),
            "Workgroup" => Ok(Self::Workgroup),
            "CrossWorkgroup" => Ok(Self::CrossWorkgroup),
            "Private" => Ok(Self::Private),
            "Function" => Ok(Self::Function),
            "Generic" => Ok(Self::Generic),
            "PushConstant" => Ok(Self::PushConstant),
            "AtomicCounter" => Ok(Self::AtomicCounter),
            "Image" => Ok(Self::Image),
            "StorageBuffer" => Ok(Self::StorageBuffer),
            "TileImageEXT" => Ok(Self::TileImageEXT),
            "NodePayloadAMDX" => Ok(Self::NodePayloadAMDX),
            "NodeOutputPayloadAMDX" => Ok(Self::NodeOutputPayloadAMDX),
            "CallableDataNV" => Ok(Self::CallableDataNV),
            "CallableDataKHR" => Ok(Self::CallableDataNV),
            "IncomingCallableDataNV" => Ok(Self::IncomingCallableDataNV),
            "IncomingCallableDataKHR" => Ok(Self::IncomingCallableDataNV),
            "RayPayloadNV" => Ok(Self::RayPayloadNV),
            "RayPayloadKHR" => Ok(Self::RayPayloadNV),
            "HitAttributeNV" => Ok(Self::HitAttributeNV),
            "HitAttributeKHR" => Ok(Self::HitAttributeNV),
            "IncomingRayPayloadNV" => Ok(Self::IncomingRayPayloadNV),
            "IncomingRayPayloadKHR" => Ok(Self::IncomingRayPayloadNV),
            "ShaderRecordBufferNV" => Ok(Self::ShaderRecordBufferNV),
            "ShaderRecordBufferKHR" => Ok(Self::ShaderRecordBufferNV),
            "PhysicalStorageBuffer" => Ok(Self::PhysicalStorageBuffer),
            "PhysicalStorageBufferEXT" => Ok(Self::PhysicalStorageBuffer),
            "HitObjectAttributeNV" => Ok(Self::HitObjectAttributeNV),
            "TaskPayloadWorkgroupEXT" => Ok(Self::TaskPayloadWorkgroupEXT),
            "CodeSectionINTEL" => Ok(Self::CodeSectionINTEL),
            "DeviceOnlyINTEL" => Ok(Self::DeviceOnlyINTEL),
            "HostOnlyINTEL" => Ok(Self::HostOnlyINTEL),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [Dim](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_dim_a_dim)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum Dim {
    Dim1D = 0u32,
    Dim2D = 1u32,
    Dim3D = 2u32,
    DimCube = 3u32,
    DimRect = 4u32,
    DimBuffer = 5u32,
    DimSubpassData = 6u32,
    DimTileImageDataEXT = 4173u32,
}
impl Dim {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=6u32 => unsafe { core::mem::transmute::<u32, Dim>(n) },
            4173u32 => unsafe { core::mem::transmute::<u32, Dim>(4173u32) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl Dim {}
impl core::str::FromStr for Dim {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Dim1D" => Ok(Self::Dim1D),
            "Dim2D" => Ok(Self::Dim2D),
            "Dim3D" => Ok(Self::Dim3D),
            "DimCube" => Ok(Self::DimCube),
            "DimRect" => Ok(Self::DimRect),
            "DimBuffer" => Ok(Self::DimBuffer),
            "DimSubpassData" => Ok(Self::DimSubpassData),
            "DimTileImageDataEXT" => Ok(Self::DimTileImageDataEXT),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [SamplerAddressingMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_sampler_addressing_mode_a_sampler_addressing_mode)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum SamplerAddressingMode {
    None = 0u32,
    ClampToEdge = 1u32,
    Clamp = 2u32,
    Repeat = 3u32,
    RepeatMirrored = 4u32,
}
impl SamplerAddressingMode {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=4u32 => unsafe { core::mem::transmute::<u32, SamplerAddressingMode>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl SamplerAddressingMode {}
impl core::str::FromStr for SamplerAddressingMode {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "None" => Ok(Self::None),
            "ClampToEdge" => Ok(Self::ClampToEdge),
            "Clamp" => Ok(Self::Clamp),
            "Repeat" => Ok(Self::Repeat),
            "RepeatMirrored" => Ok(Self::RepeatMirrored),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [SamplerFilterMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_sampler_filter_mode_a_sampler_filter_mode)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum SamplerFilterMode {
    Nearest = 0u32,
    Linear = 1u32,
}
impl SamplerFilterMode {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=1u32 => unsafe { core::mem::transmute::<u32, SamplerFilterMode>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl SamplerFilterMode {}
impl core::str::FromStr for SamplerFilterMode {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Nearest" => Ok(Self::Nearest),
            "Linear" => Ok(Self::Linear),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [ImageFormat](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_image_format_a_image_format)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
    R64ui = 40u32,
    R64i = 41u32,
}
impl ImageFormat {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=41u32 => unsafe { core::mem::transmute::<u32, ImageFormat>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl ImageFormat {}
impl core::str::FromStr for ImageFormat {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Unknown" => Ok(Self::Unknown),
            "Rgba32f" => Ok(Self::Rgba32f),
            "Rgba16f" => Ok(Self::Rgba16f),
            "R32f" => Ok(Self::R32f),
            "Rgba8" => Ok(Self::Rgba8),
            "Rgba8Snorm" => Ok(Self::Rgba8Snorm),
            "Rg32f" => Ok(Self::Rg32f),
            "Rg16f" => Ok(Self::Rg16f),
            "R11fG11fB10f" => Ok(Self::R11fG11fB10f),
            "R16f" => Ok(Self::R16f),
            "Rgba16" => Ok(Self::Rgba16),
            "Rgb10A2" => Ok(Self::Rgb10A2),
            "Rg16" => Ok(Self::Rg16),
            "Rg8" => Ok(Self::Rg8),
            "R16" => Ok(Self::R16),
            "R8" => Ok(Self::R8),
            "Rgba16Snorm" => Ok(Self::Rgba16Snorm),
            "Rg16Snorm" => Ok(Self::Rg16Snorm),
            "Rg8Snorm" => Ok(Self::Rg8Snorm),
            "R16Snorm" => Ok(Self::R16Snorm),
            "R8Snorm" => Ok(Self::R8Snorm),
            "Rgba32i" => Ok(Self::Rgba32i),
            "Rgba16i" => Ok(Self::Rgba16i),
            "Rgba8i" => Ok(Self::Rgba8i),
            "R32i" => Ok(Self::R32i),
            "Rg32i" => Ok(Self::Rg32i),
            "Rg16i" => Ok(Self::Rg16i),
            "Rg8i" => Ok(Self::Rg8i),
            "R16i" => Ok(Self::R16i),
            "R8i" => Ok(Self::R8i),
            "Rgba32ui" => Ok(Self::Rgba32ui),
            "Rgba16ui" => Ok(Self::Rgba16ui),
            "Rgba8ui" => Ok(Self::Rgba8ui),
            "R32ui" => Ok(Self::R32ui),
            "Rgb10a2ui" => Ok(Self::Rgb10a2ui),
            "Rg32ui" => Ok(Self::Rg32ui),
            "Rg16ui" => Ok(Self::Rg16ui),
            "Rg8ui" => Ok(Self::Rg8ui),
            "R16ui" => Ok(Self::R16ui),
            "R8ui" => Ok(Self::R8ui),
            "R64ui" => Ok(Self::R64ui),
            "R64i" => Ok(Self::R64i),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [ImageChannelOrder](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_image_channel_order_a_image_channel_order)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
impl ImageChannelOrder {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=19u32 => unsafe { core::mem::transmute::<u32, ImageChannelOrder>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl ImageChannelOrder {}
impl core::str::FromStr for ImageChannelOrder {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "R" => Ok(Self::R),
            "A" => Ok(Self::A),
            "RG" => Ok(Self::RG),
            "RA" => Ok(Self::RA),
            "RGB" => Ok(Self::RGB),
            "RGBA" => Ok(Self::RGBA),
            "BGRA" => Ok(Self::BGRA),
            "ARGB" => Ok(Self::ARGB),
            "Intensity" => Ok(Self::Intensity),
            "Luminance" => Ok(Self::Luminance),
            "Rx" => Ok(Self::Rx),
            "RGx" => Ok(Self::RGx),
            "RGBx" => Ok(Self::RGBx),
            "Depth" => Ok(Self::Depth),
            "DepthStencil" => Ok(Self::DepthStencil),
            "sRGB" => Ok(Self::sRGB),
            "sRGBx" => Ok(Self::sRGBx),
            "sRGBA" => Ok(Self::sRGBA),
            "sBGRA" => Ok(Self::sBGRA),
            "ABGR" => Ok(Self::ABGR),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [ImageChannelDataType](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_image_channel_data_type_a_image_channel_data_type)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
    UnsignedIntRaw10EXT = 19u32,
    UnsignedIntRaw12EXT = 20u32,
}
impl ImageChannelDataType {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=16u32 => unsafe { core::mem::transmute::<u32, ImageChannelDataType>(n) },
            19u32..=20u32 => unsafe { core::mem::transmute::<u32, ImageChannelDataType>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl ImageChannelDataType {}
impl core::str::FromStr for ImageChannelDataType {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "SnormInt8" => Ok(Self::SnormInt8),
            "SnormInt16" => Ok(Self::SnormInt16),
            "UnormInt8" => Ok(Self::UnormInt8),
            "UnormInt16" => Ok(Self::UnormInt16),
            "UnormShort565" => Ok(Self::UnormShort565),
            "UnormShort555" => Ok(Self::UnormShort555),
            "UnormInt101010" => Ok(Self::UnormInt101010),
            "SignedInt8" => Ok(Self::SignedInt8),
            "SignedInt16" => Ok(Self::SignedInt16),
            "SignedInt32" => Ok(Self::SignedInt32),
            "UnsignedInt8" => Ok(Self::UnsignedInt8),
            "UnsignedInt16" => Ok(Self::UnsignedInt16),
            "UnsignedInt32" => Ok(Self::UnsignedInt32),
            "HalfFloat" => Ok(Self::HalfFloat),
            "Float" => Ok(Self::Float),
            "UnormInt24" => Ok(Self::UnormInt24),
            "UnormInt101010_2" => Ok(Self::UnormInt101010_2),
            "UnsignedIntRaw10EXT" => Ok(Self::UnsignedIntRaw10EXT),
            "UnsignedIntRaw12EXT" => Ok(Self::UnsignedIntRaw12EXT),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [FPRoundingMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_fp_rounding_mode_a_fp_rounding_mode)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum FPRoundingMode {
    RTE = 0u32,
    RTZ = 1u32,
    RTP = 2u32,
    RTN = 3u32,
}
impl FPRoundingMode {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=3u32 => unsafe { core::mem::transmute::<u32, FPRoundingMode>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl FPRoundingMode {}
impl core::str::FromStr for FPRoundingMode {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "RTE" => Ok(Self::RTE),
            "RTZ" => Ok(Self::RTZ),
            "RTP" => Ok(Self::RTP),
            "RTN" => Ok(Self::RTN),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [FPDenormMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_fp_denorm_mode_a_fp_denorm_mode)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum FPDenormMode {
    Preserve = 0u32,
    FlushToZero = 1u32,
}
impl FPDenormMode {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=1u32 => unsafe { core::mem::transmute::<u32, FPDenormMode>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl FPDenormMode {}
impl core::str::FromStr for FPDenormMode {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Preserve" => Ok(Self::Preserve),
            "FlushToZero" => Ok(Self::FlushToZero),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [QuantizationModes](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_quantization_modes_a_quantization_modes)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum QuantizationModes {
    TRN = 0u32,
    TRN_ZERO = 1u32,
    RND = 2u32,
    RND_ZERO = 3u32,
    RND_INF = 4u32,
    RND_MIN_INF = 5u32,
    RND_CONV = 6u32,
    RND_CONV_ODD = 7u32,
}
impl QuantizationModes {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=7u32 => unsafe { core::mem::transmute::<u32, QuantizationModes>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl QuantizationModes {}
impl core::str::FromStr for QuantizationModes {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "TRN" => Ok(Self::TRN),
            "TRN_ZERO" => Ok(Self::TRN_ZERO),
            "RND" => Ok(Self::RND),
            "RND_ZERO" => Ok(Self::RND_ZERO),
            "RND_INF" => Ok(Self::RND_INF),
            "RND_MIN_INF" => Ok(Self::RND_MIN_INF),
            "RND_CONV" => Ok(Self::RND_CONV),
            "RND_CONV_ODD" => Ok(Self::RND_CONV_ODD),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [FPOperationMode](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_fp_operation_mode_a_fp_operation_mode)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum FPOperationMode {
    IEEE = 0u32,
    ALT = 1u32,
}
impl FPOperationMode {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=1u32 => unsafe { core::mem::transmute::<u32, FPOperationMode>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl FPOperationMode {}
impl core::str::FromStr for FPOperationMode {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "IEEE" => Ok(Self::IEEE),
            "ALT" => Ok(Self::ALT),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [OverflowModes](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_overflow_modes_a_overflow_modes)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum OverflowModes {
    WRAP = 0u32,
    SAT = 1u32,
    SAT_ZERO = 2u32,
    SAT_SYM = 3u32,
}
impl OverflowModes {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=3u32 => unsafe { core::mem::transmute::<u32, OverflowModes>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl OverflowModes {}
impl core::str::FromStr for OverflowModes {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "WRAP" => Ok(Self::WRAP),
            "SAT" => Ok(Self::SAT),
            "SAT_ZERO" => Ok(Self::SAT_ZERO),
            "SAT_SYM" => Ok(Self::SAT_SYM),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [LinkageType](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_linkage_type_a_linkage_type)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum LinkageType {
    Export = 0u32,
    Import = 1u32,
    LinkOnceODR = 2u32,
}
impl LinkageType {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=2u32 => unsafe { core::mem::transmute::<u32, LinkageType>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl LinkageType {}
impl core::str::FromStr for LinkageType {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Export" => Ok(Self::Export),
            "Import" => Ok(Self::Import),
            "LinkOnceODR" => Ok(Self::LinkOnceODR),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [AccessQualifier](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_access_qualifier_a_access_qualifier)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum AccessQualifier {
    ReadOnly = 0u32,
    WriteOnly = 1u32,
    ReadWrite = 2u32,
}
impl AccessQualifier {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=2u32 => unsafe { core::mem::transmute::<u32, AccessQualifier>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl AccessQualifier {}
impl core::str::FromStr for AccessQualifier {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "ReadOnly" => Ok(Self::ReadOnly),
            "WriteOnly" => Ok(Self::WriteOnly),
            "ReadWrite" => Ok(Self::ReadWrite),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [HostAccessQualifier](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_host_access_qualifier_a_host_access_qualifier)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum HostAccessQualifier {
    NoneINTEL = 0u32,
    ReadINTEL = 1u32,
    WriteINTEL = 2u32,
    ReadWriteINTEL = 3u32,
}
impl HostAccessQualifier {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=3u32 => unsafe { core::mem::transmute::<u32, HostAccessQualifier>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl HostAccessQualifier {}
impl core::str::FromStr for HostAccessQualifier {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "NoneINTEL" => Ok(Self::NoneINTEL),
            "ReadINTEL" => Ok(Self::ReadINTEL),
            "WriteINTEL" => Ok(Self::WriteINTEL),
            "ReadWriteINTEL" => Ok(Self::ReadWriteINTEL),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [FunctionParameterAttribute](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_function_parameter_attribute_a_function_parameter_attribute)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum FunctionParameterAttribute {
    Zext = 0u32,
    Sext = 1u32,
    ByVal = 2u32,
    Sret = 3u32,
    NoAlias = 4u32,
    NoCapture = 5u32,
    NoWrite = 6u32,
    NoReadWrite = 7u32,
    RuntimeAlignedINTEL = 5940u32,
}
impl FunctionParameterAttribute {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=7u32 => unsafe { core::mem::transmute::<u32, FunctionParameterAttribute>(n) },
            5940u32 => unsafe { core::mem::transmute::<u32, FunctionParameterAttribute>(5940u32) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl FunctionParameterAttribute {}
impl core::str::FromStr for FunctionParameterAttribute {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Zext" => Ok(Self::Zext),
            "Sext" => Ok(Self::Sext),
            "ByVal" => Ok(Self::ByVal),
            "Sret" => Ok(Self::Sret),
            "NoAlias" => Ok(Self::NoAlias),
            "NoCapture" => Ok(Self::NoCapture),
            "NoWrite" => Ok(Self::NoWrite),
            "NoReadWrite" => Ok(Self::NoReadWrite),
            "RuntimeAlignedINTEL" => Ok(Self::RuntimeAlignedINTEL),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [Decoration](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_decoration_a_decoration)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
    WeightTextureQCOM = 4487u32,
    BlockMatchTextureQCOM = 4488u32,
    ExplicitInterpAMD = 4999u32,
    NodeSharesPayloadLimitsWithAMDX = 5019u32,
    NodeMaxPayloadsAMDX = 5020u32,
    TrackFinishWritingAMDX = 5078u32,
    PayloadNodeNameAMDX = 5091u32,
    OverrideCoverageNV = 5248u32,
    PassthroughNV = 5250u32,
    ViewportRelativeNV = 5252u32,
    SecondaryViewportRelativeNV = 5256u32,
    PerPrimitiveNV = 5271u32,
    PerViewNV = 5272u32,
    PerTaskNV = 5273u32,
    PerVertexKHR = 5285u32,
    NonUniform = 5300u32,
    RestrictPointer = 5355u32,
    AliasedPointer = 5356u32,
    HitObjectShaderRecordBufferNV = 5386u32,
    BindlessSamplerNV = 5398u32,
    BindlessImageNV = 5399u32,
    BoundSamplerNV = 5400u32,
    BoundImageNV = 5401u32,
    SIMTCallINTEL = 5599u32,
    ReferencedIndirectlyINTEL = 5602u32,
    ClobberINTEL = 5607u32,
    SideEffectsINTEL = 5608u32,
    VectorComputeVariableINTEL = 5624u32,
    FuncParamIOKindINTEL = 5625u32,
    VectorComputeFunctionINTEL = 5626u32,
    StackCallINTEL = 5627u32,
    GlobalVariableOffsetINTEL = 5628u32,
    CounterBuffer = 5634u32,
    UserSemantic = 5635u32,
    UserTypeGOOGLE = 5636u32,
    FunctionRoundingModeINTEL = 5822u32,
    FunctionDenormModeINTEL = 5823u32,
    RegisterINTEL = 5825u32,
    MemoryINTEL = 5826u32,
    NumbanksINTEL = 5827u32,
    BankwidthINTEL = 5828u32,
    MaxPrivateCopiesINTEL = 5829u32,
    SinglepumpINTEL = 5830u32,
    DoublepumpINTEL = 5831u32,
    MaxReplicatesINTEL = 5832u32,
    SimpleDualPortINTEL = 5833u32,
    MergeINTEL = 5834u32,
    BankBitsINTEL = 5835u32,
    ForcePow2DepthINTEL = 5836u32,
    BurstCoalesceINTEL = 5899u32,
    CacheSizeINTEL = 5900u32,
    DontStaticallyCoalesceINTEL = 5901u32,
    PrefetchINTEL = 5902u32,
    StallEnableINTEL = 5905u32,
    FuseLoopsInFunctionINTEL = 5907u32,
    MathOpDSPModeINTEL = 5909u32,
    AliasScopeINTEL = 5914u32,
    NoAliasINTEL = 5915u32,
    InitiationIntervalINTEL = 5917u32,
    MaxConcurrencyINTEL = 5918u32,
    PipelineEnableINTEL = 5919u32,
    BufferLocationINTEL = 5921u32,
    IOPipeStorageINTEL = 5944u32,
    FunctionFloatingPointModeINTEL = 6080u32,
    SingleElementVectorINTEL = 6085u32,
    VectorComputeCallableFunctionINTEL = 6087u32,
    MediaBlockIOINTEL = 6140u32,
    InitModeINTEL = 6147u32,
    ImplementInRegisterMapINTEL = 6148u32,
    HostAccessINTEL = 6168u32,
    FPMaxErrorDecorationINTEL = 6170u32,
    LatencyControlLabelINTEL = 6172u32,
    LatencyControlConstraintINTEL = 6173u32,
    ConduitKernelArgumentINTEL = 6175u32,
    RegisterMapKernelArgumentINTEL = 6176u32,
    MMHostInterfaceAddressWidthINTEL = 6177u32,
    MMHostInterfaceDataWidthINTEL = 6178u32,
    MMHostInterfaceLatencyINTEL = 6179u32,
    MMHostInterfaceReadWriteModeINTEL = 6180u32,
    MMHostInterfaceMaxBurstINTEL = 6181u32,
    MMHostInterfaceWaitRequestINTEL = 6182u32,
    StableKernelArgumentINTEL = 6183u32,
    CacheControlLoadINTEL = 6442u32,
    CacheControlStoreINTEL = 6443u32,
}
impl Decoration {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=11u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            13u32..=47u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            4469u32..=4470u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            4487u32..=4488u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            4999u32 => unsafe { core::mem::transmute::<u32, Decoration>(4999u32) },
            5019u32..=5020u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5078u32 => unsafe { core::mem::transmute::<u32, Decoration>(5078u32) },
            5091u32 => unsafe { core::mem::transmute::<u32, Decoration>(5091u32) },
            5248u32 => unsafe { core::mem::transmute::<u32, Decoration>(5248u32) },
            5250u32 => unsafe { core::mem::transmute::<u32, Decoration>(5250u32) },
            5252u32 => unsafe { core::mem::transmute::<u32, Decoration>(5252u32) },
            5256u32 => unsafe { core::mem::transmute::<u32, Decoration>(5256u32) },
            5271u32..=5273u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5285u32 => unsafe { core::mem::transmute::<u32, Decoration>(5285u32) },
            5300u32 => unsafe { core::mem::transmute::<u32, Decoration>(5300u32) },
            5355u32..=5356u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5386u32 => unsafe { core::mem::transmute::<u32, Decoration>(5386u32) },
            5398u32..=5401u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5599u32 => unsafe { core::mem::transmute::<u32, Decoration>(5599u32) },
            5602u32 => unsafe { core::mem::transmute::<u32, Decoration>(5602u32) },
            5607u32..=5608u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5624u32..=5628u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5634u32..=5636u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5822u32..=5823u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5825u32..=5836u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5899u32..=5902u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5905u32 => unsafe { core::mem::transmute::<u32, Decoration>(5905u32) },
            5907u32 => unsafe { core::mem::transmute::<u32, Decoration>(5907u32) },
            5909u32 => unsafe { core::mem::transmute::<u32, Decoration>(5909u32) },
            5914u32..=5915u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5917u32..=5919u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            5921u32 => unsafe { core::mem::transmute::<u32, Decoration>(5921u32) },
            5944u32 => unsafe { core::mem::transmute::<u32, Decoration>(5944u32) },
            6080u32 => unsafe { core::mem::transmute::<u32, Decoration>(6080u32) },
            6085u32 => unsafe { core::mem::transmute::<u32, Decoration>(6085u32) },
            6087u32 => unsafe { core::mem::transmute::<u32, Decoration>(6087u32) },
            6140u32 => unsafe { core::mem::transmute::<u32, Decoration>(6140u32) },
            6147u32..=6148u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            6168u32 => unsafe { core::mem::transmute::<u32, Decoration>(6168u32) },
            6170u32 => unsafe { core::mem::transmute::<u32, Decoration>(6170u32) },
            6172u32..=6173u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            6175u32..=6183u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            6442u32..=6443u32 => unsafe { core::mem::transmute::<u32, Decoration>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl Decoration {
    pub const PerPrimitiveEXT: Self = Self::PerPrimitiveNV;
    pub const PerVertexNV: Self = Self::PerVertexKHR;
    pub const NonUniformEXT: Self = Self::NonUniform;
    pub const RestrictPointerEXT: Self = Self::RestrictPointer;
    pub const AliasedPointerEXT: Self = Self::AliasedPointer;
    pub const HlslCounterBufferGOOGLE: Self = Self::CounterBuffer;
    pub const HlslSemanticGOOGLE: Self = Self::UserSemantic;
}
impl core::str::FromStr for Decoration {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "RelaxedPrecision" => Ok(Self::RelaxedPrecision),
            "SpecId" => Ok(Self::SpecId),
            "Block" => Ok(Self::Block),
            "BufferBlock" => Ok(Self::BufferBlock),
            "RowMajor" => Ok(Self::RowMajor),
            "ColMajor" => Ok(Self::ColMajor),
            "ArrayStride" => Ok(Self::ArrayStride),
            "MatrixStride" => Ok(Self::MatrixStride),
            "GLSLShared" => Ok(Self::GLSLShared),
            "GLSLPacked" => Ok(Self::GLSLPacked),
            "CPacked" => Ok(Self::CPacked),
            "BuiltIn" => Ok(Self::BuiltIn),
            "NoPerspective" => Ok(Self::NoPerspective),
            "Flat" => Ok(Self::Flat),
            "Patch" => Ok(Self::Patch),
            "Centroid" => Ok(Self::Centroid),
            "Sample" => Ok(Self::Sample),
            "Invariant" => Ok(Self::Invariant),
            "Restrict" => Ok(Self::Restrict),
            "Aliased" => Ok(Self::Aliased),
            "Volatile" => Ok(Self::Volatile),
            "Constant" => Ok(Self::Constant),
            "Coherent" => Ok(Self::Coherent),
            "NonWritable" => Ok(Self::NonWritable),
            "NonReadable" => Ok(Self::NonReadable),
            "Uniform" => Ok(Self::Uniform),
            "UniformId" => Ok(Self::UniformId),
            "SaturatedConversion" => Ok(Self::SaturatedConversion),
            "Stream" => Ok(Self::Stream),
            "Location" => Ok(Self::Location),
            "Component" => Ok(Self::Component),
            "Index" => Ok(Self::Index),
            "Binding" => Ok(Self::Binding),
            "DescriptorSet" => Ok(Self::DescriptorSet),
            "Offset" => Ok(Self::Offset),
            "XfbBuffer" => Ok(Self::XfbBuffer),
            "XfbStride" => Ok(Self::XfbStride),
            "FuncParamAttr" => Ok(Self::FuncParamAttr),
            "FPRoundingMode" => Ok(Self::FPRoundingMode),
            "FPFastMathMode" => Ok(Self::FPFastMathMode),
            "LinkageAttributes" => Ok(Self::LinkageAttributes),
            "NoContraction" => Ok(Self::NoContraction),
            "InputAttachmentIndex" => Ok(Self::InputAttachmentIndex),
            "Alignment" => Ok(Self::Alignment),
            "MaxByteOffset" => Ok(Self::MaxByteOffset),
            "AlignmentId" => Ok(Self::AlignmentId),
            "MaxByteOffsetId" => Ok(Self::MaxByteOffsetId),
            "NoSignedWrap" => Ok(Self::NoSignedWrap),
            "NoUnsignedWrap" => Ok(Self::NoUnsignedWrap),
            "WeightTextureQCOM" => Ok(Self::WeightTextureQCOM),
            "BlockMatchTextureQCOM" => Ok(Self::BlockMatchTextureQCOM),
            "ExplicitInterpAMD" => Ok(Self::ExplicitInterpAMD),
            "NodeSharesPayloadLimitsWithAMDX" => Ok(Self::NodeSharesPayloadLimitsWithAMDX),
            "NodeMaxPayloadsAMDX" => Ok(Self::NodeMaxPayloadsAMDX),
            "TrackFinishWritingAMDX" => Ok(Self::TrackFinishWritingAMDX),
            "PayloadNodeNameAMDX" => Ok(Self::PayloadNodeNameAMDX),
            "OverrideCoverageNV" => Ok(Self::OverrideCoverageNV),
            "PassthroughNV" => Ok(Self::PassthroughNV),
            "ViewportRelativeNV" => Ok(Self::ViewportRelativeNV),
            "SecondaryViewportRelativeNV" => Ok(Self::SecondaryViewportRelativeNV),
            "PerPrimitiveNV" => Ok(Self::PerPrimitiveNV),
            "PerPrimitiveEXT" => Ok(Self::PerPrimitiveNV),
            "PerViewNV" => Ok(Self::PerViewNV),
            "PerTaskNV" => Ok(Self::PerTaskNV),
            "PerVertexKHR" => Ok(Self::PerVertexKHR),
            "PerVertexNV" => Ok(Self::PerVertexKHR),
            "NonUniform" => Ok(Self::NonUniform),
            "NonUniformEXT" => Ok(Self::NonUniform),
            "RestrictPointer" => Ok(Self::RestrictPointer),
            "RestrictPointerEXT" => Ok(Self::RestrictPointer),
            "AliasedPointer" => Ok(Self::AliasedPointer),
            "AliasedPointerEXT" => Ok(Self::AliasedPointer),
            "HitObjectShaderRecordBufferNV" => Ok(Self::HitObjectShaderRecordBufferNV),
            "BindlessSamplerNV" => Ok(Self::BindlessSamplerNV),
            "BindlessImageNV" => Ok(Self::BindlessImageNV),
            "BoundSamplerNV" => Ok(Self::BoundSamplerNV),
            "BoundImageNV" => Ok(Self::BoundImageNV),
            "SIMTCallINTEL" => Ok(Self::SIMTCallINTEL),
            "ReferencedIndirectlyINTEL" => Ok(Self::ReferencedIndirectlyINTEL),
            "ClobberINTEL" => Ok(Self::ClobberINTEL),
            "SideEffectsINTEL" => Ok(Self::SideEffectsINTEL),
            "VectorComputeVariableINTEL" => Ok(Self::VectorComputeVariableINTEL),
            "FuncParamIOKindINTEL" => Ok(Self::FuncParamIOKindINTEL),
            "VectorComputeFunctionINTEL" => Ok(Self::VectorComputeFunctionINTEL),
            "StackCallINTEL" => Ok(Self::StackCallINTEL),
            "GlobalVariableOffsetINTEL" => Ok(Self::GlobalVariableOffsetINTEL),
            "CounterBuffer" => Ok(Self::CounterBuffer),
            "HlslCounterBufferGOOGLE" => Ok(Self::CounterBuffer),
            "UserSemantic" => Ok(Self::UserSemantic),
            "HlslSemanticGOOGLE" => Ok(Self::UserSemantic),
            "UserTypeGOOGLE" => Ok(Self::UserTypeGOOGLE),
            "FunctionRoundingModeINTEL" => Ok(Self::FunctionRoundingModeINTEL),
            "FunctionDenormModeINTEL" => Ok(Self::FunctionDenormModeINTEL),
            "RegisterINTEL" => Ok(Self::RegisterINTEL),
            "MemoryINTEL" => Ok(Self::MemoryINTEL),
            "NumbanksINTEL" => Ok(Self::NumbanksINTEL),
            "BankwidthINTEL" => Ok(Self::BankwidthINTEL),
            "MaxPrivateCopiesINTEL" => Ok(Self::MaxPrivateCopiesINTEL),
            "SinglepumpINTEL" => Ok(Self::SinglepumpINTEL),
            "DoublepumpINTEL" => Ok(Self::DoublepumpINTEL),
            "MaxReplicatesINTEL" => Ok(Self::MaxReplicatesINTEL),
            "SimpleDualPortINTEL" => Ok(Self::SimpleDualPortINTEL),
            "MergeINTEL" => Ok(Self::MergeINTEL),
            "BankBitsINTEL" => Ok(Self::BankBitsINTEL),
            "ForcePow2DepthINTEL" => Ok(Self::ForcePow2DepthINTEL),
            "BurstCoalesceINTEL" => Ok(Self::BurstCoalesceINTEL),
            "CacheSizeINTEL" => Ok(Self::CacheSizeINTEL),
            "DontStaticallyCoalesceINTEL" => Ok(Self::DontStaticallyCoalesceINTEL),
            "PrefetchINTEL" => Ok(Self::PrefetchINTEL),
            "StallEnableINTEL" => Ok(Self::StallEnableINTEL),
            "FuseLoopsInFunctionINTEL" => Ok(Self::FuseLoopsInFunctionINTEL),
            "MathOpDSPModeINTEL" => Ok(Self::MathOpDSPModeINTEL),
            "AliasScopeINTEL" => Ok(Self::AliasScopeINTEL),
            "NoAliasINTEL" => Ok(Self::NoAliasINTEL),
            "InitiationIntervalINTEL" => Ok(Self::InitiationIntervalINTEL),
            "MaxConcurrencyINTEL" => Ok(Self::MaxConcurrencyINTEL),
            "PipelineEnableINTEL" => Ok(Self::PipelineEnableINTEL),
            "BufferLocationINTEL" => Ok(Self::BufferLocationINTEL),
            "IOPipeStorageINTEL" => Ok(Self::IOPipeStorageINTEL),
            "FunctionFloatingPointModeINTEL" => Ok(Self::FunctionFloatingPointModeINTEL),
            "SingleElementVectorINTEL" => Ok(Self::SingleElementVectorINTEL),
            "VectorComputeCallableFunctionINTEL" => Ok(Self::VectorComputeCallableFunctionINTEL),
            "MediaBlockIOINTEL" => Ok(Self::MediaBlockIOINTEL),
            "InitModeINTEL" => Ok(Self::InitModeINTEL),
            "ImplementInRegisterMapINTEL" => Ok(Self::ImplementInRegisterMapINTEL),
            "HostAccessINTEL" => Ok(Self::HostAccessINTEL),
            "FPMaxErrorDecorationINTEL" => Ok(Self::FPMaxErrorDecorationINTEL),
            "LatencyControlLabelINTEL" => Ok(Self::LatencyControlLabelINTEL),
            "LatencyControlConstraintINTEL" => Ok(Self::LatencyControlConstraintINTEL),
            "ConduitKernelArgumentINTEL" => Ok(Self::ConduitKernelArgumentINTEL),
            "RegisterMapKernelArgumentINTEL" => Ok(Self::RegisterMapKernelArgumentINTEL),
            "MMHostInterfaceAddressWidthINTEL" => Ok(Self::MMHostInterfaceAddressWidthINTEL),
            "MMHostInterfaceDataWidthINTEL" => Ok(Self::MMHostInterfaceDataWidthINTEL),
            "MMHostInterfaceLatencyINTEL" => Ok(Self::MMHostInterfaceLatencyINTEL),
            "MMHostInterfaceReadWriteModeINTEL" => Ok(Self::MMHostInterfaceReadWriteModeINTEL),
            "MMHostInterfaceMaxBurstINTEL" => Ok(Self::MMHostInterfaceMaxBurstINTEL),
            "MMHostInterfaceWaitRequestINTEL" => Ok(Self::MMHostInterfaceWaitRequestINTEL),
            "StableKernelArgumentINTEL" => Ok(Self::StableKernelArgumentINTEL),
            "CacheControlLoadINTEL" => Ok(Self::CacheControlLoadINTEL),
            "CacheControlStoreINTEL" => Ok(Self::CacheControlStoreINTEL),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [BuiltIn](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_built_in_a_built_in)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
    CoreIDARM = 4160u32,
    CoreCountARM = 4161u32,
    CoreMaxIDARM = 4162u32,
    WarpIDARM = 4163u32,
    WarpMaxIDARM = 4164u32,
    SubgroupEqMask = 4416u32,
    SubgroupGeMask = 4417u32,
    SubgroupGtMask = 4418u32,
    SubgroupLeMask = 4419u32,
    SubgroupLtMask = 4420u32,
    BaseVertex = 4424u32,
    BaseInstance = 4425u32,
    DrawIndex = 4426u32,
    PrimitiveShadingRateKHR = 4432u32,
    DeviceIndex = 4438u32,
    ViewIndex = 4440u32,
    ShadingRateKHR = 4444u32,
    BaryCoordNoPerspAMD = 4992u32,
    BaryCoordNoPerspCentroidAMD = 4993u32,
    BaryCoordNoPerspSampleAMD = 4994u32,
    BaryCoordSmoothAMD = 4995u32,
    BaryCoordSmoothCentroidAMD = 4996u32,
    BaryCoordSmoothSampleAMD = 4997u32,
    BaryCoordPullModelAMD = 4998u32,
    FragStencilRefEXT = 5014u32,
    CoalescedInputCountAMDX = 5021u32,
    ShaderIndexAMDX = 5073u32,
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
    BaryCoordKHR = 5286u32,
    BaryCoordNoPerspKHR = 5287u32,
    FragSizeEXT = 5292u32,
    FragInvocationCountEXT = 5293u32,
    PrimitivePointIndicesEXT = 5294u32,
    PrimitiveLineIndicesEXT = 5295u32,
    PrimitiveTriangleIndicesEXT = 5296u32,
    CullPrimitiveEXT = 5299u32,
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
    CurrentRayTimeNV = 5334u32,
    HitTriangleVertexPositionsKHR = 5335u32,
    HitMicroTriangleVertexPositionsNV = 5337u32,
    HitMicroTriangleVertexBarycentricsNV = 5344u32,
    IncomingRayFlagsNV = 5351u32,
    RayGeometryIndexKHR = 5352u32,
    WarpsPerSMNV = 5374u32,
    SMCountNV = 5375u32,
    WarpIDNV = 5376u32,
    SMIDNV = 5377u32,
    HitKindFrontFacingMicroTriangleNV = 5405u32,
    HitKindBackFacingMicroTriangleNV = 5406u32,
    CullMaskKHR = 6021u32,
}
impl BuiltIn {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=1u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            3u32..=20u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            22u32..=34u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            36u32..=43u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            4160u32..=4164u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            4416u32..=4420u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            4424u32..=4426u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            4432u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(4432u32) },
            4438u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(4438u32) },
            4440u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(4440u32) },
            4444u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(4444u32) },
            4992u32..=4998u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            5014u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(5014u32) },
            5021u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(5021u32) },
            5073u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(5073u32) },
            5253u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(5253u32) },
            5257u32..=5258u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            5261u32..=5262u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            5264u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(5264u32) },
            5274u32..=5281u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            5286u32..=5287u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            5292u32..=5296u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            5299u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(5299u32) },
            5319u32..=5327u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            5330u32..=5335u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            5337u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(5337u32) },
            5344u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(5344u32) },
            5351u32..=5352u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            5374u32..=5377u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            5405u32..=5406u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(n) },
            6021u32 => unsafe { core::mem::transmute::<u32, BuiltIn>(6021u32) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl BuiltIn {
    pub const SubgroupEqMaskKHR: Self = Self::SubgroupEqMask;
    pub const SubgroupGeMaskKHR: Self = Self::SubgroupGeMask;
    pub const SubgroupGtMaskKHR: Self = Self::SubgroupGtMask;
    pub const SubgroupLeMaskKHR: Self = Self::SubgroupLeMask;
    pub const SubgroupLtMaskKHR: Self = Self::SubgroupLtMask;
    pub const BaryCoordNV: Self = Self::BaryCoordKHR;
    pub const BaryCoordNoPerspNV: Self = Self::BaryCoordNoPerspKHR;
    pub const FragmentSizeNV: Self = Self::FragSizeEXT;
    pub const InvocationsPerPixelNV: Self = Self::FragInvocationCountEXT;
    pub const LaunchIdKHR: Self = Self::LaunchIdNV;
    pub const LaunchSizeKHR: Self = Self::LaunchSizeNV;
    pub const WorldRayOriginKHR: Self = Self::WorldRayOriginNV;
    pub const WorldRayDirectionKHR: Self = Self::WorldRayDirectionNV;
    pub const ObjectRayOriginKHR: Self = Self::ObjectRayOriginNV;
    pub const ObjectRayDirectionKHR: Self = Self::ObjectRayDirectionNV;
    pub const RayTminKHR: Self = Self::RayTminNV;
    pub const RayTmaxKHR: Self = Self::RayTmaxNV;
    pub const InstanceCustomIndexKHR: Self = Self::InstanceCustomIndexNV;
    pub const ObjectToWorldKHR: Self = Self::ObjectToWorldNV;
    pub const WorldToObjectKHR: Self = Self::WorldToObjectNV;
    pub const HitKindKHR: Self = Self::HitKindNV;
    pub const IncomingRayFlagsKHR: Self = Self::IncomingRayFlagsNV;
}
impl core::str::FromStr for BuiltIn {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Position" => Ok(Self::Position),
            "PointSize" => Ok(Self::PointSize),
            "ClipDistance" => Ok(Self::ClipDistance),
            "CullDistance" => Ok(Self::CullDistance),
            "VertexId" => Ok(Self::VertexId),
            "InstanceId" => Ok(Self::InstanceId),
            "PrimitiveId" => Ok(Self::PrimitiveId),
            "InvocationId" => Ok(Self::InvocationId),
            "Layer" => Ok(Self::Layer),
            "ViewportIndex" => Ok(Self::ViewportIndex),
            "TessLevelOuter" => Ok(Self::TessLevelOuter),
            "TessLevelInner" => Ok(Self::TessLevelInner),
            "TessCoord" => Ok(Self::TessCoord),
            "PatchVertices" => Ok(Self::PatchVertices),
            "FragCoord" => Ok(Self::FragCoord),
            "PointCoord" => Ok(Self::PointCoord),
            "FrontFacing" => Ok(Self::FrontFacing),
            "SampleId" => Ok(Self::SampleId),
            "SamplePosition" => Ok(Self::SamplePosition),
            "SampleMask" => Ok(Self::SampleMask),
            "FragDepth" => Ok(Self::FragDepth),
            "HelperInvocation" => Ok(Self::HelperInvocation),
            "NumWorkgroups" => Ok(Self::NumWorkgroups),
            "WorkgroupSize" => Ok(Self::WorkgroupSize),
            "WorkgroupId" => Ok(Self::WorkgroupId),
            "LocalInvocationId" => Ok(Self::LocalInvocationId),
            "GlobalInvocationId" => Ok(Self::GlobalInvocationId),
            "LocalInvocationIndex" => Ok(Self::LocalInvocationIndex),
            "WorkDim" => Ok(Self::WorkDim),
            "GlobalSize" => Ok(Self::GlobalSize),
            "EnqueuedWorkgroupSize" => Ok(Self::EnqueuedWorkgroupSize),
            "GlobalOffset" => Ok(Self::GlobalOffset),
            "GlobalLinearId" => Ok(Self::GlobalLinearId),
            "SubgroupSize" => Ok(Self::SubgroupSize),
            "SubgroupMaxSize" => Ok(Self::SubgroupMaxSize),
            "NumSubgroups" => Ok(Self::NumSubgroups),
            "NumEnqueuedSubgroups" => Ok(Self::NumEnqueuedSubgroups),
            "SubgroupId" => Ok(Self::SubgroupId),
            "SubgroupLocalInvocationId" => Ok(Self::SubgroupLocalInvocationId),
            "VertexIndex" => Ok(Self::VertexIndex),
            "InstanceIndex" => Ok(Self::InstanceIndex),
            "CoreIDARM" => Ok(Self::CoreIDARM),
            "CoreCountARM" => Ok(Self::CoreCountARM),
            "CoreMaxIDARM" => Ok(Self::CoreMaxIDARM),
            "WarpIDARM" => Ok(Self::WarpIDARM),
            "WarpMaxIDARM" => Ok(Self::WarpMaxIDARM),
            "SubgroupEqMask" => Ok(Self::SubgroupEqMask),
            "SubgroupEqMaskKHR" => Ok(Self::SubgroupEqMask),
            "SubgroupGeMask" => Ok(Self::SubgroupGeMask),
            "SubgroupGeMaskKHR" => Ok(Self::SubgroupGeMask),
            "SubgroupGtMask" => Ok(Self::SubgroupGtMask),
            "SubgroupGtMaskKHR" => Ok(Self::SubgroupGtMask),
            "SubgroupLeMask" => Ok(Self::SubgroupLeMask),
            "SubgroupLeMaskKHR" => Ok(Self::SubgroupLeMask),
            "SubgroupLtMask" => Ok(Self::SubgroupLtMask),
            "SubgroupLtMaskKHR" => Ok(Self::SubgroupLtMask),
            "BaseVertex" => Ok(Self::BaseVertex),
            "BaseInstance" => Ok(Self::BaseInstance),
            "DrawIndex" => Ok(Self::DrawIndex),
            "PrimitiveShadingRateKHR" => Ok(Self::PrimitiveShadingRateKHR),
            "DeviceIndex" => Ok(Self::DeviceIndex),
            "ViewIndex" => Ok(Self::ViewIndex),
            "ShadingRateKHR" => Ok(Self::ShadingRateKHR),
            "BaryCoordNoPerspAMD" => Ok(Self::BaryCoordNoPerspAMD),
            "BaryCoordNoPerspCentroidAMD" => Ok(Self::BaryCoordNoPerspCentroidAMD),
            "BaryCoordNoPerspSampleAMD" => Ok(Self::BaryCoordNoPerspSampleAMD),
            "BaryCoordSmoothAMD" => Ok(Self::BaryCoordSmoothAMD),
            "BaryCoordSmoothCentroidAMD" => Ok(Self::BaryCoordSmoothCentroidAMD),
            "BaryCoordSmoothSampleAMD" => Ok(Self::BaryCoordSmoothSampleAMD),
            "BaryCoordPullModelAMD" => Ok(Self::BaryCoordPullModelAMD),
            "FragStencilRefEXT" => Ok(Self::FragStencilRefEXT),
            "CoalescedInputCountAMDX" => Ok(Self::CoalescedInputCountAMDX),
            "ShaderIndexAMDX" => Ok(Self::ShaderIndexAMDX),
            "ViewportMaskNV" => Ok(Self::ViewportMaskNV),
            "SecondaryPositionNV" => Ok(Self::SecondaryPositionNV),
            "SecondaryViewportMaskNV" => Ok(Self::SecondaryViewportMaskNV),
            "PositionPerViewNV" => Ok(Self::PositionPerViewNV),
            "ViewportMaskPerViewNV" => Ok(Self::ViewportMaskPerViewNV),
            "FullyCoveredEXT" => Ok(Self::FullyCoveredEXT),
            "TaskCountNV" => Ok(Self::TaskCountNV),
            "PrimitiveCountNV" => Ok(Self::PrimitiveCountNV),
            "PrimitiveIndicesNV" => Ok(Self::PrimitiveIndicesNV),
            "ClipDistancePerViewNV" => Ok(Self::ClipDistancePerViewNV),
            "CullDistancePerViewNV" => Ok(Self::CullDistancePerViewNV),
            "LayerPerViewNV" => Ok(Self::LayerPerViewNV),
            "MeshViewCountNV" => Ok(Self::MeshViewCountNV),
            "MeshViewIndicesNV" => Ok(Self::MeshViewIndicesNV),
            "BaryCoordKHR" => Ok(Self::BaryCoordKHR),
            "BaryCoordNV" => Ok(Self::BaryCoordKHR),
            "BaryCoordNoPerspKHR" => Ok(Self::BaryCoordNoPerspKHR),
            "BaryCoordNoPerspNV" => Ok(Self::BaryCoordNoPerspKHR),
            "FragSizeEXT" => Ok(Self::FragSizeEXT),
            "FragmentSizeNV" => Ok(Self::FragSizeEXT),
            "FragInvocationCountEXT" => Ok(Self::FragInvocationCountEXT),
            "InvocationsPerPixelNV" => Ok(Self::FragInvocationCountEXT),
            "PrimitivePointIndicesEXT" => Ok(Self::PrimitivePointIndicesEXT),
            "PrimitiveLineIndicesEXT" => Ok(Self::PrimitiveLineIndicesEXT),
            "PrimitiveTriangleIndicesEXT" => Ok(Self::PrimitiveTriangleIndicesEXT),
            "CullPrimitiveEXT" => Ok(Self::CullPrimitiveEXT),
            "LaunchIdNV" => Ok(Self::LaunchIdNV),
            "LaunchIdKHR" => Ok(Self::LaunchIdNV),
            "LaunchSizeNV" => Ok(Self::LaunchSizeNV),
            "LaunchSizeKHR" => Ok(Self::LaunchSizeNV),
            "WorldRayOriginNV" => Ok(Self::WorldRayOriginNV),
            "WorldRayOriginKHR" => Ok(Self::WorldRayOriginNV),
            "WorldRayDirectionNV" => Ok(Self::WorldRayDirectionNV),
            "WorldRayDirectionKHR" => Ok(Self::WorldRayDirectionNV),
            "ObjectRayOriginNV" => Ok(Self::ObjectRayOriginNV),
            "ObjectRayOriginKHR" => Ok(Self::ObjectRayOriginNV),
            "ObjectRayDirectionNV" => Ok(Self::ObjectRayDirectionNV),
            "ObjectRayDirectionKHR" => Ok(Self::ObjectRayDirectionNV),
            "RayTminNV" => Ok(Self::RayTminNV),
            "RayTminKHR" => Ok(Self::RayTminNV),
            "RayTmaxNV" => Ok(Self::RayTmaxNV),
            "RayTmaxKHR" => Ok(Self::RayTmaxNV),
            "InstanceCustomIndexNV" => Ok(Self::InstanceCustomIndexNV),
            "InstanceCustomIndexKHR" => Ok(Self::InstanceCustomIndexNV),
            "ObjectToWorldNV" => Ok(Self::ObjectToWorldNV),
            "ObjectToWorldKHR" => Ok(Self::ObjectToWorldNV),
            "WorldToObjectNV" => Ok(Self::WorldToObjectNV),
            "WorldToObjectKHR" => Ok(Self::WorldToObjectNV),
            "HitTNV" => Ok(Self::HitTNV),
            "HitKindNV" => Ok(Self::HitKindNV),
            "HitKindKHR" => Ok(Self::HitKindNV),
            "CurrentRayTimeNV" => Ok(Self::CurrentRayTimeNV),
            "HitTriangleVertexPositionsKHR" => Ok(Self::HitTriangleVertexPositionsKHR),
            "HitMicroTriangleVertexPositionsNV" => Ok(Self::HitMicroTriangleVertexPositionsNV),
            "HitMicroTriangleVertexBarycentricsNV" => {
                Ok(Self::HitMicroTriangleVertexBarycentricsNV)
            }
            "IncomingRayFlagsNV" => Ok(Self::IncomingRayFlagsNV),
            "IncomingRayFlagsKHR" => Ok(Self::IncomingRayFlagsNV),
            "RayGeometryIndexKHR" => Ok(Self::RayGeometryIndexKHR),
            "WarpsPerSMNV" => Ok(Self::WarpsPerSMNV),
            "SMCountNV" => Ok(Self::SMCountNV),
            "WarpIDNV" => Ok(Self::WarpIDNV),
            "SMIDNV" => Ok(Self::SMIDNV),
            "HitKindFrontFacingMicroTriangleNV" => Ok(Self::HitKindFrontFacingMicroTriangleNV),
            "HitKindBackFacingMicroTriangleNV" => Ok(Self::HitKindBackFacingMicroTriangleNV),
            "CullMaskKHR" => Ok(Self::CullMaskKHR),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [Scope](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_scope_a_scope)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum Scope {
    CrossDevice = 0u32,
    Device = 1u32,
    Workgroup = 2u32,
    Subgroup = 3u32,
    Invocation = 4u32,
    QueueFamily = 5u32,
    ShaderCallKHR = 6u32,
}
impl Scope {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=6u32 => unsafe { core::mem::transmute::<u32, Scope>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl Scope {
    pub const QueueFamilyKHR: Self = Self::QueueFamily;
}
impl core::str::FromStr for Scope {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "CrossDevice" => Ok(Self::CrossDevice),
            "Device" => Ok(Self::Device),
            "Workgroup" => Ok(Self::Workgroup),
            "Subgroup" => Ok(Self::Subgroup),
            "Invocation" => Ok(Self::Invocation),
            "QueueFamily" => Ok(Self::QueueFamily),
            "QueueFamilyKHR" => Ok(Self::QueueFamily),
            "ShaderCallKHR" => Ok(Self::ShaderCallKHR),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [GroupOperation](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_group_operation_a_group_operation)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum GroupOperation {
    Reduce = 0u32,
    InclusiveScan = 1u32,
    ExclusiveScan = 2u32,
    ClusteredReduce = 3u32,
    PartitionedReduceNV = 6u32,
    PartitionedInclusiveScanNV = 7u32,
    PartitionedExclusiveScanNV = 8u32,
}
impl GroupOperation {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=3u32 => unsafe { core::mem::transmute::<u32, GroupOperation>(n) },
            6u32..=8u32 => unsafe { core::mem::transmute::<u32, GroupOperation>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl GroupOperation {}
impl core::str::FromStr for GroupOperation {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Reduce" => Ok(Self::Reduce),
            "InclusiveScan" => Ok(Self::InclusiveScan),
            "ExclusiveScan" => Ok(Self::ExclusiveScan),
            "ClusteredReduce" => Ok(Self::ClusteredReduce),
            "PartitionedReduceNV" => Ok(Self::PartitionedReduceNV),
            "PartitionedInclusiveScanNV" => Ok(Self::PartitionedInclusiveScanNV),
            "PartitionedExclusiveScanNV" => Ok(Self::PartitionedExclusiveScanNV),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [KernelEnqueueFlags](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_kernel_enqueue_flags_a_kernel_enqueue_flags)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum KernelEnqueueFlags {
    NoWait = 0u32,
    WaitKernel = 1u32,
    WaitWorkGroup = 2u32,
}
impl KernelEnqueueFlags {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=2u32 => unsafe { core::mem::transmute::<u32, KernelEnqueueFlags>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl KernelEnqueueFlags {}
impl core::str::FromStr for KernelEnqueueFlags {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "NoWait" => Ok(Self::NoWait),
            "WaitKernel" => Ok(Self::WaitKernel),
            "WaitWorkGroup" => Ok(Self::WaitWorkGroup),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [Capability](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_capability_a_capability)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
    UniformDecoration = 71u32,
    CoreBuiltinsARM = 4165u32,
    TileImageColorReadAccessEXT = 4166u32,
    TileImageDepthReadAccessEXT = 4167u32,
    TileImageStencilReadAccessEXT = 4168u32,
    FragmentShadingRateKHR = 4422u32,
    SubgroupBallotKHR = 4423u32,
    DrawParameters = 4427u32,
    WorkgroupMemoryExplicitLayoutKHR = 4428u32,
    WorkgroupMemoryExplicitLayout8BitAccessKHR = 4429u32,
    WorkgroupMemoryExplicitLayout16BitAccessKHR = 4430u32,
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
    RayQueryKHR = 4472u32,
    RayTraversalPrimitiveCullingKHR = 4478u32,
    RayTracingKHR = 4479u32,
    TextureSampleWeightedQCOM = 4484u32,
    TextureBoxFilterQCOM = 4485u32,
    TextureBlockMatchQCOM = 4486u32,
    Float16ImageAMD = 5008u32,
    ImageGatherBiasLodAMD = 5009u32,
    FragmentMaskAMD = 5010u32,
    StencilExportEXT = 5013u32,
    ImageReadWriteLodAMD = 5015u32,
    Int64ImageEXT = 5016u32,
    ShaderClockKHR = 5055u32,
    ShaderEnqueueAMDX = 5067u32,
    SampleMaskOverrideCoverageNV = 5249u32,
    GeometryShaderPassthroughNV = 5251u32,
    ShaderViewportIndexLayerEXT = 5254u32,
    ShaderViewportMaskNV = 5255u32,
    ShaderStereoViewNV = 5259u32,
    PerViewAttributesNV = 5260u32,
    FragmentFullyCoveredEXT = 5265u32,
    MeshShadingNV = 5266u32,
    ImageFootprintNV = 5282u32,
    MeshShadingEXT = 5283u32,
    FragmentBarycentricKHR = 5284u32,
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
    RayTracingPositionFetchKHR = 5336u32,
    RayTracingNV = 5340u32,
    RayTracingMotionBlurNV = 5341u32,
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
    DemoteToHelperInvocation = 5379u32,
    DisplacementMicromapNV = 5380u32,
    RayTracingOpacityMicromapEXT = 5381u32,
    ShaderInvocationReorderNV = 5383u32,
    BindlessTextureNV = 5390u32,
    RayQueryPositionFetchKHR = 5391u32,
    RayTracingDisplacementMicromapNV = 5409u32,
    SubgroupShuffleINTEL = 5568u32,
    SubgroupBufferBlockIOINTEL = 5569u32,
    SubgroupImageBlockIOINTEL = 5570u32,
    SubgroupImageMediaBlockIOINTEL = 5579u32,
    RoundToInfinityINTEL = 5582u32,
    FloatingPointModeINTEL = 5583u32,
    IntegerFunctions2INTEL = 5584u32,
    FunctionPointersINTEL = 5603u32,
    IndirectReferencesINTEL = 5604u32,
    AsmINTEL = 5606u32,
    AtomicFloat32MinMaxEXT = 5612u32,
    AtomicFloat64MinMaxEXT = 5613u32,
    AtomicFloat16MinMaxEXT = 5616u32,
    VectorComputeINTEL = 5617u32,
    VectorAnyINTEL = 5619u32,
    ExpectAssumeKHR = 5629u32,
    SubgroupAvcMotionEstimationINTEL = 5696u32,
    SubgroupAvcMotionEstimationIntraINTEL = 5697u32,
    SubgroupAvcMotionEstimationChromaINTEL = 5698u32,
    VariableLengthArrayINTEL = 5817u32,
    FunctionFloatControlINTEL = 5821u32,
    FPGAMemoryAttributesINTEL = 5824u32,
    FPFastMathModeINTEL = 5837u32,
    ArbitraryPrecisionIntegersINTEL = 5844u32,
    ArbitraryPrecisionFloatingPointINTEL = 5845u32,
    UnstructuredLoopControlsINTEL = 5886u32,
    FPGALoopControlsINTEL = 5888u32,
    KernelAttributesINTEL = 5892u32,
    FPGAKernelAttributesINTEL = 5897u32,
    FPGAMemoryAccessesINTEL = 5898u32,
    FPGAClusterAttributesINTEL = 5904u32,
    LoopFuseINTEL = 5906u32,
    FPGADSPControlINTEL = 5908u32,
    MemoryAccessAliasingINTEL = 5910u32,
    FPGAInvocationPipeliningAttributesINTEL = 5916u32,
    FPGABufferLocationINTEL = 5920u32,
    ArbitraryPrecisionFixedPointINTEL = 5922u32,
    USMStorageClassesINTEL = 5935u32,
    RuntimeAlignedAttributeINTEL = 5939u32,
    IOPipesINTEL = 5943u32,
    BlockingPipesINTEL = 5945u32,
    FPGARegINTEL = 5948u32,
    DotProductInputAll = 6016u32,
    DotProductInput4x8Bit = 6017u32,
    DotProductInput4x8BitPacked = 6018u32,
    DotProduct = 6019u32,
    RayCullMaskKHR = 6020u32,
    CooperativeMatrixKHR = 6022u32,
    BitInstructions = 6025u32,
    GroupNonUniformRotateKHR = 6026u32,
    AtomicFloat32AddEXT = 6033u32,
    AtomicFloat64AddEXT = 6034u32,
    LongConstantCompositeINTEL = 6089u32,
    OptNoneINTEL = 6094u32,
    AtomicFloat16AddEXT = 6095u32,
    DebugInfoModuleINTEL = 6114u32,
    BFloat16ConversionINTEL = 6115u32,
    SplitBarrierINTEL = 6141u32,
    GlobalVariableFPGADecorationsINTEL = 6146u32,
    FPGAKernelAttributesv2INTEL = 6161u32,
    GlobalVariableHostAccessINTEL = 6167u32,
    FPMaxErrorINTEL = 6169u32,
    FPGALatencyControlINTEL = 6171u32,
    FPGAArgumentInterfacesINTEL = 6174u32,
    GroupUniformArithmeticKHR = 6400u32,
    CacheControlsINTEL = 6441u32,
}
impl Capability {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=15u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            17u32..=25u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            27u32..=71u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            4165u32..=4168u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            4422u32..=4423u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            4427u32..=4431u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            4433u32..=4437u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            4439u32 => unsafe { core::mem::transmute::<u32, Capability>(4439u32) },
            4441u32..=4442u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            4445u32 => unsafe { core::mem::transmute::<u32, Capability>(4445u32) },
            4447u32..=4450u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            4464u32..=4468u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            4471u32..=4472u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            4478u32..=4479u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            4484u32..=4486u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5008u32..=5010u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5013u32 => unsafe { core::mem::transmute::<u32, Capability>(5013u32) },
            5015u32..=5016u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5055u32 => unsafe { core::mem::transmute::<u32, Capability>(5055u32) },
            5067u32 => unsafe { core::mem::transmute::<u32, Capability>(5067u32) },
            5249u32 => unsafe { core::mem::transmute::<u32, Capability>(5249u32) },
            5251u32 => unsafe { core::mem::transmute::<u32, Capability>(5251u32) },
            5254u32..=5255u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5259u32..=5260u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5265u32..=5266u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5282u32..=5284u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5288u32 => unsafe { core::mem::transmute::<u32, Capability>(5288u32) },
            5291u32 => unsafe { core::mem::transmute::<u32, Capability>(5291u32) },
            5297u32 => unsafe { core::mem::transmute::<u32, Capability>(5297u32) },
            5301u32..=5312u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5336u32 => unsafe { core::mem::transmute::<u32, Capability>(5336u32) },
            5340u32..=5341u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5345u32..=5347u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5350u32 => unsafe { core::mem::transmute::<u32, Capability>(5350u32) },
            5353u32 => unsafe { core::mem::transmute::<u32, Capability>(5353u32) },
            5357u32 => unsafe { core::mem::transmute::<u32, Capability>(5357u32) },
            5363u32 => unsafe { core::mem::transmute::<u32, Capability>(5363u32) },
            5372u32..=5373u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5378u32..=5381u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5383u32 => unsafe { core::mem::transmute::<u32, Capability>(5383u32) },
            5390u32..=5391u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5409u32 => unsafe { core::mem::transmute::<u32, Capability>(5409u32) },
            5568u32..=5570u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5579u32 => unsafe { core::mem::transmute::<u32, Capability>(5579u32) },
            5582u32..=5584u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5603u32..=5604u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5606u32 => unsafe { core::mem::transmute::<u32, Capability>(5606u32) },
            5612u32..=5613u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5616u32..=5617u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5619u32 => unsafe { core::mem::transmute::<u32, Capability>(5619u32) },
            5629u32 => unsafe { core::mem::transmute::<u32, Capability>(5629u32) },
            5696u32..=5698u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5817u32 => unsafe { core::mem::transmute::<u32, Capability>(5817u32) },
            5821u32 => unsafe { core::mem::transmute::<u32, Capability>(5821u32) },
            5824u32 => unsafe { core::mem::transmute::<u32, Capability>(5824u32) },
            5837u32 => unsafe { core::mem::transmute::<u32, Capability>(5837u32) },
            5844u32..=5845u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5886u32 => unsafe { core::mem::transmute::<u32, Capability>(5886u32) },
            5888u32 => unsafe { core::mem::transmute::<u32, Capability>(5888u32) },
            5892u32 => unsafe { core::mem::transmute::<u32, Capability>(5892u32) },
            5897u32..=5898u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            5904u32 => unsafe { core::mem::transmute::<u32, Capability>(5904u32) },
            5906u32 => unsafe { core::mem::transmute::<u32, Capability>(5906u32) },
            5908u32 => unsafe { core::mem::transmute::<u32, Capability>(5908u32) },
            5910u32 => unsafe { core::mem::transmute::<u32, Capability>(5910u32) },
            5916u32 => unsafe { core::mem::transmute::<u32, Capability>(5916u32) },
            5920u32 => unsafe { core::mem::transmute::<u32, Capability>(5920u32) },
            5922u32 => unsafe { core::mem::transmute::<u32, Capability>(5922u32) },
            5935u32 => unsafe { core::mem::transmute::<u32, Capability>(5935u32) },
            5939u32 => unsafe { core::mem::transmute::<u32, Capability>(5939u32) },
            5943u32 => unsafe { core::mem::transmute::<u32, Capability>(5943u32) },
            5945u32 => unsafe { core::mem::transmute::<u32, Capability>(5945u32) },
            5948u32 => unsafe { core::mem::transmute::<u32, Capability>(5948u32) },
            6016u32..=6020u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            6022u32 => unsafe { core::mem::transmute::<u32, Capability>(6022u32) },
            6025u32..=6026u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            6033u32..=6034u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            6089u32 => unsafe { core::mem::transmute::<u32, Capability>(6089u32) },
            6094u32..=6095u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            6114u32..=6115u32 => unsafe { core::mem::transmute::<u32, Capability>(n) },
            6141u32 => unsafe { core::mem::transmute::<u32, Capability>(6141u32) },
            6146u32 => unsafe { core::mem::transmute::<u32, Capability>(6146u32) },
            6161u32 => unsafe { core::mem::transmute::<u32, Capability>(6161u32) },
            6167u32 => unsafe { core::mem::transmute::<u32, Capability>(6167u32) },
            6169u32 => unsafe { core::mem::transmute::<u32, Capability>(6169u32) },
            6171u32 => unsafe { core::mem::transmute::<u32, Capability>(6171u32) },
            6174u32 => unsafe { core::mem::transmute::<u32, Capability>(6174u32) },
            6400u32 => unsafe { core::mem::transmute::<u32, Capability>(6400u32) },
            6441u32 => unsafe { core::mem::transmute::<u32, Capability>(6441u32) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl Capability {
    pub const StorageUniformBufferBlock16: Self = Self::StorageBuffer16BitAccess;
    pub const StorageUniform16: Self = Self::UniformAndStorageBuffer16BitAccess;
    pub const ShaderViewportIndexLayerNV: Self = Self::ShaderViewportIndexLayerEXT;
    pub const FragmentBarycentricNV: Self = Self::FragmentBarycentricKHR;
    pub const ShadingRateNV: Self = Self::FragmentDensityEXT;
    pub const ShaderNonUniformEXT: Self = Self::ShaderNonUniform;
    pub const RuntimeDescriptorArrayEXT: Self = Self::RuntimeDescriptorArray;
    pub const InputAttachmentArrayDynamicIndexingEXT: Self =
        Self::InputAttachmentArrayDynamicIndexing;
    pub const UniformTexelBufferArrayDynamicIndexingEXT: Self =
        Self::UniformTexelBufferArrayDynamicIndexing;
    pub const StorageTexelBufferArrayDynamicIndexingEXT: Self =
        Self::StorageTexelBufferArrayDynamicIndexing;
    pub const UniformBufferArrayNonUniformIndexingEXT: Self =
        Self::UniformBufferArrayNonUniformIndexing;
    pub const SampledImageArrayNonUniformIndexingEXT: Self =
        Self::SampledImageArrayNonUniformIndexing;
    pub const StorageBufferArrayNonUniformIndexingEXT: Self =
        Self::StorageBufferArrayNonUniformIndexing;
    pub const StorageImageArrayNonUniformIndexingEXT: Self =
        Self::StorageImageArrayNonUniformIndexing;
    pub const InputAttachmentArrayNonUniformIndexingEXT: Self =
        Self::InputAttachmentArrayNonUniformIndexing;
    pub const UniformTexelBufferArrayNonUniformIndexingEXT: Self =
        Self::UniformTexelBufferArrayNonUniformIndexing;
    pub const StorageTexelBufferArrayNonUniformIndexingEXT: Self =
        Self::StorageTexelBufferArrayNonUniformIndexing;
    pub const VulkanMemoryModelKHR: Self = Self::VulkanMemoryModel;
    pub const VulkanMemoryModelDeviceScopeKHR: Self = Self::VulkanMemoryModelDeviceScope;
    pub const PhysicalStorageBufferAddressesEXT: Self = Self::PhysicalStorageBufferAddresses;
    pub const DemoteToHelperInvocationEXT: Self = Self::DemoteToHelperInvocation;
    pub const DotProductInputAllKHR: Self = Self::DotProductInputAll;
    pub const DotProductInput4x8BitKHR: Self = Self::DotProductInput4x8Bit;
    pub const DotProductInput4x8BitPackedKHR: Self = Self::DotProductInput4x8BitPacked;
    pub const DotProductKHR: Self = Self::DotProduct;
}
impl core::str::FromStr for Capability {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "Matrix" => Ok(Self::Matrix),
            "Shader" => Ok(Self::Shader),
            "Geometry" => Ok(Self::Geometry),
            "Tessellation" => Ok(Self::Tessellation),
            "Addresses" => Ok(Self::Addresses),
            "Linkage" => Ok(Self::Linkage),
            "Kernel" => Ok(Self::Kernel),
            "Vector16" => Ok(Self::Vector16),
            "Float16Buffer" => Ok(Self::Float16Buffer),
            "Float16" => Ok(Self::Float16),
            "Float64" => Ok(Self::Float64),
            "Int64" => Ok(Self::Int64),
            "Int64Atomics" => Ok(Self::Int64Atomics),
            "ImageBasic" => Ok(Self::ImageBasic),
            "ImageReadWrite" => Ok(Self::ImageReadWrite),
            "ImageMipmap" => Ok(Self::ImageMipmap),
            "Pipes" => Ok(Self::Pipes),
            "Groups" => Ok(Self::Groups),
            "DeviceEnqueue" => Ok(Self::DeviceEnqueue),
            "LiteralSampler" => Ok(Self::LiteralSampler),
            "AtomicStorage" => Ok(Self::AtomicStorage),
            "Int16" => Ok(Self::Int16),
            "TessellationPointSize" => Ok(Self::TessellationPointSize),
            "GeometryPointSize" => Ok(Self::GeometryPointSize),
            "ImageGatherExtended" => Ok(Self::ImageGatherExtended),
            "StorageImageMultisample" => Ok(Self::StorageImageMultisample),
            "UniformBufferArrayDynamicIndexing" => Ok(Self::UniformBufferArrayDynamicIndexing),
            "SampledImageArrayDynamicIndexing" => Ok(Self::SampledImageArrayDynamicIndexing),
            "StorageBufferArrayDynamicIndexing" => Ok(Self::StorageBufferArrayDynamicIndexing),
            "StorageImageArrayDynamicIndexing" => Ok(Self::StorageImageArrayDynamicIndexing),
            "ClipDistance" => Ok(Self::ClipDistance),
            "CullDistance" => Ok(Self::CullDistance),
            "ImageCubeArray" => Ok(Self::ImageCubeArray),
            "SampleRateShading" => Ok(Self::SampleRateShading),
            "ImageRect" => Ok(Self::ImageRect),
            "SampledRect" => Ok(Self::SampledRect),
            "GenericPointer" => Ok(Self::GenericPointer),
            "Int8" => Ok(Self::Int8),
            "InputAttachment" => Ok(Self::InputAttachment),
            "SparseResidency" => Ok(Self::SparseResidency),
            "MinLod" => Ok(Self::MinLod),
            "Sampled1D" => Ok(Self::Sampled1D),
            "Image1D" => Ok(Self::Image1D),
            "SampledCubeArray" => Ok(Self::SampledCubeArray),
            "SampledBuffer" => Ok(Self::SampledBuffer),
            "ImageBuffer" => Ok(Self::ImageBuffer),
            "ImageMSArray" => Ok(Self::ImageMSArray),
            "StorageImageExtendedFormats" => Ok(Self::StorageImageExtendedFormats),
            "ImageQuery" => Ok(Self::ImageQuery),
            "DerivativeControl" => Ok(Self::DerivativeControl),
            "InterpolationFunction" => Ok(Self::InterpolationFunction),
            "TransformFeedback" => Ok(Self::TransformFeedback),
            "GeometryStreams" => Ok(Self::GeometryStreams),
            "StorageImageReadWithoutFormat" => Ok(Self::StorageImageReadWithoutFormat),
            "StorageImageWriteWithoutFormat" => Ok(Self::StorageImageWriteWithoutFormat),
            "MultiViewport" => Ok(Self::MultiViewport),
            "SubgroupDispatch" => Ok(Self::SubgroupDispatch),
            "NamedBarrier" => Ok(Self::NamedBarrier),
            "PipeStorage" => Ok(Self::PipeStorage),
            "GroupNonUniform" => Ok(Self::GroupNonUniform),
            "GroupNonUniformVote" => Ok(Self::GroupNonUniformVote),
            "GroupNonUniformArithmetic" => Ok(Self::GroupNonUniformArithmetic),
            "GroupNonUniformBallot" => Ok(Self::GroupNonUniformBallot),
            "GroupNonUniformShuffle" => Ok(Self::GroupNonUniformShuffle),
            "GroupNonUniformShuffleRelative" => Ok(Self::GroupNonUniformShuffleRelative),
            "GroupNonUniformClustered" => Ok(Self::GroupNonUniformClustered),
            "GroupNonUniformQuad" => Ok(Self::GroupNonUniformQuad),
            "ShaderLayer" => Ok(Self::ShaderLayer),
            "ShaderViewportIndex" => Ok(Self::ShaderViewportIndex),
            "UniformDecoration" => Ok(Self::UniformDecoration),
            "CoreBuiltinsARM" => Ok(Self::CoreBuiltinsARM),
            "TileImageColorReadAccessEXT" => Ok(Self::TileImageColorReadAccessEXT),
            "TileImageDepthReadAccessEXT" => Ok(Self::TileImageDepthReadAccessEXT),
            "TileImageStencilReadAccessEXT" => Ok(Self::TileImageStencilReadAccessEXT),
            "FragmentShadingRateKHR" => Ok(Self::FragmentShadingRateKHR),
            "SubgroupBallotKHR" => Ok(Self::SubgroupBallotKHR),
            "DrawParameters" => Ok(Self::DrawParameters),
            "WorkgroupMemoryExplicitLayoutKHR" => Ok(Self::WorkgroupMemoryExplicitLayoutKHR),
            "WorkgroupMemoryExplicitLayout8BitAccessKHR" => {
                Ok(Self::WorkgroupMemoryExplicitLayout8BitAccessKHR)
            }
            "WorkgroupMemoryExplicitLayout16BitAccessKHR" => {
                Ok(Self::WorkgroupMemoryExplicitLayout16BitAccessKHR)
            }
            "SubgroupVoteKHR" => Ok(Self::SubgroupVoteKHR),
            "StorageBuffer16BitAccess" => Ok(Self::StorageBuffer16BitAccess),
            "StorageUniformBufferBlock16" => Ok(Self::StorageBuffer16BitAccess),
            "UniformAndStorageBuffer16BitAccess" => Ok(Self::UniformAndStorageBuffer16BitAccess),
            "StorageUniform16" => Ok(Self::UniformAndStorageBuffer16BitAccess),
            "StoragePushConstant16" => Ok(Self::StoragePushConstant16),
            "StorageInputOutput16" => Ok(Self::StorageInputOutput16),
            "DeviceGroup" => Ok(Self::DeviceGroup),
            "MultiView" => Ok(Self::MultiView),
            "VariablePointersStorageBuffer" => Ok(Self::VariablePointersStorageBuffer),
            "VariablePointers" => Ok(Self::VariablePointers),
            "AtomicStorageOps" => Ok(Self::AtomicStorageOps),
            "SampleMaskPostDepthCoverage" => Ok(Self::SampleMaskPostDepthCoverage),
            "StorageBuffer8BitAccess" => Ok(Self::StorageBuffer8BitAccess),
            "UniformAndStorageBuffer8BitAccess" => Ok(Self::UniformAndStorageBuffer8BitAccess),
            "StoragePushConstant8" => Ok(Self::StoragePushConstant8),
            "DenormPreserve" => Ok(Self::DenormPreserve),
            "DenormFlushToZero" => Ok(Self::DenormFlushToZero),
            "SignedZeroInfNanPreserve" => Ok(Self::SignedZeroInfNanPreserve),
            "RoundingModeRTE" => Ok(Self::RoundingModeRTE),
            "RoundingModeRTZ" => Ok(Self::RoundingModeRTZ),
            "RayQueryProvisionalKHR" => Ok(Self::RayQueryProvisionalKHR),
            "RayQueryKHR" => Ok(Self::RayQueryKHR),
            "RayTraversalPrimitiveCullingKHR" => Ok(Self::RayTraversalPrimitiveCullingKHR),
            "RayTracingKHR" => Ok(Self::RayTracingKHR),
            "TextureSampleWeightedQCOM" => Ok(Self::TextureSampleWeightedQCOM),
            "TextureBoxFilterQCOM" => Ok(Self::TextureBoxFilterQCOM),
            "TextureBlockMatchQCOM" => Ok(Self::TextureBlockMatchQCOM),
            "Float16ImageAMD" => Ok(Self::Float16ImageAMD),
            "ImageGatherBiasLodAMD" => Ok(Self::ImageGatherBiasLodAMD),
            "FragmentMaskAMD" => Ok(Self::FragmentMaskAMD),
            "StencilExportEXT" => Ok(Self::StencilExportEXT),
            "ImageReadWriteLodAMD" => Ok(Self::ImageReadWriteLodAMD),
            "Int64ImageEXT" => Ok(Self::Int64ImageEXT),
            "ShaderClockKHR" => Ok(Self::ShaderClockKHR),
            "ShaderEnqueueAMDX" => Ok(Self::ShaderEnqueueAMDX),
            "SampleMaskOverrideCoverageNV" => Ok(Self::SampleMaskOverrideCoverageNV),
            "GeometryShaderPassthroughNV" => Ok(Self::GeometryShaderPassthroughNV),
            "ShaderViewportIndexLayerEXT" => Ok(Self::ShaderViewportIndexLayerEXT),
            "ShaderViewportIndexLayerNV" => Ok(Self::ShaderViewportIndexLayerEXT),
            "ShaderViewportMaskNV" => Ok(Self::ShaderViewportMaskNV),
            "ShaderStereoViewNV" => Ok(Self::ShaderStereoViewNV),
            "PerViewAttributesNV" => Ok(Self::PerViewAttributesNV),
            "FragmentFullyCoveredEXT" => Ok(Self::FragmentFullyCoveredEXT),
            "MeshShadingNV" => Ok(Self::MeshShadingNV),
            "ImageFootprintNV" => Ok(Self::ImageFootprintNV),
            "MeshShadingEXT" => Ok(Self::MeshShadingEXT),
            "FragmentBarycentricKHR" => Ok(Self::FragmentBarycentricKHR),
            "FragmentBarycentricNV" => Ok(Self::FragmentBarycentricKHR),
            "ComputeDerivativeGroupQuadsNV" => Ok(Self::ComputeDerivativeGroupQuadsNV),
            "FragmentDensityEXT" => Ok(Self::FragmentDensityEXT),
            "ShadingRateNV" => Ok(Self::FragmentDensityEXT),
            "GroupNonUniformPartitionedNV" => Ok(Self::GroupNonUniformPartitionedNV),
            "ShaderNonUniform" => Ok(Self::ShaderNonUniform),
            "ShaderNonUniformEXT" => Ok(Self::ShaderNonUniform),
            "RuntimeDescriptorArray" => Ok(Self::RuntimeDescriptorArray),
            "RuntimeDescriptorArrayEXT" => Ok(Self::RuntimeDescriptorArray),
            "InputAttachmentArrayDynamicIndexing" => Ok(Self::InputAttachmentArrayDynamicIndexing),
            "InputAttachmentArrayDynamicIndexingEXT" => {
                Ok(Self::InputAttachmentArrayDynamicIndexing)
            }
            "UniformTexelBufferArrayDynamicIndexing" => {
                Ok(Self::UniformTexelBufferArrayDynamicIndexing)
            }
            "UniformTexelBufferArrayDynamicIndexingEXT" => {
                Ok(Self::UniformTexelBufferArrayDynamicIndexing)
            }
            "StorageTexelBufferArrayDynamicIndexing" => {
                Ok(Self::StorageTexelBufferArrayDynamicIndexing)
            }
            "StorageTexelBufferArrayDynamicIndexingEXT" => {
                Ok(Self::StorageTexelBufferArrayDynamicIndexing)
            }
            "UniformBufferArrayNonUniformIndexing" => {
                Ok(Self::UniformBufferArrayNonUniformIndexing)
            }
            "UniformBufferArrayNonUniformIndexingEXT" => {
                Ok(Self::UniformBufferArrayNonUniformIndexing)
            }
            "SampledImageArrayNonUniformIndexing" => Ok(Self::SampledImageArrayNonUniformIndexing),
            "SampledImageArrayNonUniformIndexingEXT" => {
                Ok(Self::SampledImageArrayNonUniformIndexing)
            }
            "StorageBufferArrayNonUniformIndexing" => {
                Ok(Self::StorageBufferArrayNonUniformIndexing)
            }
            "StorageBufferArrayNonUniformIndexingEXT" => {
                Ok(Self::StorageBufferArrayNonUniformIndexing)
            }
            "StorageImageArrayNonUniformIndexing" => Ok(Self::StorageImageArrayNonUniformIndexing),
            "StorageImageArrayNonUniformIndexingEXT" => {
                Ok(Self::StorageImageArrayNonUniformIndexing)
            }
            "InputAttachmentArrayNonUniformIndexing" => {
                Ok(Self::InputAttachmentArrayNonUniformIndexing)
            }
            "InputAttachmentArrayNonUniformIndexingEXT" => {
                Ok(Self::InputAttachmentArrayNonUniformIndexing)
            }
            "UniformTexelBufferArrayNonUniformIndexing" => {
                Ok(Self::UniformTexelBufferArrayNonUniformIndexing)
            }
            "UniformTexelBufferArrayNonUniformIndexingEXT" => {
                Ok(Self::UniformTexelBufferArrayNonUniformIndexing)
            }
            "StorageTexelBufferArrayNonUniformIndexing" => {
                Ok(Self::StorageTexelBufferArrayNonUniformIndexing)
            }
            "StorageTexelBufferArrayNonUniformIndexingEXT" => {
                Ok(Self::StorageTexelBufferArrayNonUniformIndexing)
            }
            "RayTracingPositionFetchKHR" => Ok(Self::RayTracingPositionFetchKHR),
            "RayTracingNV" => Ok(Self::RayTracingNV),
            "RayTracingMotionBlurNV" => Ok(Self::RayTracingMotionBlurNV),
            "VulkanMemoryModel" => Ok(Self::VulkanMemoryModel),
            "VulkanMemoryModelKHR" => Ok(Self::VulkanMemoryModel),
            "VulkanMemoryModelDeviceScope" => Ok(Self::VulkanMemoryModelDeviceScope),
            "VulkanMemoryModelDeviceScopeKHR" => Ok(Self::VulkanMemoryModelDeviceScope),
            "PhysicalStorageBufferAddresses" => Ok(Self::PhysicalStorageBufferAddresses),
            "PhysicalStorageBufferAddressesEXT" => Ok(Self::PhysicalStorageBufferAddresses),
            "ComputeDerivativeGroupLinearNV" => Ok(Self::ComputeDerivativeGroupLinearNV),
            "RayTracingProvisionalKHR" => Ok(Self::RayTracingProvisionalKHR),
            "CooperativeMatrixNV" => Ok(Self::CooperativeMatrixNV),
            "FragmentShaderSampleInterlockEXT" => Ok(Self::FragmentShaderSampleInterlockEXT),
            "FragmentShaderShadingRateInterlockEXT" => {
                Ok(Self::FragmentShaderShadingRateInterlockEXT)
            }
            "ShaderSMBuiltinsNV" => Ok(Self::ShaderSMBuiltinsNV),
            "FragmentShaderPixelInterlockEXT" => Ok(Self::FragmentShaderPixelInterlockEXT),
            "DemoteToHelperInvocation" => Ok(Self::DemoteToHelperInvocation),
            "DemoteToHelperInvocationEXT" => Ok(Self::DemoteToHelperInvocation),
            "DisplacementMicromapNV" => Ok(Self::DisplacementMicromapNV),
            "RayTracingOpacityMicromapEXT" => Ok(Self::RayTracingOpacityMicromapEXT),
            "ShaderInvocationReorderNV" => Ok(Self::ShaderInvocationReorderNV),
            "BindlessTextureNV" => Ok(Self::BindlessTextureNV),
            "RayQueryPositionFetchKHR" => Ok(Self::RayQueryPositionFetchKHR),
            "RayTracingDisplacementMicromapNV" => Ok(Self::RayTracingDisplacementMicromapNV),
            "SubgroupShuffleINTEL" => Ok(Self::SubgroupShuffleINTEL),
            "SubgroupBufferBlockIOINTEL" => Ok(Self::SubgroupBufferBlockIOINTEL),
            "SubgroupImageBlockIOINTEL" => Ok(Self::SubgroupImageBlockIOINTEL),
            "SubgroupImageMediaBlockIOINTEL" => Ok(Self::SubgroupImageMediaBlockIOINTEL),
            "RoundToInfinityINTEL" => Ok(Self::RoundToInfinityINTEL),
            "FloatingPointModeINTEL" => Ok(Self::FloatingPointModeINTEL),
            "IntegerFunctions2INTEL" => Ok(Self::IntegerFunctions2INTEL),
            "FunctionPointersINTEL" => Ok(Self::FunctionPointersINTEL),
            "IndirectReferencesINTEL" => Ok(Self::IndirectReferencesINTEL),
            "AsmINTEL" => Ok(Self::AsmINTEL),
            "AtomicFloat32MinMaxEXT" => Ok(Self::AtomicFloat32MinMaxEXT),
            "AtomicFloat64MinMaxEXT" => Ok(Self::AtomicFloat64MinMaxEXT),
            "AtomicFloat16MinMaxEXT" => Ok(Self::AtomicFloat16MinMaxEXT),
            "VectorComputeINTEL" => Ok(Self::VectorComputeINTEL),
            "VectorAnyINTEL" => Ok(Self::VectorAnyINTEL),
            "ExpectAssumeKHR" => Ok(Self::ExpectAssumeKHR),
            "SubgroupAvcMotionEstimationINTEL" => Ok(Self::SubgroupAvcMotionEstimationINTEL),
            "SubgroupAvcMotionEstimationIntraINTEL" => {
                Ok(Self::SubgroupAvcMotionEstimationIntraINTEL)
            }
            "SubgroupAvcMotionEstimationChromaINTEL" => {
                Ok(Self::SubgroupAvcMotionEstimationChromaINTEL)
            }
            "VariableLengthArrayINTEL" => Ok(Self::VariableLengthArrayINTEL),
            "FunctionFloatControlINTEL" => Ok(Self::FunctionFloatControlINTEL),
            "FPGAMemoryAttributesINTEL" => Ok(Self::FPGAMemoryAttributesINTEL),
            "FPFastMathModeINTEL" => Ok(Self::FPFastMathModeINTEL),
            "ArbitraryPrecisionIntegersINTEL" => Ok(Self::ArbitraryPrecisionIntegersINTEL),
            "ArbitraryPrecisionFloatingPointINTEL" => {
                Ok(Self::ArbitraryPrecisionFloatingPointINTEL)
            }
            "UnstructuredLoopControlsINTEL" => Ok(Self::UnstructuredLoopControlsINTEL),
            "FPGALoopControlsINTEL" => Ok(Self::FPGALoopControlsINTEL),
            "KernelAttributesINTEL" => Ok(Self::KernelAttributesINTEL),
            "FPGAKernelAttributesINTEL" => Ok(Self::FPGAKernelAttributesINTEL),
            "FPGAMemoryAccessesINTEL" => Ok(Self::FPGAMemoryAccessesINTEL),
            "FPGAClusterAttributesINTEL" => Ok(Self::FPGAClusterAttributesINTEL),
            "LoopFuseINTEL" => Ok(Self::LoopFuseINTEL),
            "FPGADSPControlINTEL" => Ok(Self::FPGADSPControlINTEL),
            "MemoryAccessAliasingINTEL" => Ok(Self::MemoryAccessAliasingINTEL),
            "FPGAInvocationPipeliningAttributesINTEL" => {
                Ok(Self::FPGAInvocationPipeliningAttributesINTEL)
            }
            "FPGABufferLocationINTEL" => Ok(Self::FPGABufferLocationINTEL),
            "ArbitraryPrecisionFixedPointINTEL" => Ok(Self::ArbitraryPrecisionFixedPointINTEL),
            "USMStorageClassesINTEL" => Ok(Self::USMStorageClassesINTEL),
            "RuntimeAlignedAttributeINTEL" => Ok(Self::RuntimeAlignedAttributeINTEL),
            "IOPipesINTEL" => Ok(Self::IOPipesINTEL),
            "BlockingPipesINTEL" => Ok(Self::BlockingPipesINTEL),
            "FPGARegINTEL" => Ok(Self::FPGARegINTEL),
            "DotProductInputAll" => Ok(Self::DotProductInputAll),
            "DotProductInputAllKHR" => Ok(Self::DotProductInputAll),
            "DotProductInput4x8Bit" => Ok(Self::DotProductInput4x8Bit),
            "DotProductInput4x8BitKHR" => Ok(Self::DotProductInput4x8Bit),
            "DotProductInput4x8BitPacked" => Ok(Self::DotProductInput4x8BitPacked),
            "DotProductInput4x8BitPackedKHR" => Ok(Self::DotProductInput4x8BitPacked),
            "DotProduct" => Ok(Self::DotProduct),
            "DotProductKHR" => Ok(Self::DotProduct),
            "RayCullMaskKHR" => Ok(Self::RayCullMaskKHR),
            "CooperativeMatrixKHR" => Ok(Self::CooperativeMatrixKHR),
            "BitInstructions" => Ok(Self::BitInstructions),
            "GroupNonUniformRotateKHR" => Ok(Self::GroupNonUniformRotateKHR),
            "AtomicFloat32AddEXT" => Ok(Self::AtomicFloat32AddEXT),
            "AtomicFloat64AddEXT" => Ok(Self::AtomicFloat64AddEXT),
            "LongConstantCompositeINTEL" => Ok(Self::LongConstantCompositeINTEL),
            "OptNoneINTEL" => Ok(Self::OptNoneINTEL),
            "AtomicFloat16AddEXT" => Ok(Self::AtomicFloat16AddEXT),
            "DebugInfoModuleINTEL" => Ok(Self::DebugInfoModuleINTEL),
            "BFloat16ConversionINTEL" => Ok(Self::BFloat16ConversionINTEL),
            "SplitBarrierINTEL" => Ok(Self::SplitBarrierINTEL),
            "GlobalVariableFPGADecorationsINTEL" => Ok(Self::GlobalVariableFPGADecorationsINTEL),
            "FPGAKernelAttributesv2INTEL" => Ok(Self::FPGAKernelAttributesv2INTEL),
            "GlobalVariableHostAccessINTEL" => Ok(Self::GlobalVariableHostAccessINTEL),
            "FPMaxErrorINTEL" => Ok(Self::FPMaxErrorINTEL),
            "FPGALatencyControlINTEL" => Ok(Self::FPGALatencyControlINTEL),
            "FPGAArgumentInterfacesINTEL" => Ok(Self::FPGAArgumentInterfacesINTEL),
            "GroupUniformArithmeticKHR" => Ok(Self::GroupUniformArithmeticKHR),
            "CacheControlsINTEL" => Ok(Self::CacheControlsINTEL),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [RayQueryIntersection](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_ray_query_intersection_a_ray_query_intersection)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum RayQueryIntersection {
    RayQueryCandidateIntersectionKHR = 0u32,
    RayQueryCommittedIntersectionKHR = 1u32,
}
impl RayQueryIntersection {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=1u32 => unsafe { core::mem::transmute::<u32, RayQueryIntersection>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl RayQueryIntersection {}
impl core::str::FromStr for RayQueryIntersection {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "RayQueryCandidateIntersectionKHR" => Ok(Self::RayQueryCandidateIntersectionKHR),
            "RayQueryCommittedIntersectionKHR" => Ok(Self::RayQueryCommittedIntersectionKHR),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [RayQueryCommittedIntersectionType](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_ray_query_committed_intersection_type_a_ray_query_committed_intersection_type)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum RayQueryCommittedIntersectionType {
    RayQueryCommittedIntersectionNoneKHR = 0u32,
    RayQueryCommittedIntersectionTriangleKHR = 1u32,
    RayQueryCommittedIntersectionGeneratedKHR = 2u32,
}
impl RayQueryCommittedIntersectionType {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=2u32 => unsafe {
                core::mem::transmute::<u32, RayQueryCommittedIntersectionType>(n)
            },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl RayQueryCommittedIntersectionType {}
impl core::str::FromStr for RayQueryCommittedIntersectionType {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "RayQueryCommittedIntersectionNoneKHR" => {
                Ok(Self::RayQueryCommittedIntersectionNoneKHR)
            }
            "RayQueryCommittedIntersectionTriangleKHR" => {
                Ok(Self::RayQueryCommittedIntersectionTriangleKHR)
            }
            "RayQueryCommittedIntersectionGeneratedKHR" => {
                Ok(Self::RayQueryCommittedIntersectionGeneratedKHR)
            }
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [RayQueryCandidateIntersectionType](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_ray_query_candidate_intersection_type_a_ray_query_candidate_intersection_type)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum RayQueryCandidateIntersectionType {
    RayQueryCandidateIntersectionTriangleKHR = 0u32,
    RayQueryCandidateIntersectionAABBKHR = 1u32,
}
impl RayQueryCandidateIntersectionType {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=1u32 => unsafe {
                core::mem::transmute::<u32, RayQueryCandidateIntersectionType>(n)
            },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl RayQueryCandidateIntersectionType {}
impl core::str::FromStr for RayQueryCandidateIntersectionType {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "RayQueryCandidateIntersectionTriangleKHR" => {
                Ok(Self::RayQueryCandidateIntersectionTriangleKHR)
            }
            "RayQueryCandidateIntersectionAABBKHR" => {
                Ok(Self::RayQueryCandidateIntersectionAABBKHR)
            }
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [PackedVectorFormat](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_packed_vector_format_a_packed_vector_format)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum PackedVectorFormat {
    PackedVectorFormat4x8Bit = 0u32,
}
impl PackedVectorFormat {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32 => unsafe { core::mem::transmute::<u32, PackedVectorFormat>(0u32) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl PackedVectorFormat {
    pub const PackedVectorFormat4x8BitKHR: Self = Self::PackedVectorFormat4x8Bit;
}
impl core::str::FromStr for PackedVectorFormat {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "PackedVectorFormat4x8Bit" => Ok(Self::PackedVectorFormat4x8Bit),
            "PackedVectorFormat4x8BitKHR" => Ok(Self::PackedVectorFormat4x8Bit),
            _ => Err(()),
        }
    }
}
bitflags! { # [doc = "SPIR-V operand kind: [CooperativeMatrixOperands](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_cooperative_matrix_operands_a_cooperative_matrix_operands)"] # [derive (Clone , Copy , Debug , PartialEq , Eq , Hash)] # [cfg_attr (feature = "serialize" , derive (serde :: Serialize))] # [cfg_attr (feature = "deserialize" , derive (serde :: Deserialize))] pub struct CooperativeMatrixOperands : u32 { const NONE_KHR = 0u32 ; const MATRIX_A_SIGNED_COMPONENTS_KHR = 1u32 ; const MATRIX_B_SIGNED_COMPONENTS_KHR = 2u32 ; const MATRIX_C_SIGNED_COMPONENTS_KHR = 4u32 ; const MATRIX_RESULT_SIGNED_COMPONENTS_KHR = 8u32 ; const SATURATING_ACCUMULATION_KHR = 16u32 ; } }
#[doc = "SPIR-V operand kind: [CooperativeMatrixLayout](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_cooperative_matrix_layout_a_cooperative_matrix_layout)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum CooperativeMatrixLayout {
    RowMajorKHR = 0u32,
    ColumnMajorKHR = 1u32,
}
impl CooperativeMatrixLayout {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=1u32 => unsafe { core::mem::transmute::<u32, CooperativeMatrixLayout>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl CooperativeMatrixLayout {}
impl core::str::FromStr for CooperativeMatrixLayout {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "RowMajorKHR" => Ok(Self::RowMajorKHR),
            "ColumnMajorKHR" => Ok(Self::ColumnMajorKHR),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [CooperativeMatrixUse](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_cooperative_matrix_use_a_cooperative_matrix_use)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum CooperativeMatrixUse {
    MatrixAKHR = 0u32,
    MatrixBKHR = 1u32,
    MatrixAccumulatorKHR = 2u32,
}
impl CooperativeMatrixUse {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=2u32 => unsafe { core::mem::transmute::<u32, CooperativeMatrixUse>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl CooperativeMatrixUse {}
impl core::str::FromStr for CooperativeMatrixUse {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "MatrixAKHR" => Ok(Self::MatrixAKHR),
            "MatrixBKHR" => Ok(Self::MatrixBKHR),
            "MatrixAccumulatorKHR" => Ok(Self::MatrixAccumulatorKHR),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [InitializationModeQualifier](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_initialization_mode_qualifier_a_initialization_mode_qualifier)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum InitializationModeQualifier {
    InitOnDeviceReprogramINTEL = 0u32,
    InitOnDeviceResetINTEL = 1u32,
}
impl InitializationModeQualifier {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=1u32 => unsafe { core::mem::transmute::<u32, InitializationModeQualifier>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl InitializationModeQualifier {}
impl core::str::FromStr for InitializationModeQualifier {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "InitOnDeviceReprogramINTEL" => Ok(Self::InitOnDeviceReprogramINTEL),
            "InitOnDeviceResetINTEL" => Ok(Self::InitOnDeviceResetINTEL),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [LoadCacheControl](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_load_cache_control_a_load_cache_control)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum LoadCacheControl {
    UncachedINTEL = 0u32,
    CachedINTEL = 1u32,
    StreamingINTEL = 2u32,
    InvalidateAfterReadINTEL = 3u32,
    ConstCachedINTEL = 4u32,
}
impl LoadCacheControl {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=4u32 => unsafe { core::mem::transmute::<u32, LoadCacheControl>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl LoadCacheControl {}
impl core::str::FromStr for LoadCacheControl {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "UncachedINTEL" => Ok(Self::UncachedINTEL),
            "CachedINTEL" => Ok(Self::CachedINTEL),
            "StreamingINTEL" => Ok(Self::StreamingINTEL),
            "InvalidateAfterReadINTEL" => Ok(Self::InvalidateAfterReadINTEL),
            "ConstCachedINTEL" => Ok(Self::ConstCachedINTEL),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V operand kind: [StoreCacheControl](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_store_cache_control_a_store_cache_control)"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
pub enum StoreCacheControl {
    UncachedINTEL = 0u32,
    WriteThroughINTEL = 1u32,
    WriteBackINTEL = 2u32,
    StreamingINTEL = 3u32,
}
impl StoreCacheControl {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=3u32 => unsafe { core::mem::transmute::<u32, StoreCacheControl>(n) },
            _ => return None,
        })
    }
}
#[allow(non_upper_case_globals)]
impl StoreCacheControl {}
impl core::str::FromStr for StoreCacheControl {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "UncachedINTEL" => Ok(Self::UncachedINTEL),
            "WriteThroughINTEL" => Ok(Self::WriteThroughINTEL),
            "WriteBackINTEL" => Ok(Self::WriteBackINTEL),
            "StreamingINTEL" => Ok(Self::StreamingINTEL),
            _ => Err(()),
        }
    }
}
#[doc = "SPIR-V [instructions](https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#_a_id_instructions_a_instructions) opcodes"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
    ColorAttachmentReadEXT = 4160u32,
    DepthAttachmentReadEXT = 4161u32,
    StencilAttachmentReadEXT = 4162u32,
    TerminateInvocation = 4416u32,
    SubgroupBallotKHR = 4421u32,
    SubgroupFirstInvocationKHR = 4422u32,
    SubgroupAllKHR = 4428u32,
    SubgroupAnyKHR = 4429u32,
    SubgroupAllEqualKHR = 4430u32,
    GroupNonUniformRotateKHR = 4431u32,
    SubgroupReadInvocationKHR = 4432u32,
    TraceRayKHR = 4445u32,
    ExecuteCallableKHR = 4446u32,
    ConvertUToAccelerationStructureKHR = 4447u32,
    IgnoreIntersectionKHR = 4448u32,
    TerminateRayKHR = 4449u32,
    SDot = 4450u32,
    UDot = 4451u32,
    SUDot = 4452u32,
    SDotAccSat = 4453u32,
    UDotAccSat = 4454u32,
    SUDotAccSat = 4455u32,
    TypeCooperativeMatrixKHR = 4456u32,
    CooperativeMatrixLoadKHR = 4457u32,
    CooperativeMatrixStoreKHR = 4458u32,
    CooperativeMatrixMulAddKHR = 4459u32,
    CooperativeMatrixLengthKHR = 4460u32,
    TypeRayQueryKHR = 4472u32,
    RayQueryInitializeKHR = 4473u32,
    RayQueryTerminateKHR = 4474u32,
    RayQueryGenerateIntersectionKHR = 4475u32,
    RayQueryConfirmIntersectionKHR = 4476u32,
    RayQueryProceedKHR = 4477u32,
    RayQueryGetIntersectionTypeKHR = 4479u32,
    ImageSampleWeightedQCOM = 4480u32,
    ImageBoxFilterQCOM = 4481u32,
    ImageBlockMatchSSDQCOM = 4482u32,
    ImageBlockMatchSADQCOM = 4483u32,
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
    FinalizeNodePayloadsAMDX = 5075u32,
    FinishWritingNodePayloadAMDX = 5078u32,
    InitializeNodePayloadsAMDX = 5090u32,
    HitObjectRecordHitMotionNV = 5249u32,
    HitObjectRecordHitWithIndexMotionNV = 5250u32,
    HitObjectRecordMissMotionNV = 5251u32,
    HitObjectGetWorldToObjectNV = 5252u32,
    HitObjectGetObjectToWorldNV = 5253u32,
    HitObjectGetObjectRayDirectionNV = 5254u32,
    HitObjectGetObjectRayOriginNV = 5255u32,
    HitObjectTraceRayMotionNV = 5256u32,
    HitObjectGetShaderRecordBufferHandleNV = 5257u32,
    HitObjectGetShaderBindingTableRecordIndexNV = 5258u32,
    HitObjectRecordEmptyNV = 5259u32,
    HitObjectTraceRayNV = 5260u32,
    HitObjectRecordHitNV = 5261u32,
    HitObjectRecordHitWithIndexNV = 5262u32,
    HitObjectRecordMissNV = 5263u32,
    HitObjectExecuteShaderNV = 5264u32,
    HitObjectGetCurrentTimeNV = 5265u32,
    HitObjectGetAttributesNV = 5266u32,
    HitObjectGetHitKindNV = 5267u32,
    HitObjectGetPrimitiveIndexNV = 5268u32,
    HitObjectGetGeometryIndexNV = 5269u32,
    HitObjectGetInstanceIdNV = 5270u32,
    HitObjectGetInstanceCustomIndexNV = 5271u32,
    HitObjectGetWorldRayDirectionNV = 5272u32,
    HitObjectGetWorldRayOriginNV = 5273u32,
    HitObjectGetRayTMaxNV = 5274u32,
    HitObjectGetRayTMinNV = 5275u32,
    HitObjectIsEmptyNV = 5276u32,
    HitObjectIsHitNV = 5277u32,
    HitObjectIsMissNV = 5278u32,
    ReorderThreadWithHitObjectNV = 5279u32,
    ReorderThreadWithHintNV = 5280u32,
    TypeHitObjectNV = 5281u32,
    ImageSampleFootprintNV = 5283u32,
    EmitMeshTasksEXT = 5294u32,
    SetMeshOutputsEXT = 5295u32,
    GroupNonUniformPartitionNV = 5296u32,
    WritePackedPrimitiveIndices4x8NV = 5299u32,
    FetchMicroTriangleVertexPositionNV = 5300u32,
    FetchMicroTriangleVertexBarycentricNV = 5301u32,
    ReportIntersectionKHR = 5334u32,
    IgnoreIntersectionNV = 5335u32,
    TerminateRayNV = 5336u32,
    TraceNV = 5337u32,
    TraceMotionNV = 5338u32,
    TraceRayMotionNV = 5339u32,
    RayQueryGetIntersectionTriangleVertexPositionsKHR = 5340u32,
    TypeAccelerationStructureKHR = 5341u32,
    ExecuteCallableNV = 5344u32,
    TypeCooperativeMatrixNV = 5358u32,
    CooperativeMatrixLoadNV = 5359u32,
    CooperativeMatrixStoreNV = 5360u32,
    CooperativeMatrixMulAddNV = 5361u32,
    CooperativeMatrixLengthNV = 5362u32,
    BeginInvocationInterlockEXT = 5364u32,
    EndInvocationInterlockEXT = 5365u32,
    DemoteToHelperInvocation = 5380u32,
    IsHelperInvocationEXT = 5381u32,
    ConvertUToImageNV = 5391u32,
    ConvertUToSamplerNV = 5392u32,
    ConvertImageToUNV = 5393u32,
    ConvertSamplerToUNV = 5394u32,
    ConvertUToSampledImageNV = 5395u32,
    ConvertSampledImageToUNV = 5396u32,
    SamplerImageAddressingModeNV = 5397u32,
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
    ConstantFunctionPointerINTEL = 5600u32,
    FunctionPointerCallINTEL = 5601u32,
    AsmTargetINTEL = 5609u32,
    AsmINTEL = 5610u32,
    AsmCallINTEL = 5611u32,
    AtomicFMinEXT = 5614u32,
    AtomicFMaxEXT = 5615u32,
    AssumeTrueKHR = 5630u32,
    ExpectKHR = 5631u32,
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
    VariableLengthArrayINTEL = 5818u32,
    SaveMemoryINTEL = 5819u32,
    RestoreMemoryINTEL = 5820u32,
    ArbitraryFloatSinCosPiINTEL = 5840u32,
    ArbitraryFloatCastINTEL = 5841u32,
    ArbitraryFloatCastFromIntINTEL = 5842u32,
    ArbitraryFloatCastToIntINTEL = 5843u32,
    ArbitraryFloatAddINTEL = 5846u32,
    ArbitraryFloatSubINTEL = 5847u32,
    ArbitraryFloatMulINTEL = 5848u32,
    ArbitraryFloatDivINTEL = 5849u32,
    ArbitraryFloatGTINTEL = 5850u32,
    ArbitraryFloatGEINTEL = 5851u32,
    ArbitraryFloatLTINTEL = 5852u32,
    ArbitraryFloatLEINTEL = 5853u32,
    ArbitraryFloatEQINTEL = 5854u32,
    ArbitraryFloatRecipINTEL = 5855u32,
    ArbitraryFloatRSqrtINTEL = 5856u32,
    ArbitraryFloatCbrtINTEL = 5857u32,
    ArbitraryFloatHypotINTEL = 5858u32,
    ArbitraryFloatSqrtINTEL = 5859u32,
    ArbitraryFloatLogINTEL = 5860u32,
    ArbitraryFloatLog2INTEL = 5861u32,
    ArbitraryFloatLog10INTEL = 5862u32,
    ArbitraryFloatLog1pINTEL = 5863u32,
    ArbitraryFloatExpINTEL = 5864u32,
    ArbitraryFloatExp2INTEL = 5865u32,
    ArbitraryFloatExp10INTEL = 5866u32,
    ArbitraryFloatExpm1INTEL = 5867u32,
    ArbitraryFloatSinINTEL = 5868u32,
    ArbitraryFloatCosINTEL = 5869u32,
    ArbitraryFloatSinCosINTEL = 5870u32,
    ArbitraryFloatSinPiINTEL = 5871u32,
    ArbitraryFloatCosPiINTEL = 5872u32,
    ArbitraryFloatASinINTEL = 5873u32,
    ArbitraryFloatASinPiINTEL = 5874u32,
    ArbitraryFloatACosINTEL = 5875u32,
    ArbitraryFloatACosPiINTEL = 5876u32,
    ArbitraryFloatATanINTEL = 5877u32,
    ArbitraryFloatATanPiINTEL = 5878u32,
    ArbitraryFloatATan2INTEL = 5879u32,
    ArbitraryFloatPowINTEL = 5880u32,
    ArbitraryFloatPowRINTEL = 5881u32,
    ArbitraryFloatPowNINTEL = 5882u32,
    LoopControlINTEL = 5887u32,
    AliasDomainDeclINTEL = 5911u32,
    AliasScopeDeclINTEL = 5912u32,
    AliasScopeListDeclINTEL = 5913u32,
    FixedSqrtINTEL = 5923u32,
    FixedRecipINTEL = 5924u32,
    FixedRsqrtINTEL = 5925u32,
    FixedSinINTEL = 5926u32,
    FixedCosINTEL = 5927u32,
    FixedSinCosINTEL = 5928u32,
    FixedSinPiINTEL = 5929u32,
    FixedCosPiINTEL = 5930u32,
    FixedSinCosPiINTEL = 5931u32,
    FixedLogINTEL = 5932u32,
    FixedExpINTEL = 5933u32,
    PtrCastToCrossWorkgroupINTEL = 5934u32,
    CrossWorkgroupCastToPtrINTEL = 5938u32,
    ReadPipeBlockingINTEL = 5946u32,
    WritePipeBlockingINTEL = 5947u32,
    FPGARegINTEL = 5949u32,
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
    AtomicFAddEXT = 6035u32,
    TypeBufferSurfaceINTEL = 6086u32,
    TypeStructContinuedINTEL = 6090u32,
    ConstantCompositeContinuedINTEL = 6091u32,
    SpecConstantCompositeContinuedINTEL = 6092u32,
    ConvertFToBF16INTEL = 6116u32,
    ConvertBF16ToFINTEL = 6117u32,
    ControlBarrierArriveINTEL = 6142u32,
    ControlBarrierWaitINTEL = 6143u32,
    GroupIMulKHR = 6401u32,
    GroupFMulKHR = 6402u32,
    GroupBitwiseAndKHR = 6403u32,
    GroupBitwiseOrKHR = 6404u32,
    GroupBitwiseXorKHR = 6405u32,
    GroupLogicalAndKHR = 6406u32,
    GroupLogicalOrKHR = 6407u32,
    GroupLogicalXorKHR = 6408u32,
}
impl Op {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=8u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            10u32..=12u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            14u32..=17u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            19u32..=39u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            41u32..=46u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            48u32..=52u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            54u32..=57u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            59u32..=75u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            77u32..=84u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            86u32..=107u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            109u32..=124u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            126u32..=152u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            154u32..=191u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            194u32..=205u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            207u32..=215u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            218u32..=221u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            224u32..=225u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            227u32..=242u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            245u32..=257u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            259u32..=271u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            274u32..=288u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            291u32..=366u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            400u32..=403u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            4160u32..=4162u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            4416u32 => unsafe { core::mem::transmute::<u32, Op>(4416u32) },
            4421u32..=4422u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            4428u32..=4432u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            4445u32..=4460u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            4472u32..=4477u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            4479u32..=4483u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5000u32..=5007u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5011u32..=5012u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5056u32 => unsafe { core::mem::transmute::<u32, Op>(5056u32) },
            5075u32 => unsafe { core::mem::transmute::<u32, Op>(5075u32) },
            5078u32 => unsafe { core::mem::transmute::<u32, Op>(5078u32) },
            5090u32 => unsafe { core::mem::transmute::<u32, Op>(5090u32) },
            5249u32..=5281u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5283u32 => unsafe { core::mem::transmute::<u32, Op>(5283u32) },
            5294u32..=5296u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5299u32..=5301u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5334u32..=5341u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5344u32 => unsafe { core::mem::transmute::<u32, Op>(5344u32) },
            5358u32..=5362u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5364u32..=5365u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5380u32..=5381u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5391u32..=5397u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5571u32..=5578u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5580u32..=5581u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5585u32..=5598u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5600u32..=5601u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5609u32..=5611u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5614u32..=5615u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5630u32..=5633u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5699u32..=5816u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5818u32..=5820u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5840u32..=5843u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5846u32..=5882u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5887u32 => unsafe { core::mem::transmute::<u32, Op>(5887u32) },
            5911u32..=5913u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5923u32..=5934u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5938u32 => unsafe { core::mem::transmute::<u32, Op>(5938u32) },
            5946u32..=5947u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            5949u32 => unsafe { core::mem::transmute::<u32, Op>(5949u32) },
            6016u32..=6032u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            6035u32 => unsafe { core::mem::transmute::<u32, Op>(6035u32) },
            6086u32 => unsafe { core::mem::transmute::<u32, Op>(6086u32) },
            6090u32..=6092u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            6116u32..=6117u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            6142u32..=6143u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            6401u32..=6408u32 => unsafe { core::mem::transmute::<u32, Op>(n) },
            _ => return None,
        })
    }
}
#[allow(clippy::upper_case_acronyms)]
#[allow(non_upper_case_globals)]
impl Op {
    pub const SDotKHR: Op = Op::SDot;
    pub const UDotKHR: Op = Op::UDot;
    pub const SUDotKHR: Op = Op::SUDot;
    pub const SDotAccSatKHR: Op = Op::SDotAccSat;
    pub const UDotAccSatKHR: Op = Op::UDotAccSat;
    pub const SUDotAccSatKHR: Op = Op::SUDotAccSat;
    pub const ReportIntersectionNV: Op = Op::ReportIntersectionKHR;
    pub const TypeAccelerationStructureNV: Op = Op::TypeAccelerationStructureKHR;
    pub const DemoteToHelperInvocationEXT: Op = Op::DemoteToHelperInvocation;
    pub const DecorateStringGOOGLE: Op = Op::DecorateString;
    pub const MemberDecorateStringGOOGLE: Op = Op::MemberDecorateString;
}
#[doc = "[GLSL.std.450](https://www.khronos.org/registry/spir-v/specs/unified1/GLSL.std.450.html) extended instruction opcode"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
impl GLOp {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            1u32..=81u32 => unsafe { core::mem::transmute::<u32, GLOp>(n) },
            _ => return None,
        })
    }
}
#[doc = "[OpenCL.std](https://www.khronos.org/registry/spir-v/specs/unified1/OpenCL.ExtendedInstructionSet.100.html) extended instruction opcode"]
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
#[allow(clippy::upper_case_acronyms)]
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
    bitselect = 186u32,
    select = 187u32,
    u_abs = 201u32,
    u_abs_diff = 202u32,
    u_mul_hi = 203u32,
    u_mad_hi = 204u32,
}
impl CLOp {
    pub fn from_u32(n: u32) -> Option<Self> {
        Some(match n {
            0u32..=110u32 => unsafe { core::mem::transmute::<u32, CLOp>(n) },
            141u32..=187u32 => unsafe { core::mem::transmute::<u32, CLOp>(n) },
            201u32..=204u32 => unsafe { core::mem::transmute::<u32, CLOp>(n) },
            _ => return None,
        })
    }
}
