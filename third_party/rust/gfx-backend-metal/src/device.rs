use crate::{
    command,
    conversions as conv,
    internal::{Channel, FastStorageMap},
    native as n,
    AsNative,
    Backend,
    OnlineRecording,
    QueueFamily,
    ResourceIndex,
    Shared,
    Surface,
    Swapchain,
    VisibilityShared,
    MAX_BOUND_DESCRIPTOR_SETS,
    MAX_COLOR_ATTACHMENTS,
};

use arrayvec::ArrayVec;
use auxil::{spirv_cross_specialize_ast, FastHashMap};
use cocoa::foundation::{NSRange, NSUInteger};
use copyless::VecHelper;
use foreign_types::{ForeignType, ForeignTypeRef};
use hal::{
    adapter,
    buffer,
    device::{
        AllocationError,
        BindError,
        CreationError as DeviceCreationError,
        DeviceLost,
        MapError,
        OomOrDeviceLost,
        OutOfMemory,
        ShaderError,
    },
    format,
    image,
    memory,
    memory::Properties,
    pass,
    pool::CommandPoolCreateFlags,
    pso,
    pso::VertexInputRate,
    query,
    queue::{QueueFamilyId, QueueGroup, QueuePriority},
    window,
};
use metal::{
    self,
    CaptureManager,
    MTLCPUCacheMode,
    MTLLanguageVersion,
    MTLPrimitiveTopologyClass,
    MTLPrimitiveType,
    MTLResourceOptions,
    MTLSamplerBorderColor,
    MTLSamplerMipFilter,
    MTLStorageMode,
    MTLTextureType,
    MTLVertexStepFunction,
};
use objc::rc::autoreleasepool;
use objc::runtime::{Object, BOOL, NO};
use parking_lot::Mutex;
use spirv_cross::{msl, spirv, ErrorCode as SpirvErrorCode};

use std::borrow::Borrow;
use std::cell::RefCell;
use std::collections::hash_map::Entry;
use std::collections::BTreeMap;
use std::ops::Range;
use std::path::Path;
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};
use std::{cmp, iter, mem, ptr, thread, time};

const PUSH_CONSTANTS_DESC_SET: u32 = !0;
const PUSH_CONSTANTS_DESC_BINDING: u32 = 0;
const STRIDE_GRANULARITY: pso::ElemStride = 4; //TODO: work around?
const SHADER_STAGE_COUNT: usize = 3;

/// Emit error during shader module creation. Used if we don't expect an error
/// but might panic due to an exception in SPIRV-Cross.
fn gen_unexpected_error(err: SpirvErrorCode) -> ShaderError {
    let msg = match err {
        SpirvErrorCode::CompilationError(msg) => msg,
        SpirvErrorCode::Unhandled => "Unexpected error".into(),
    };
    ShaderError::CompilationFailed(msg)
}

#[derive(Clone, Debug)]
enum FunctionError {
    InvalidEntryPoint,
    MissingRequiredSpecialization,
    BadSpecialization,
}

fn get_final_function(
    library: &metal::LibraryRef,
    entry: &str,
    specialization: &pso::Specialization,
    function_specialization: bool,
) -> Result<metal::Function, FunctionError> {
    type MTLFunctionConstant = Object;

    let mut mtl_function = library.get_function(entry, None).map_err(|e| {
        error!("Function retrieval error {:?}", e);
        FunctionError::InvalidEntryPoint
    })?;

    if !function_specialization {
        if !specialization.data.is_empty() || !specialization.constants.is_empty() {
            error!("platform does not support specialization");
        }
        return Ok(mtl_function);
    }

    let dictionary = mtl_function.function_constants_dictionary();
    let count: NSUInteger = unsafe { msg_send![dictionary, count] };
    if count == 0 {
        return Ok(mtl_function);
    }

    let all_values: *mut Object = unsafe { msg_send![dictionary, allValues] };

    let constants = metal::FunctionConstantValues::new();
    for i in 0 .. count {
        let object: *mut MTLFunctionConstant = unsafe { msg_send![all_values, objectAtIndex: i] };
        let index: NSUInteger = unsafe { msg_send![object, index] };
        let required: BOOL = unsafe { msg_send![object, required] };
        match specialization
            .constants
            .iter()
            .find(|c| c.id as NSUInteger == index)
        {
            Some(c) => unsafe {
                let ptr = &specialization.data[c.range.start as usize] as *const u8 as *const _;
                let ty: metal::MTLDataType = msg_send![object, type];
                constants.set_constant_value_at_index(c.id as NSUInteger, ty, ptr);
            },
            None if required != NO => {
                //TODO: get name
                error!("Missing required specialization constant id {}", index);
                return Err(FunctionError::MissingRequiredSpecialization);
            }
            None => {}
        }
    }

    mtl_function = library.get_function(entry, Some(constants)).map_err(|e| {
        error!("Specialized function retrieval error {:?}", e);
        FunctionError::BadSpecialization
    })?;

    Ok(mtl_function)
}

impl VisibilityShared {
    fn are_available(&self, pool_base: query::Id, queries: &Range<query::Id>) -> bool {
        unsafe {
            let availability_ptr = ((self.buffer.contents() as *mut u8)
                .offset(self.availability_offset as isize)
                as *mut u32)
                .offset(pool_base as isize);
            queries
                .clone()
                .all(|id| *availability_ptr.offset(id as isize) != 0)
        }
    }
}

#[derive(Debug)]
pub struct Device {
    pub(crate) shared: Arc<Shared>,
    invalidation_queue: command::QueueInner,
    memory_types: Vec<adapter::MemoryType>,
    features: hal::Features,
    pub online_recording: OnlineRecording,
}
unsafe impl Send for Device {}
unsafe impl Sync for Device {}

impl Drop for Device {
    fn drop(&mut self) {
        if cfg!(feature = "auto-capture") {
            info!("Metal capture stop");
            let shared_capture_manager = CaptureManager::shared();
            if let Some(default_capture_scope) = shared_capture_manager.default_capture_scope() {
                default_capture_scope.end_scope();
            }
            shared_capture_manager.stop_capture();
        }
    }
}

bitflags! {
    /// Memory type bits.
    struct MemoryTypes: u64 {
        const PRIVATE = 1<<0;
        const SHARED = 1<<1;
        const MANAGED_UPLOAD = 1<<2;
        const MANAGED_DOWNLOAD = 1<<3;
    }
}

impl MemoryTypes {
    fn describe(index: usize) -> (MTLStorageMode, MTLCPUCacheMode) {
        match Self::from_bits(1 << index).unwrap() {
            Self::PRIVATE => (MTLStorageMode::Private, MTLCPUCacheMode::DefaultCache),
            Self::SHARED => (MTLStorageMode::Shared, MTLCPUCacheMode::DefaultCache),
            Self::MANAGED_UPLOAD => (MTLStorageMode::Managed, MTLCPUCacheMode::WriteCombined),
            Self::MANAGED_DOWNLOAD => (MTLStorageMode::Managed, MTLCPUCacheMode::DefaultCache),
            _ => unreachable!(),
        }
    }
}

#[derive(Debug)]
pub struct PhysicalDevice {
    pub(crate) shared: Arc<Shared>,
    memory_types: Vec<adapter::MemoryType>,
}
unsafe impl Send for PhysicalDevice {}
unsafe impl Sync for PhysicalDevice {}

impl PhysicalDevice {
    pub(crate) fn new(shared: Arc<Shared>) -> Self {
        let memory_types = if shared.private_caps.os_is_mac {
            vec![
                adapter::MemoryType {
                    // PRIVATE
                    properties: Properties::DEVICE_LOCAL,
                    heap_index: 0,
                },
                adapter::MemoryType {
                    // SHARED
                    properties: Properties::CPU_VISIBLE | Properties::COHERENT,
                    heap_index: 1,
                },
                adapter::MemoryType {
                    // MANAGED_UPLOAD
                    properties: Properties::DEVICE_LOCAL | Properties::CPU_VISIBLE,
                    heap_index: 1,
                },
                adapter::MemoryType {
                    // MANAGED_DOWNLOAD
                    properties: Properties::DEVICE_LOCAL
                        | Properties::CPU_VISIBLE
                        | Properties::CPU_CACHED,
                    heap_index: 1,
                },
            ]
        } else {
            vec![
                adapter::MemoryType {
                    // PRIVATE
                    properties: Properties::DEVICE_LOCAL,
                    heap_index: 0,
                },
                adapter::MemoryType {
                    // SHARED
                    properties: Properties::CPU_VISIBLE | Properties::COHERENT,
                    heap_index: 1,
                },
            ]
        };
        PhysicalDevice {
            shared: shared.clone(),
            memory_types,
        }
    }

    /// Return true if the specified format-swizzle pair is supported natively.
    pub fn supports_swizzle(&self, format: format::Format, swizzle: format::Swizzle) -> bool {
        self.shared
            .private_caps
            .map_format_with_swizzle(format, swizzle)
            .is_some()
    }
}

impl adapter::PhysicalDevice<Backend> for PhysicalDevice {
    unsafe fn open(
        &self,
        families: &[(&QueueFamily, &[QueuePriority])],
        requested_features: hal::Features,
    ) -> Result<adapter::Gpu<Backend>, DeviceCreationError> {
        use hal::queue::QueueFamily as _;

        // TODO: Query supported features by feature set rather than hard coding in the supported
        // features. https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
        if !self.features().contains(requested_features) {
            warn!(
                "Features missing: {:?}",
                requested_features - self.features()
            );
            return Err(DeviceCreationError::MissingFeature);
        }

        let device = self.shared.device.lock();

        if cfg!(feature = "auto-capture") {
            info!("Metal capture start");
            let shared_capture_manager = CaptureManager::shared();
            let default_capture_scope =
                shared_capture_manager.new_capture_scope_with_device(&*device);
            shared_capture_manager.set_default_capture_scope(&default_capture_scope);
            shared_capture_manager.start_capture_with_scope(&default_capture_scope);
            default_capture_scope.begin_scope();
        }

        assert_eq!(families.len(), 1);
        assert_eq!(families[0].1.len(), 1);
        let mut queue_group = QueueGroup::new(families[0].0.id());
        for _ in 0 .. self.shared.private_caps.exposed_queues {
            queue_group.add_queue(command::CommandQueue::new(self.shared.clone()));
        }

        let device = Device {
            shared: self.shared.clone(),
            invalidation_queue: command::QueueInner::new(&*device, Some(1)),
            memory_types: self.memory_types.clone(),
            features: requested_features,
            online_recording: OnlineRecording::default(),
        };

        Ok(adapter::Gpu {
            device,
            queue_groups: vec![queue_group],
        })
    }

    fn format_properties(&self, format: Option<format::Format>) -> format::Properties {
        match format {
            Some(format) => self.shared.private_caps.map_format_properties(format),
            None => format::Properties {
                linear_tiling: format::ImageFeature::empty(),
                optimal_tiling: format::ImageFeature::empty(),
                buffer_features: format::BufferFeature::empty(),
            },
        }
    }

    fn image_format_properties(
        &self,
        format: format::Format,
        dimensions: u8,
        tiling: image::Tiling,
        usage: image::Usage,
        view_caps: image::ViewCapabilities,
    ) -> Option<image::FormatProperties> {
        if let image::Tiling::Linear = tiling {
            let format_desc = format.surface_desc();
            let host_usage = image::Usage::TRANSFER_SRC | image::Usage::TRANSFER_DST;
            if dimensions != 2
                || !view_caps.is_empty()
                || !host_usage.contains(usage)
                || format_desc.aspects != format::Aspects::COLOR
                || format_desc.is_compressed()
            {
                return None;
            }
        }
        if dimensions == 1
            && usage
                .intersects(image::Usage::COLOR_ATTACHMENT | image::Usage::DEPTH_STENCIL_ATTACHMENT)
        {
            // MTLRenderPassDescriptor texture must not be MTLTextureType1D
            return None;
        }
        if dimensions == 3 && view_caps.contains(image::ViewCapabilities::KIND_2D_ARRAY) {
            // Can't create 2D/2DArray views of 3D textures
            return None;
        }
        let max_dimension = if dimensions == 3 {
            self.shared.private_caps.max_texture_3d_size as _
        } else {
            self.shared.private_caps.max_texture_size as _
        };

        let max_extent = image::Extent {
            width: max_dimension,
            height: if dimensions >= 2 { max_dimension } else { 1 },
            depth: if dimensions >= 3 { max_dimension } else { 1 },
        };

        self.shared
            .private_caps
            .map_format(format)
            .map(|_| image::FormatProperties {
                max_extent,
                max_levels: if dimensions == 1 { 1 } else { 12 },
                // 3D images enforce a single layer
                max_layers: if dimensions == 3 {
                    1
                } else {
                    self.shared.private_caps.max_texture_layers as _
                },
                sample_count_mask: self.shared.private_caps.sample_count_mask as _,
                //TODO: buffers and textures have separate limits
                // Max buffer size is determined by feature set
                // Max texture size does not appear to be documented publicly
                max_resource_size: self.shared.private_caps.max_buffer_size as _,
            })
    }

    fn memory_properties(&self) -> adapter::MemoryProperties {
        adapter::MemoryProperties {
            memory_heaps: vec![
                !0, //TODO: private memory limits
                self.shared.private_caps.max_buffer_size,
            ],
            memory_types: self.memory_types.to_vec(),
        }
    }

    fn features(&self) -> hal::Features {
        hal::Features::empty()
            | hal::Features::ROBUST_BUFFER_ACCESS
            | hal::Features::DRAW_INDIRECT_FIRST_INSTANCE
            | hal::Features::DEPTH_CLAMP
            | hal::Features::SAMPLER_ANISOTROPY
            | hal::Features::FORMAT_BC
            | hal::Features::PRECISE_OCCLUSION_QUERY
            | hal::Features::SHADER_STORAGE_BUFFER_ARRAY_DYNAMIC_INDEXING
            | hal::Features::VERTEX_STORES_AND_ATOMICS
            | hal::Features::FRAGMENT_STORES_AND_ATOMICS
            | if self.shared.private_caps.dual_source_blending {
                hal::Features::DUAL_SRC_BLENDING
            } else {
                hal::Features::empty()
            }
            | hal::Features::INSTANCE_RATE
            | hal::Features::SEPARATE_STENCIL_REF_VALUES
            | if self.shared.private_caps.expose_line_mode {
                hal::Features::NON_FILL_POLYGON_MODE
            } else {
                hal::Features::empty()
            }
            | hal::Features::SHADER_CLIP_DISTANCE
            //| hal::Features::SAMPLER_MIRROR_CLAMP_EDGE
            | hal::Features::NDC_Y_UP
    }

    fn hints(&self) -> hal::Hints {
        if self.shared.private_caps.base_vertex_instance_drawing {
            hal::Hints::BASE_VERTEX_INSTANCE_DRAWING
        } else {
            hal::Hints::empty()
        }
    }

    fn limits(&self) -> hal::Limits {
        let pc = &self.shared.private_caps;
        hal::Limits {
            max_image_1d_size: pc.max_texture_size as _,
            max_image_2d_size: pc.max_texture_size as _,
            max_image_3d_size: pc.max_texture_3d_size as _,
            max_image_cube_size: pc.max_texture_size as _,
            max_image_array_layers: pc.max_texture_layers as _,
            max_texel_elements: (pc.max_texture_size * pc.max_texture_size) as usize,
            max_uniform_buffer_range: pc.max_buffer_size,
            max_storage_buffer_range: pc.max_buffer_size,
            // "Maximum length of an inlined constant data buffer, per graphics or compute function"
            max_push_constants_size: 0x1000,
            max_sampler_allocation_count: !0,
            max_bound_descriptor_sets: MAX_BOUND_DESCRIPTOR_SETS as _,
            max_descriptor_set_samplers: pc.max_samplers_per_stage as usize * SHADER_STAGE_COUNT,
            max_descriptor_set_uniform_buffers: pc.max_buffers_per_stage as usize
                * SHADER_STAGE_COUNT,
            max_descriptor_set_storage_buffers: pc.max_buffers_per_stage as usize
                * SHADER_STAGE_COUNT,
            max_descriptor_set_sampled_images: pc.max_textures_per_stage as usize
                * SHADER_STAGE_COUNT,
            max_descriptor_set_storage_images: pc.max_textures_per_stage as usize
                * SHADER_STAGE_COUNT,
            max_descriptor_set_input_attachments: pc.max_textures_per_stage as usize
                * SHADER_STAGE_COUNT,
            max_fragment_input_components: pc.max_fragment_input_components as usize,
            max_framebuffer_layers: 2048, // TODO: Determine is this is the correct value
            max_memory_allocation_count: 4096, // TODO: Determine is this is the correct value

            max_per_stage_descriptor_samplers: pc.max_samplers_per_stage as usize,
            max_per_stage_descriptor_uniform_buffers: pc.max_buffers_per_stage as usize,
            max_per_stage_descriptor_storage_buffers: pc.max_buffers_per_stage as usize,
            max_per_stage_descriptor_sampled_images: pc.max_textures_per_stage as usize,
            max_per_stage_descriptor_storage_images: pc.max_textures_per_stage as usize,
            max_per_stage_descriptor_input_attachments: pc.max_textures_per_stage as usize, //TODO
            max_per_stage_resources: 0x100,                                                 //TODO

            max_patch_size: 0, // No tessellation

            // Note: The maximum number of supported viewports and scissor rectangles varies by device.
            // TODO: read from Metal Feature Sets.
            max_viewports: 1,
            max_viewport_dimensions: [pc.max_texture_size as _; 2],
            max_framebuffer_extent: hal::image::Extent {
                //TODO
                width: pc.max_texture_size as _,
                height: pc.max_texture_size as _,
                depth: pc.max_texture_layers as _,
            },

            optimal_buffer_copy_offset_alignment: pc.buffer_alignment,
            optimal_buffer_copy_pitch_alignment: 4,
            min_texel_buffer_offset_alignment: pc.buffer_alignment,
            min_uniform_buffer_offset_alignment: pc.buffer_alignment,
            min_storage_buffer_offset_alignment: pc.buffer_alignment,

            max_compute_work_group_count: [16; 3], // TODO
            max_compute_work_group_size: [64; 3],  // TODO

            max_vertex_input_attributes: 31,
            max_vertex_input_bindings: 31,
            max_vertex_input_attribute_offset: 255, // TODO
            max_vertex_input_binding_stride: 256,   // TODO
            max_vertex_output_components: pc.max_fragment_input_components as usize,

            framebuffer_color_sample_counts: 0b101,   // TODO
            framebuffer_depth_sample_counts: 0b101,   // TODO
            framebuffer_stencil_sample_counts: 0b101, // TODO
            max_color_attachments: MAX_COLOR_ATTACHMENTS,

            buffer_image_granularity: 1,
            // Note: we issue Metal buffer-to-buffer copies on memory flush/invalidate,
            // and those need to operate on sizes being multiples of 4.
            non_coherent_atom_size: 4,
            max_sampler_anisotropy: 16.,
            min_vertex_input_binding_stride_alignment: STRIDE_GRANULARITY as u64,

            ..hal::Limits::default() // TODO!
        }
    }
}

pub struct LanguageVersion {
    pub major: u8,
    pub minor: u8,
}

impl LanguageVersion {
    pub fn new(major: u8, minor: u8) -> Self {
        LanguageVersion { major, minor }
    }
}

impl Device {
    fn _is_heap_coherent(&self, heap: &n::MemoryHeap) -> bool {
        match *heap {
            n::MemoryHeap::Private => false,
            n::MemoryHeap::Public(memory_type, _) => self.memory_types[memory_type.0]
                .properties
                .contains(Properties::COHERENT),
            n::MemoryHeap::Native(ref heap) => heap.storage_mode() == MTLStorageMode::Shared,
        }
    }

    pub fn create_shader_library_from_file<P>(
        &self,
        _path: P,
    ) -> Result<n::ShaderModule, ShaderError>
    where
        P: AsRef<Path>,
    {
        unimplemented!()
    }

    pub fn create_shader_library_from_source<S>(
        &self,
        source: S,
        version: LanguageVersion,
        rasterization_enabled: bool,
    ) -> Result<n::ShaderModule, ShaderError>
    where
        S: AsRef<str>,
    {
        let options = metal::CompileOptions::new();
        let msl_version = match version {
            LanguageVersion { major: 1, minor: 0 } => MTLLanguageVersion::V1_0,
            LanguageVersion { major: 1, minor: 1 } => MTLLanguageVersion::V1_1,
            LanguageVersion { major: 1, minor: 2 } => MTLLanguageVersion::V1_2,
            LanguageVersion { major: 2, minor: 0 } => MTLLanguageVersion::V2_0,
            LanguageVersion { major: 2, minor: 1 } => MTLLanguageVersion::V2_1,
            _ => {
                return Err(ShaderError::CompilationFailed(
                    "shader model not supported".into(),
                ));
            }
        };
        if msl_version > self.shared.private_caps.msl_version {
            return Err(ShaderError::CompilationFailed(
                "shader model too high".into(),
            ));
        }
        options.set_language_version(msl_version);

        self.shared
            .device
            .lock()
            .new_library_with_source(source.as_ref(), &options)
            .map(|library| {
                n::ShaderModule::Compiled(n::ModuleInfo {
                    library,
                    entry_point_map: n::EntryPointMap::default(),
                    rasterization_enabled,
                })
            })
            .map_err(|e| ShaderError::CompilationFailed(e.into()))
    }

    fn compile_shader_library(
        device: &Mutex<metal::Device>,
        raw_data: &[u32],
        compiler_options: &msl::CompilerOptions,
        msl_version: MTLLanguageVersion,
        specialization: &pso::Specialization,
    ) -> Result<n::ModuleInfo, ShaderError> {
        let module = spirv::Module::from_words(raw_data);

        // now parse again using the new overrides
        let mut ast = spirv::Ast::<msl::Target>::parse(&module).map_err(|err| {
            ShaderError::CompilationFailed(match err {
                SpirvErrorCode::CompilationError(msg) => msg,
                SpirvErrorCode::Unhandled => "Unexpected parse error".into(),
            })
        })?;

        spirv_cross_specialize_ast(&mut ast, specialization)?;

        ast.set_compiler_options(compiler_options)
            .map_err(gen_unexpected_error)?;

        let entry_points = ast.get_entry_points().map_err(|err| {
            ShaderError::CompilationFailed(match err {
                SpirvErrorCode::CompilationError(msg) => msg,
                SpirvErrorCode::Unhandled => "Unexpected entry point error".into(),
            })
        })?;

        let shader_code = ast.compile().map_err(|err| {
            ShaderError::CompilationFailed(match err {
                SpirvErrorCode::CompilationError(msg) => msg,
                SpirvErrorCode::Unhandled => "Unknown compile error".into(),
            })
        })?;

        let mut entry_point_map = n::EntryPointMap::default();
        for entry_point in entry_points {
            info!("Entry point {:?}", entry_point);
            let cleansed = ast
                .get_cleansed_entry_point_name(&entry_point.name, entry_point.execution_model)
                .map_err(|err| {
                    ShaderError::CompilationFailed(match err {
                        SpirvErrorCode::CompilationError(msg) => msg,
                        SpirvErrorCode::Unhandled => "Unknown compile error".into(),
                    })
                })?;
            entry_point_map.insert(
                entry_point.name,
                spirv::EntryPoint {
                    name: cleansed,
                    ..entry_point
                },
            );
        }

        let rasterization_enabled = ast
            .is_rasterization_enabled()
            .map_err(|_| ShaderError::CompilationFailed("Unknown compile error".into()))?;

        // done
        debug!("SPIRV-Cross generated shader:\n{}", shader_code);

        let options = metal::CompileOptions::new();
        options.set_language_version(msl_version);

        let library = device
            .lock()
            .new_library_with_source(shader_code.as_ref(), &options)
            .map_err(|err| ShaderError::CompilationFailed(err.into()))?;

        Ok(n::ModuleInfo {
            library,
            entry_point_map,
            rasterization_enabled,
        })
    }

    fn load_shader(
        &self,
        ep: &pso::EntryPoint<Backend>,
        layout: &n::PipelineLayout,
        primitive_class: MTLPrimitiveTopologyClass,
        pipeline_cache: Option<&n::PipelineCache>,
    ) -> Result<(metal::Library, metal::Function, metal::MTLSize, bool), pso::CreationError> {
        let device = &self.shared.device;
        let msl_version = self.shared.private_caps.msl_version;
        let module_map;
        let (info_owned, info_guard);

        let info = match *ep.module {
            n::ShaderModule::Compiled(ref info) => info,
            n::ShaderModule::Raw(ref data) => {
                let compiler_options = match primitive_class {
                    MTLPrimitiveTopologyClass::Point => &layout.shader_compiler_options_point,
                    _ => &layout.shader_compiler_options,
                };
                match pipeline_cache {
                    Some(cache) => {
                        module_map = cache
                            .modules
                            .get_or_create_with(compiler_options, || FastStorageMap::default());
                        info_guard = module_map.get_or_create_with(data, || {
                            Self::compile_shader_library(
                                device,
                                data,
                                compiler_options,
                                msl_version,
                                &ep.specialization,
                            )
                            .unwrap()
                        });
                        &*info_guard
                    }
                    None => {
                        info_owned = Self::compile_shader_library(
                            device,
                            data,
                            compiler_options,
                            msl_version,
                            &ep.specialization,
                        )
                        .map_err(|e| {
                            error!("Error compiling the shader {:?}", e);
                            pso::CreationError::Other
                        })?;
                        &info_owned
                    }
                }
            }
        };

        let lib = info.library.clone();
        let (name, wg_size) = match info.entry_point_map.get(ep.entry) {
            Some(p) => (
                p.name.as_str(),
                metal::MTLSize {
                    width: p.work_group_size.x as _,
                    height: p.work_group_size.y as _,
                    depth: p.work_group_size.z as _,
                },
            ),
            // this can only happen if the shader came directly from the user
            None => (
                ep.entry,
                metal::MTLSize {
                    width: 0,
                    height: 0,
                    depth: 0,
                },
            ),
        };
        let mtl_function = get_final_function(
            &lib,
            name,
            &ep.specialization,
            self.shared.private_caps.function_specialization,
        )
        .map_err(|e| {
            error!("Invalid shader entry point '{}': {:?}", name, e);
            pso::CreationError::Other
        })?;

        Ok((lib, mtl_function, wg_size, info.rasterization_enabled))
    }

    fn make_sampler_descriptor(
        &self,
        info: &image::SamplerDesc,
    ) -> Option<metal::SamplerDescriptor> {
        let caps = &self.shared.private_caps;
        let descriptor = metal::SamplerDescriptor::new();

        descriptor.set_normalized_coordinates(info.normalized);

        descriptor.set_min_filter(conv::map_filter(info.min_filter));
        descriptor.set_mag_filter(conv::map_filter(info.mag_filter));
        descriptor.set_mip_filter(match info.mip_filter {
            // Note: this shouldn't be required, but Metal appears to be confused when mipmaps
            // are provided even with trivial LOD bias.
            image::Filter::Nearest if info.lod_range.end.0 < 0.5 => {
                MTLSamplerMipFilter::NotMipmapped
            }
            image::Filter::Nearest => MTLSamplerMipFilter::Nearest,
            image::Filter::Linear => MTLSamplerMipFilter::Linear,
        });

        if let Some(aniso) = info.anisotropy_clamp {
            descriptor.set_max_anisotropy(aniso as _);
        }

        let (s, t, r) = info.wrap_mode;
        descriptor.set_address_mode_s(conv::map_wrap_mode(s));
        descriptor.set_address_mode_t(conv::map_wrap_mode(t));
        descriptor.set_address_mode_r(conv::map_wrap_mode(r));

        let lod_bias = info.lod_bias.0;
        if lod_bias != 0.0 {
            if self.features.contains(hal::Features::SAMPLER_MIP_LOD_BIAS) {
                unsafe {
                    descriptor.set_lod_bias(lod_bias);
                }
            } else {
                error!("Lod bias {:?} is not supported", info.lod_bias);
            }
        }
        descriptor.set_lod_min_clamp(info.lod_range.start.0);
        descriptor.set_lod_max_clamp(info.lod_range.end.0);

        // TODO: Clarify minimum macOS version with Apple (43707452)
        if (caps.os_is_mac && caps.has_version_at_least(10, 13))
            || (!caps.os_is_mac && caps.has_version_at_least(9, 0))
        {
            descriptor.set_lod_average(true); // optimization
        }

        if let Some(fun) = info.comparison {
            if !caps.mutable_comparison_samplers {
                return None;
            }
            descriptor.set_compare_function(conv::map_compare_function(fun));
        }
        if [r, s, t].iter().any(|&am| am == image::WrapMode::Border) {
            descriptor.set_border_color(match info.border.0 {
                0x0000_0000 => MTLSamplerBorderColor::TransparentBlack,
                0x0000_00FF => MTLSamplerBorderColor::OpaqueBlack,
                0xFFFF_FFFF => MTLSamplerBorderColor::OpaqueWhite,
                other => {
                    error!("Border color 0x{:X} is not supported", other);
                    MTLSamplerBorderColor::TransparentBlack
                }
            });
        }

        if caps.argument_buffers {
            descriptor.set_support_argument_buffers(true);
        }

        Some(descriptor)
    }

    fn make_sampler_data(info: &image::SamplerDesc) -> msl::SamplerData {
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

        let lods = info.lod_range.start.0 .. info.lod_range.end.0;
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
                Some(func) => unsafe { mem::transmute(conv::map_compare_function(func) as u32) },
                None => msl::SamplerCompareFunc::Always,
            },
            border_color: match info.border.0 {
                0x0000_0000 => msl::SamplerBorderColor::TransparentBlack,
                0x0000_00FF => msl::SamplerBorderColor::OpaqueBlack,
                0xFFFF_FFFF => msl::SamplerBorderColor::OpaqueWhite,
                other => {
                    error!("Border color 0x{:X} is not supported", other);
                    msl::SamplerBorderColor::TransparentBlack
                }
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
}

impl hal::device::Device<Backend> for Device {
    unsafe fn create_command_pool(
        &self,
        _family: QueueFamilyId,
        _flags: CommandPoolCreateFlags,
    ) -> Result<command::CommandPool, OutOfMemory> {
        Ok(command::CommandPool::new(
            &self.shared,
            self.online_recording.clone(),
        ))
    }

    unsafe fn destroy_command_pool(&self, mut pool: command::CommandPool) {
        use hal::pool::CommandPool as _;
        pool.reset(false);
    }

    unsafe fn create_render_pass<'a, IA, IS, ID>(
        &self,
        attachments: IA,
        subpasses: IS,
        _dependencies: ID,
    ) -> Result<n::RenderPass, OutOfMemory>
    where
        IA: IntoIterator,
        IA::Item: Borrow<pass::Attachment>,
        IS: IntoIterator,
        IS::Item: Borrow<pass::SubpassDesc<'a>>,
        ID: IntoIterator,
        ID::Item: Borrow<pass::SubpassDependency>,
    {
        let attachments: Vec<pass::Attachment> = attachments
            .into_iter()
            .map(|at| at.borrow().clone())
            .collect();

        let mut subpasses: Vec<n::Subpass> = subpasses
            .into_iter()
            .map(|sp| {
                let sub = sp.borrow();
                let mut colors: ArrayVec<[_; MAX_COLOR_ATTACHMENTS]> = sub
                    .colors
                    .iter()
                    .map(|&(id, _)| (id, n::SubpassOps::empty(), None))
                    .collect();
                for (color, &(resolve_id, _)) in colors.iter_mut().zip(sub.resolves.iter()) {
                    if resolve_id != pass::ATTACHMENT_UNUSED {
                        color.2 = Some(resolve_id);
                    }
                }

                n::Subpass {
                    colors,
                    depth_stencil: sub
                        .depth_stencil
                        .map(|&(id, _)| (id, n::SubpassOps::empty())),
                    inputs: sub.inputs.iter().map(|&(id, _)| id).collect(),
                    target_formats: n::SubpassFormats {
                        colors: sub
                            .colors
                            .iter()
                            .map(|&(id, _)| {
                                let format =
                                    attachments[id].format.expect("No color format provided");
                                let mtl_format = self
                                    .shared
                                    .private_caps
                                    .map_format(format)
                                    .expect("Unable to map color format!");
                                (mtl_format, Channel::from(format.base_format().1))
                            })
                            .collect(),
                        depth_stencil: sub.depth_stencil.map(|&(id, _)| {
                            self.shared
                                .private_caps
                                .map_format(
                                    attachments[id]
                                        .format
                                        .expect("No depth-stencil format provided"),
                                )
                                .expect("Unable to map depth-stencil format!")
                        }),
                    },
                }
            })
            .collect();

        // sprinkle load operations
        // an attachment receives LOAD flag on a subpass if it's the first sub-pass that uses it
        let mut use_mask = 0u64;
        for sub in subpasses.iter_mut() {
            for &mut (id, ref mut ops, _) in sub.colors.iter_mut() {
                if use_mask & 1 << id == 0 {
                    *ops |= n::SubpassOps::LOAD;
                    use_mask ^= 1 << id;
                }
            }
            if let Some((id, ref mut ops)) = sub.depth_stencil {
                if use_mask & 1 << id == 0 {
                    *ops |= n::SubpassOps::LOAD;
                    use_mask ^= 1 << id;
                }
            }
        }
        // sprinkle store operations
        // an attachment receives STORE flag on a subpass if it's the last sub-pass that uses it
        for sub in subpasses.iter_mut().rev() {
            for &mut (id, ref mut ops, _) in sub.colors.iter_mut() {
                if use_mask & 1 << id != 0 {
                    *ops |= n::SubpassOps::STORE;
                    use_mask ^= 1 << id;
                }
            }
            if let Some((id, ref mut ops)) = sub.depth_stencil {
                if use_mask & 1 << id != 0 {
                    *ops |= n::SubpassOps::STORE;
                    use_mask ^= 1 << id;
                }
            }
        }

        Ok(n::RenderPass {
            attachments,
            subpasses,
            name: String::new(),
        })
    }

    unsafe fn create_pipeline_layout<IS, IR>(
        &self,
        set_layouts: IS,
        push_constant_ranges: IR,
    ) -> Result<n::PipelineLayout, OutOfMemory>
    where
        IS: IntoIterator,
        IS::Item: Borrow<n::DescriptorSetLayout>,
        IR: IntoIterator,
        IR::Item: Borrow<(pso::ShaderStageFlags, Range<u32>)>,
    {
        let mut stage_infos = [
            (
                pso::ShaderStageFlags::VERTEX,
                spirv::ExecutionModel::Vertex,
                n::ResourceData::<ResourceIndex>::new(),
            ),
            (
                pso::ShaderStageFlags::FRAGMENT,
                spirv::ExecutionModel::Fragment,
                n::ResourceData::<ResourceIndex>::new(),
            ),
            (
                pso::ShaderStageFlags::COMPUTE,
                spirv::ExecutionModel::GlCompute,
                n::ResourceData::<ResourceIndex>::new(),
            ),
        ];
        let mut res_overrides = BTreeMap::new();
        let mut const_samplers = BTreeMap::new();
        let mut infos = Vec::new();

        // First, place the push constants
        let mut pc_buffers = [None; 3];
        let mut pc_limits = [0u32; 3];
        for pcr in push_constant_ranges {
            let (flags, range) = pcr.borrow();
            for (limit, &(stage_bit, _, _)) in pc_limits.iter_mut().zip(&stage_infos) {
                if flags.contains(stage_bit) {
                    debug_assert_eq!(range.end % 4, 0);
                    *limit = (range.end / 4).max(*limit);
                }
            }
        }

        const LIMIT_MASK: u32 = 3;
        // round up the limits alignment to 4, so that it matches MTL compiler logic
        //TODO: figure out what and how exactly does the alignment. Clearly, it's not
        // straightforward, given that value of 2 stays non-aligned.
        for limit in &mut pc_limits {
            if *limit > LIMIT_MASK {
                *limit = (*limit + LIMIT_MASK) & !LIMIT_MASK;
            }
        }

        for ((limit, ref mut buffer_index), &mut (_, stage, ref mut counters)) in pc_limits
            .iter()
            .zip(pc_buffers.iter_mut())
            .zip(stage_infos.iter_mut())
        {
            // handle the push constant buffer assignment and shader overrides
            if *limit != 0 {
                let index = counters.buffers;
                **buffer_index = Some(index);
                counters.buffers += 1;

                res_overrides.insert(
                    msl::ResourceBindingLocation {
                        stage,
                        desc_set: PUSH_CONSTANTS_DESC_SET,
                        binding: PUSH_CONSTANTS_DESC_BINDING,
                    },
                    msl::ResourceBinding {
                        buffer_id: index as _,
                        texture_id: !0,
                        sampler_id: !0,
                    },
                );
            }
        }

        // Second, place the descripted resources
        for (set_index, set_layout) in set_layouts.into_iter().enumerate() {
            // remember where the resources for this set start at each shader stage
            let mut dynamic_buffers = Vec::new();
            let offsets = n::MultiStageResourceCounters {
                vs: stage_infos[0].2.clone(),
                ps: stage_infos[1].2.clone(),
                cs: stage_infos[2].2.clone(),
            };
            match *set_layout.borrow() {
                n::DescriptorSetLayout::Emulated(ref desc_layouts, ref samplers) => {
                    for &(binding, ref data) in samplers {
                        //TODO: array support?
                        const_samplers.insert(
                            msl::SamplerLocation {
                                desc_set: set_index as u32,
                                binding,
                            },
                            data.clone(),
                        );
                    }
                    for layout in desc_layouts.iter() {
                        if layout
                            .content
                            .contains(n::DescriptorContent::DYNAMIC_BUFFER)
                        {
                            dynamic_buffers.alloc().init(n::MultiStageData {
                                vs: if layout.stages.contains(pso::ShaderStageFlags::VERTEX) {
                                    stage_infos[0].2.buffers
                                } else {
                                    !0
                                },
                                ps: if layout.stages.contains(pso::ShaderStageFlags::FRAGMENT) {
                                    stage_infos[1].2.buffers
                                } else {
                                    !0
                                },
                                cs: if layout.stages.contains(pso::ShaderStageFlags::COMPUTE) {
                                    stage_infos[2].2.buffers
                                } else {
                                    !0
                                },
                            });
                        }
                        for &mut (stage_bit, stage, ref mut counters) in stage_infos.iter_mut() {
                            if !layout.stages.contains(stage_bit) {
                                continue;
                            }
                            let res = msl::ResourceBinding {
                                buffer_id: if layout.content.contains(n::DescriptorContent::BUFFER)
                                {
                                    counters.buffers += 1;
                                    (counters.buffers - 1) as _
                                } else {
                                    !0
                                },
                                texture_id: if layout
                                    .content
                                    .contains(n::DescriptorContent::TEXTURE)
                                {
                                    counters.textures += 1;
                                    (counters.textures - 1) as _
                                } else {
                                    !0
                                },
                                sampler_id: if layout
                                    .content
                                    .contains(n::DescriptorContent::SAMPLER)
                                {
                                    counters.samplers += 1;
                                    (counters.samplers - 1) as _
                                } else {
                                    !0
                                },
                            };
                            if layout.array_index == 0 {
                                let location = msl::ResourceBindingLocation {
                                    stage,
                                    desc_set: set_index as _,
                                    binding: layout.binding,
                                };
                                res_overrides.insert(location, res);
                            }
                        }
                    }
                }
                n::DescriptorSetLayout::ArgumentBuffer {
                    ref bindings,
                    stage_flags,
                    ..
                } => {
                    for &mut (stage_bit, stage, ref mut counters) in stage_infos.iter_mut() {
                        let has_stage = stage_flags.contains(stage_bit);
                        res_overrides.insert(
                            msl::ResourceBindingLocation {
                                stage,
                                desc_set: set_index as _,
                                binding: msl::ARGUMENT_BUFFER_BINDING,
                            },
                            msl::ResourceBinding {
                                buffer_id: if has_stage { counters.buffers } else { !0 },
                                texture_id: !0,
                                sampler_id: !0,
                            },
                        );
                        if has_stage {
                            res_overrides.extend(bindings.iter().map(|(&binding, arg)| {
                                let key = msl::ResourceBindingLocation {
                                    stage,
                                    desc_set: set_index as _,
                                    binding,
                                };
                                (key, arg.res.clone())
                            }));
                            counters.buffers += 1;
                        }
                    }
                }
            }

            infos.alloc().init(n::DescriptorSetInfo {
                offsets,
                dynamic_buffers,
            });
        }

        // Finally, make sure we fit the limits
        for &(_, _, ref counters) in stage_infos.iter() {
            assert!(counters.buffers <= self.shared.private_caps.max_buffers_per_stage);
            assert!(counters.textures <= self.shared.private_caps.max_textures_per_stage);
            assert!(counters.samplers <= self.shared.private_caps.max_samplers_per_stage);
        }

        let mut shader_compiler_options = msl::CompilerOptions::default();
        shader_compiler_options.version = match self.shared.private_caps.msl_version {
            MTLLanguageVersion::V1_0 => msl::Version::V1_0,
            MTLLanguageVersion::V1_1 => msl::Version::V1_1,
            MTLLanguageVersion::V1_2 => msl::Version::V1_2,
            MTLLanguageVersion::V2_0 => msl::Version::V2_0,
            MTLLanguageVersion::V2_1 => msl::Version::V2_1,
            MTLLanguageVersion::V2_2 => msl::Version::V2_2,
        };
        shader_compiler_options.enable_point_size_builtin = false;
        shader_compiler_options.vertex.invert_y = !self.features.contains(hal::Features::NDC_Y_UP);
        shader_compiler_options.resource_binding_overrides = res_overrides;
        shader_compiler_options.const_samplers = const_samplers;
        shader_compiler_options.enable_argument_buffers = self.shared.private_caps.argument_buffers;
        let mut shader_compiler_options_point = shader_compiler_options.clone();
        shader_compiler_options_point.enable_point_size_builtin = true;

        Ok(n::PipelineLayout {
            shader_compiler_options,
            shader_compiler_options_point,
            infos,
            total: n::MultiStageResourceCounters {
                vs: stage_infos[0].2.clone(),
                ps: stage_infos[1].2.clone(),
                cs: stage_infos[2].2.clone(),
            },
            push_constants: n::MultiStageData {
                vs: pc_buffers[0].map(|buffer_index| n::PushConstantInfo {
                    count: pc_limits[0],
                    buffer_index,
                }),
                ps: pc_buffers[1].map(|buffer_index| n::PushConstantInfo {
                    count: pc_limits[1],
                    buffer_index,
                }),
                cs: pc_buffers[2].map(|buffer_index| n::PushConstantInfo {
                    count: pc_limits[2],
                    buffer_index,
                }),
            },
            total_push_constants: pc_limits[0].max(pc_limits[1]).max(pc_limits[2]),
        })
    }

    unsafe fn create_pipeline_cache(
        &self,
        _data: Option<&[u8]>,
    ) -> Result<n::PipelineCache, OutOfMemory> {
        Ok(n::PipelineCache {
            modules: FastStorageMap::default(),
        })
    }

    unsafe fn get_pipeline_cache_data(
        &self,
        _cache: &n::PipelineCache,
    ) -> Result<Vec<u8>, OutOfMemory> {
        //empty
        Ok(Vec::new())
    }

    unsafe fn destroy_pipeline_cache(&self, _cache: n::PipelineCache) {
        //drop
    }

    unsafe fn merge_pipeline_caches<I>(
        &self,
        target: &n::PipelineCache,
        sources: I,
    ) -> Result<(), OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<n::PipelineCache>,
    {
        let mut dst = target.modules.whole_write();
        for source in sources {
            let src = source.borrow().modules.whole_write();
            for (key, value) in src.iter() {
                let storage = match dst.entry(key.clone()) {
                    Entry::Vacant(e) => e.insert(FastStorageMap::default()),
                    Entry::Occupied(e) => e.into_mut(),
                };
                let mut dst_module = storage.whole_write();
                let src_module = value.whole_write();
                for (key_module, value_module) in src_module.iter() {
                    match dst_module.entry(key_module.clone()) {
                        Entry::Vacant(em) => {
                            em.insert(value_module.clone());
                        }
                        Entry::Occupied(em) => {
                            assert_eq!(em.get().library.as_ptr(), value_module.library.as_ptr());
                            assert_eq!(em.get().entry_point_map, value_module.entry_point_map);
                        }
                    }
                }
            }
        }

        Ok(())
    }

    unsafe fn create_graphics_pipeline<'a>(
        &self,
        pipeline_desc: &pso::GraphicsPipelineDesc<'a, Backend>,
        cache: Option<&n::PipelineCache>,
    ) -> Result<n::GraphicsPipeline, pso::CreationError> {
        debug!("create_graphics_pipeline {:#?}", pipeline_desc);
        let pipeline = metal::RenderPipelineDescriptor::new();
        let pipeline_layout = &pipeline_desc.layout;
        let (rp_attachments, subpass) = {
            let pass::Subpass { main_pass, index } = pipeline_desc.subpass;
            (&main_pass.attachments, &main_pass.subpasses[index as usize])
        };

        let (primitive_class, primitive_type) = match pipeline_desc.input_assembler.primitive {
            pso::Primitive::PointList => {
                (MTLPrimitiveTopologyClass::Point, MTLPrimitiveType::Point)
            }
            pso::Primitive::LineList => (MTLPrimitiveTopologyClass::Line, MTLPrimitiveType::Line),
            pso::Primitive::LineStrip => {
                (MTLPrimitiveTopologyClass::Line, MTLPrimitiveType::LineStrip)
            }
            pso::Primitive::TriangleList => (
                MTLPrimitiveTopologyClass::Triangle,
                MTLPrimitiveType::Triangle,
            ),
            pso::Primitive::TriangleStrip => (
                MTLPrimitiveTopologyClass::Triangle,
                MTLPrimitiveType::TriangleStrip,
            ),
            pso::Primitive::PatchList(_) => (
                MTLPrimitiveTopologyClass::Unspecified,
                MTLPrimitiveType::Point,
            ),
        };
        if self.shared.private_caps.layered_rendering {
            pipeline.set_input_primitive_topology(primitive_class);
        }

        // Vertex shader
        let (vs_lib, vs_function, _, enable_rasterization) = self.load_shader(
            &pipeline_desc.shaders.vertex,
            pipeline_layout,
            primitive_class,
            cache,
        )?;
        pipeline.set_vertex_function(Some(&vs_function));

        // Fragment shader
        let fs_function;
        let fs_lib = match pipeline_desc.shaders.fragment {
            Some(ref ep) => {
                let (lib, fun, _, _) =
                    self.load_shader(ep, pipeline_layout, primitive_class, cache)?;
                fs_function = fun;
                pipeline.set_fragment_function(Some(&fs_function));
                Some(lib)
            }
            None => {
                // TODO: This is a workaround for what appears to be a Metal validation bug
                // A pixel format is required even though no attachments are provided
                if subpass.colors.is_empty() && subpass.depth_stencil.is_none() {
                    pipeline.set_depth_attachment_pixel_format(metal::MTLPixelFormat::Depth32Float);
                }
                None
            }
        };

        // Other shaders
        if pipeline_desc.shaders.hull.is_some() {
            return Err(pso::CreationError::Shader(ShaderError::UnsupportedStage(
                pso::Stage::Hull,
            )));
        }
        if pipeline_desc.shaders.domain.is_some() {
            return Err(pso::CreationError::Shader(ShaderError::UnsupportedStage(
                pso::Stage::Domain,
            )));
        }
        if pipeline_desc.shaders.geometry.is_some() {
            return Err(pso::CreationError::Shader(ShaderError::UnsupportedStage(
                pso::Stage::Geometry,
            )));
        }

        pipeline.set_rasterization_enabled(enable_rasterization);

        // Assign target formats
        let blend_targets = pipeline_desc
            .blender
            .targets
            .iter()
            .chain(iter::repeat(&pso::ColorBlendDesc::EMPTY));
        for (i, (&(mtl_format, _), color_desc)) in subpass
            .target_formats
            .colors
            .iter()
            .zip(blend_targets)
            .enumerate()
        {
            let desc = pipeline
                .color_attachments()
                .object_at(i as u64)
                .expect("too many color attachments");

            desc.set_pixel_format(mtl_format);
            desc.set_write_mask(conv::map_write_mask(color_desc.mask));

            if let Some(ref blend) = color_desc.blend {
                desc.set_blending_enabled(true);
                let (color_op, color_src, color_dst) = conv::map_blend_op(blend.color);
                let (alpha_op, alpha_src, alpha_dst) = conv::map_blend_op(blend.alpha);

                desc.set_rgb_blend_operation(color_op);
                desc.set_source_rgb_blend_factor(color_src);
                desc.set_destination_rgb_blend_factor(color_dst);

                desc.set_alpha_blend_operation(alpha_op);
                desc.set_source_alpha_blend_factor(alpha_src);
                desc.set_destination_alpha_blend_factor(alpha_dst);
            }
        }
        if let Some(mtl_format) = subpass.target_formats.depth_stencil {
            let orig_format = rp_attachments[subpass.depth_stencil.unwrap().0]
                .format
                .unwrap();
            if orig_format.is_depth() {
                pipeline.set_depth_attachment_pixel_format(mtl_format);
            }
            if orig_format.is_stencil() {
                pipeline.set_stencil_attachment_pixel_format(mtl_format);
            }
        }

        // Vertex buffers
        let vertex_descriptor = metal::VertexDescriptor::new();
        let mut vertex_buffers: n::VertexBufferVec = Vec::new();
        trace!("Vertex attribute remapping started");

        for &pso::AttributeDesc {
            location,
            binding,
            element,
        } in &pipeline_desc.attributes
        {
            let original = pipeline_desc
                .vertex_buffers
                .iter()
                .find(|vb| vb.binding == binding)
                .expect("no associated vertex buffer found");
            // handle wrapping offsets
            let elem_size = element.format.surface_desc().bits as pso::ElemOffset / 8;
            let (cut_offset, base_offset) =
                if original.stride == 0 || element.offset + elem_size <= original.stride {
                    (element.offset, 0)
                } else {
                    let remainder = element.offset % original.stride;
                    if remainder + elem_size <= original.stride {
                        (remainder, element.offset - remainder)
                    } else {
                        (0, element.offset)
                    }
                };
            let relative_index = vertex_buffers
                .iter()
                .position(|(ref vb, offset)| vb.binding == binding && base_offset == *offset)
                .unwrap_or_else(|| {
                    vertex_buffers.alloc().init((original.clone(), base_offset));
                    vertex_buffers.len() - 1
                });
            let mtl_buffer_index = self.shared.private_caps.max_buffers_per_stage
                - 1
                - (relative_index as ResourceIndex);
            if mtl_buffer_index < pipeline_layout.total.vs.buffers {
                error!("Attribute offset {} exceeds the stride {}, and there is no room for replacement.",
                    element.offset, original.stride);
                return Err(pso::CreationError::Other);
            }
            trace!("\tAttribute[{}] is mapped to vertex buffer[{}] with binding {} and offsets {} + {}",
                location, binding, mtl_buffer_index, base_offset, cut_offset);
            // pass the refined data to Metal
            let mtl_attribute_desc = vertex_descriptor
                .attributes()
                .object_at(location as u64)
                .expect("too many vertex attributes");
            let mtl_vertex_format =
                conv::map_vertex_format(element.format).expect("unsupported vertex format");
            mtl_attribute_desc.set_format(mtl_vertex_format);
            mtl_attribute_desc.set_buffer_index(mtl_buffer_index as _);
            mtl_attribute_desc.set_offset(cut_offset as _);
        }

        for (i, (vb, _)) in vertex_buffers.iter().enumerate() {
            let mtl_buffer_desc = vertex_descriptor
                .layouts()
                .object_at(self.shared.private_caps.max_buffers_per_stage as u64 - 1 - i as u64)
                .expect("too many vertex descriptor layouts");
            if vb.stride % STRIDE_GRANULARITY != 0 {
                error!(
                    "Stride ({}) must be a multiple of {}",
                    vb.stride, STRIDE_GRANULARITY
                );
                return Err(pso::CreationError::Other);
            }
            if vb.stride != 0 {
                mtl_buffer_desc.set_stride(vb.stride as u64);
                match vb.rate {
                    VertexInputRate::Vertex => {
                        mtl_buffer_desc.set_step_function(MTLVertexStepFunction::PerVertex);
                    }
                    VertexInputRate::Instance(divisor) => {
                        mtl_buffer_desc.set_step_function(MTLVertexStepFunction::PerInstance);
                        mtl_buffer_desc.set_step_rate(divisor as u64);
                    }
                }
            } else {
                mtl_buffer_desc.set_stride(256); // big enough to fit all the elements
                mtl_buffer_desc.set_step_function(MTLVertexStepFunction::PerInstance);
                mtl_buffer_desc.set_step_rate(!0);
            }
        }
        if !vertex_buffers.is_empty() {
            pipeline.set_vertex_descriptor(Some(&vertex_descriptor));
        }

        if let pso::State::Static(w) = pipeline_desc.rasterizer.line_width {
            if w != 1.0 {
                warn!("Unsupported line width: {:?}", w);
            }
        }

        let rasterizer_state = Some(n::RasterizerState {
            front_winding: conv::map_winding(pipeline_desc.rasterizer.front_face),
            fill_mode: conv::map_polygon_mode(pipeline_desc.rasterizer.polygon_mode),
            cull_mode: match conv::map_cull_face(pipeline_desc.rasterizer.cull_face) {
                Some(mode) => mode,
                None => {
                    //TODO - Metal validation fails with
                    // RasterizationEnabled is false but the vertex shader's return type is not void
                    error!("Culling both sides is not yet supported");
                    //pipeline.set_rasterization_enabled(false);
                    metal::MTLCullMode::None
                }
            },
            depth_clip: if self.shared.private_caps.depth_clip_mode {
                Some(if pipeline_desc.rasterizer.depth_clamping {
                    metal::MTLDepthClipMode::Clamp
                } else {
                    metal::MTLDepthClipMode::Clip
                })
            } else {
                None
            },
        });
        let depth_bias = pipeline_desc
            .rasterizer
            .depth_bias
            .unwrap_or(pso::State::Static(pso::DepthBias::default()));

        // prepare the depth-stencil state now
        let device = self.shared.device.lock();
        self.shared
            .service_pipes
            .depth_stencil_states
            .prepare(&pipeline_desc.depth_stencil, &*device);

        if let Some(multisampling) = &pipeline_desc.multisampling {
            pipeline.set_sample_count(multisampling.rasterization_samples as u64);
            pipeline.set_alpha_to_coverage_enabled(multisampling.alpha_coverage);
            pipeline.set_alpha_to_one_enabled(multisampling.alpha_to_one);
            // TODO: sample_mask
            // TODO: sample_shading
        }

        device
            .new_render_pipeline_state(&pipeline)
            .map(|raw| n::GraphicsPipeline {
                vs_lib,
                fs_lib,
                raw,
                primitive_type,
                vs_pc_info: pipeline_desc.layout.push_constants.vs,
                ps_pc_info: pipeline_desc.layout.push_constants.ps,
                rasterizer_state,
                depth_bias,
                depth_stencil_desc: pipeline_desc.depth_stencil.clone(),
                baked_states: pipeline_desc.baked_states.clone(),
                vertex_buffers,
                attachment_formats: subpass.target_formats.clone(),
            })
            .map_err(|err| {
                error!("PSO creation failed: {}", err);
                pso::CreationError::Other
            })
    }

    unsafe fn create_compute_pipeline<'a>(
        &self,
        pipeline_desc: &pso::ComputePipelineDesc<'a, Backend>,
        cache: Option<&n::PipelineCache>,
    ) -> Result<n::ComputePipeline, pso::CreationError> {
        debug!("create_compute_pipeline {:?}", pipeline_desc);
        let pipeline = metal::ComputePipelineDescriptor::new();

        let (cs_lib, cs_function, work_group_size, _) = self.load_shader(
            &pipeline_desc.shader,
            &pipeline_desc.layout,
            MTLPrimitiveTopologyClass::Unspecified,
            cache,
        )?;
        pipeline.set_compute_function(Some(&cs_function));

        self.shared
            .device
            .lock()
            .new_compute_pipeline_state(&pipeline)
            .map(|raw| n::ComputePipeline {
                cs_lib,
                raw,
                work_group_size,
                pc_info: pipeline_desc.layout.push_constants.cs,
            })
            .map_err(|err| {
                error!("PSO creation failed: {}", err);
                pso::CreationError::Other
            })
    }

    unsafe fn create_framebuffer<I>(
        &self,
        _render_pass: &n::RenderPass,
        attachments: I,
        extent: image::Extent,
    ) -> Result<n::Framebuffer, OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<n::ImageView>,
    {
        Ok(n::Framebuffer {
            extent,
            attachments: attachments
                .into_iter()
                .map(|at| at.borrow().texture.clone())
                .collect(),
        })
    }

    unsafe fn create_shader_module(
        &self,
        raw_data: &[u32],
    ) -> Result<n::ShaderModule, ShaderError> {
        //TODO: we can probably at least parse here and save the `Ast`
        let depends_on_pipeline_layout = true; //TODO: !self.private_caps.argument_buffers

        // TODO: also depends on pipeline layout if there are specialization constants that
        // SPIRV-Cross generates macros for, which occurs when MSL version is older than 1.2 or the
        // constant is used as an array size (see
        // `CompilerMSL::emit_specialization_constants_and_structs` in SPIRV-Cross)
        Ok(if depends_on_pipeline_layout {
            n::ShaderModule::Raw(raw_data.to_vec())
        } else {
            let mut options = msl::CompilerOptions::default();
            options.enable_point_size_builtin = false;
            options.vertex.invert_y = !self.features.contains(hal::Features::NDC_Y_UP);
            let info = Self::compile_shader_library(
                &self.shared.device,
                raw_data,
                &options,
                self.shared.private_caps.msl_version,
                &pso::Specialization::default(), // we should only pass empty specialization constants
                                                 // here if we know they won't be used by
                                                 // SPIRV-Cross, see above
            )?;
            n::ShaderModule::Compiled(info)
        })
    }

    unsafe fn create_sampler(
        &self,
        info: &image::SamplerDesc,
    ) -> Result<n::Sampler, AllocationError> {
        Ok(n::Sampler {
            raw: match self.make_sampler_descriptor(&info) {
                Some(ref descriptor) => Some(self.shared.device.lock().new_sampler(descriptor)),
                None => None,
            },
            data: Self::make_sampler_data(&info),
        })
    }

    unsafe fn destroy_sampler(&self, _sampler: n::Sampler) {}

    unsafe fn map_memory(
        &self,
        memory: &n::Memory,
        segment: memory::Segment,
    ) -> Result<*mut u8, MapError> {
        let range = memory.resolve(&segment);
        debug!("map_memory of size {} at {:?}", memory.size, range);

        let base_ptr = match memory.heap {
            n::MemoryHeap::Public(_, ref cpu_buffer) => cpu_buffer.contents() as *mut u8,
            n::MemoryHeap::Native(_) | n::MemoryHeap::Private => panic!("Unable to map memory!"),
        };
        Ok(base_ptr.offset(range.start as _))
    }

    unsafe fn unmap_memory(&self, memory: &n::Memory) {
        debug!("unmap_memory of size {}", memory.size);
    }

    unsafe fn flush_mapped_memory_ranges<'a, I>(&self, iter: I) -> Result<(), OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<(&'a n::Memory, memory::Segment)>,
    {
        debug!("flush_mapped_memory_ranges");
        for item in iter {
            let (memory, ref segment) = *item.borrow();
            let range = memory.resolve(segment);
            debug!("\trange {:?}", range);

            match memory.heap {
                n::MemoryHeap::Native(_) => unimplemented!(),
                n::MemoryHeap::Public(mt, ref cpu_buffer)
                    if 1 << mt.0 != MemoryTypes::SHARED.bits() as usize =>
                {
                    cpu_buffer.did_modify_range(NSRange {
                        location: range.start as _,
                        length: (range.end - range.start) as _,
                    });
                }
                n::MemoryHeap::Public(..) => continue,
                n::MemoryHeap::Private => panic!("Can't map private memory!"),
            };
        }

        Ok(())
    }

    unsafe fn invalidate_mapped_memory_ranges<'a, I>(&self, iter: I) -> Result<(), OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<(&'a n::Memory, memory::Segment)>,
    {
        let mut num_syncs = 0;
        debug!("invalidate_mapped_memory_ranges");

        // temporary command buffer to copy the contents from
        // the given buffers into the allocated CPU-visible buffers
        // Note: using a separate internal queue in order to avoid a stall
        let cmd_buffer = self.invalidation_queue.spawn_temp();
        autoreleasepool(|| {
            let encoder = cmd_buffer.new_blit_command_encoder();

            for item in iter {
                let (memory, ref segment) = *item.borrow();
                let range = memory.resolve(segment);
                debug!("\trange {:?}", range);

                match memory.heap {
                    n::MemoryHeap::Native(_) => unimplemented!(),
                    n::MemoryHeap::Public(mt, ref cpu_buffer)
                        if 1 << mt.0 != MemoryTypes::SHARED.bits() as usize =>
                    {
                        num_syncs += 1;
                        encoder.synchronize_resource(cpu_buffer);
                    }
                    n::MemoryHeap::Public(..) => continue,
                    n::MemoryHeap::Private => panic!("Can't map private memory!"),
                };
            }
            encoder.end_encoding();
        });

        if num_syncs != 0 {
            debug!("\twaiting...");
            cmd_buffer.set_label("invalidate_mapped_memory_ranges");
            cmd_buffer.commit();
            cmd_buffer.wait_until_completed();
        }

        Ok(())
    }

    fn create_semaphore(&self) -> Result<n::Semaphore, OutOfMemory> {
        Ok(n::Semaphore {
            // Semaphore synchronization between command buffers of the same queue
            // is useless, don't bother even creating one.
            system: if self.shared.private_caps.exposed_queues > 1 {
                Some(n::SystemSemaphore::new())
            } else {
                None
            },
            image_ready: Arc::new(Mutex::new(None)),
        })
    }

    unsafe fn create_descriptor_pool<I>(
        &self,
        max_sets: usize,
        descriptor_ranges: I,
        _flags: pso::DescriptorPoolCreateFlags,
    ) -> Result<n::DescriptorPool, OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorRangeDesc>,
    {
        if self.shared.private_caps.argument_buffers {
            let mut arguments = n::ArgumentArray::default();
            for desc_range in descriptor_ranges {
                let dr = desc_range.borrow();
                let content = n::DescriptorContent::from(dr.ty);
                let usage = n::ArgumentArray::describe_usage(dr.ty);
                if content.contains(n::DescriptorContent::BUFFER) {
                    arguments.push(metal::MTLDataType::Pointer, dr.count, usage);
                }
                if content.contains(n::DescriptorContent::TEXTURE) {
                    arguments.push(metal::MTLDataType::Texture, dr.count, usage);
                }
                if content.contains(n::DescriptorContent::SAMPLER) {
                    arguments.push(metal::MTLDataType::Sampler, dr.count, usage);
                }
            }

            let device = self.shared.device.lock();
            let (array_ref, total_resources) = arguments.build();
            let encoder = device.new_argument_encoder(array_ref);

            let alignment = self.shared.private_caps.buffer_alignment;
            let total_size = encoder.encoded_length() + (max_sets as u64) * alignment;
            let raw = device.new_buffer(total_size, MTLResourceOptions::empty());

            Ok(n::DescriptorPool::new_argument(
                raw,
                total_size,
                alignment,
                total_resources,
            ))
        } else {
            let mut counters = n::ResourceData::<n::PoolResourceIndex>::new();
            for desc_range in descriptor_ranges {
                let dr = desc_range.borrow();
                counters.add_many(
                    n::DescriptorContent::from(dr.ty),
                    dr.count as pso::DescriptorBinding,
                );
            }
            Ok(n::DescriptorPool::new_emulated(counters))
        }
    }

    unsafe fn create_descriptor_set_layout<I, J>(
        &self,
        binding_iter: I,
        immutable_samplers: J,
    ) -> Result<n::DescriptorSetLayout, OutOfMemory>
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorSetLayoutBinding>,
        J: IntoIterator,
        J::Item: Borrow<n::Sampler>,
    {
        if self.shared.private_caps.argument_buffers {
            let mut stage_flags = pso::ShaderStageFlags::empty();
            let mut arguments = n::ArgumentArray::default();
            let mut bindings = FastHashMap::default();
            for desc in binding_iter {
                let desc = desc.borrow();
                //TODO: have the API providing the dimensions and MSAA flag
                // for textures in an argument buffer
                match desc.ty {
                    pso::DescriptorType::Buffer {
                        format:
                            pso::BufferDescriptorFormat::Structured {
                                dynamic_offset: true,
                            },
                        ..
                    } => {
                        //TODO: apply the offsets somehow at the binding time
                        error!("Dynamic offsets are not yet supported in argument buffers!");
                    }
                    pso::DescriptorType::Image {
                        ty: pso::ImageDescriptorType::Storage { .. },
                    }
                    | pso::DescriptorType::Buffer {
                        ty: pso::BufferDescriptorType::Storage { .. },
                        format: pso::BufferDescriptorFormat::Texel,
                    } => {
                        //TODO: bind storage buffers and images separately
                        error!("Storage images are not yet supported in argument buffers!");
                    }
                    _ => {}
                }

                stage_flags |= desc.stage_flags;
                let content = n::DescriptorContent::from(desc.ty);
                let usage = n::ArgumentArray::describe_usage(desc.ty);
                let res = msl::ResourceBinding {
                    buffer_id: if content.contains(n::DescriptorContent::BUFFER) {
                        arguments.push(metal::MTLDataType::Pointer, desc.count, usage) as u32
                    } else {
                        !0
                    },
                    texture_id: if content.contains(n::DescriptorContent::TEXTURE) {
                        arguments.push(metal::MTLDataType::Texture, desc.count, usage) as u32
                    } else {
                        !0
                    },
                    sampler_id: if content.contains(n::DescriptorContent::SAMPLER) {
                        arguments.push(metal::MTLDataType::Sampler, desc.count, usage) as u32
                    } else {
                        !0
                    },
                };
                let res_offset = res.buffer_id.min(res.texture_id).min(res.sampler_id);
                bindings.insert(
                    desc.binding,
                    n::ArgumentLayout {
                        res,
                        res_offset,
                        count: desc.count,
                        usage,
                        content,
                    },
                );
            }

            let (array_ref, arg_total) = arguments.build();
            let encoder = self.shared.device.lock().new_argument_encoder(array_ref);

            Ok(n::DescriptorSetLayout::ArgumentBuffer {
                encoder,
                stage_flags,
                bindings: Arc::new(bindings),
                total: arg_total as n::PoolResourceIndex,
            })
        } else {
            struct TempSampler {
                data: msl::SamplerData,
                binding: pso::DescriptorBinding,
                array_index: pso::DescriptorArrayIndex,
            };
            let mut immutable_sampler_iter = immutable_samplers.into_iter();
            let mut tmp_samplers = Vec::new();
            let mut desc_layouts = Vec::new();

            for set_layout_binding in binding_iter {
                let slb = set_layout_binding.borrow();
                let mut content = n::DescriptorContent::from(slb.ty);

                if slb.immutable_samplers {
                    tmp_samplers.extend(
                        immutable_sampler_iter
                            .by_ref()
                            .take(slb.count)
                            .enumerate()
                            .map(|(array_index, sm)| TempSampler {
                                data: sm.borrow().data.clone(),
                                binding: slb.binding,
                                array_index,
                            }),
                    );
                    content |= n::DescriptorContent::IMMUTABLE_SAMPLER;
                }

                desc_layouts.extend((0 .. slb.count).map(|array_index| n::DescriptorLayout {
                    content,
                    stages: slb.stage_flags,
                    binding: slb.binding,
                    array_index,
                }));
            }

            desc_layouts.sort_by_key(|dl| (dl.binding, dl.array_index));
            tmp_samplers.sort_by_key(|ts| (ts.binding, ts.array_index));
            // From here on, we assume that `desc_layouts` has at most a single item for
            // a (binding, array_index) pair. To achieve that, we deduplicate the array now
            desc_layouts.dedup_by(|a, b| {
                if (a.binding, a.array_index) == (b.binding, b.array_index) {
                    debug_assert!(!b.stages.intersects(a.stages));
                    debug_assert_eq!(a.content, b.content); //TODO: double check if this can be demanded
                    b.stages |= a.stages; //`b` is here to stay
                    true
                } else {
                    false
                }
            });

            Ok(n::DescriptorSetLayout::Emulated(
                Arc::new(desc_layouts),
                tmp_samplers
                    .into_iter()
                    .map(|ts| (ts.binding, ts.data))
                    .collect(),
            ))
        }
    }

    unsafe fn write_descriptor_sets<'a, I, J>(&self, write_iter: I)
    where
        I: IntoIterator<Item = pso::DescriptorSetWrite<'a, Backend, J>>,
        J: IntoIterator,
        J::Item: Borrow<pso::Descriptor<'a, Backend>>,
    {
        debug!("write_descriptor_sets");
        for write in write_iter {
            match *write.set {
                n::DescriptorSet::Emulated {
                    ref pool,
                    ref layouts,
                    ref resources,
                } => {
                    let mut counters = resources.map(|r| r.start);
                    let mut start = None; //TODO: can pre-compute this
                    for (i, layout) in layouts.iter().enumerate() {
                        if layout.binding == write.binding
                            && layout.array_index == write.array_offset
                        {
                            start = Some(i);
                            break;
                        }
                        counters.add(layout.content);
                    }
                    let mut data = pool.write();

                    for (layout, descriptor) in
                        layouts[start.unwrap() ..].iter().zip(write.descriptors)
                    {
                        trace!("\t{:?}", layout);
                        match *descriptor.borrow() {
                            pso::Descriptor::Sampler(sam) => {
                                debug_assert!(!layout
                                    .content
                                    .contains(n::DescriptorContent::IMMUTABLE_SAMPLER));
                                data.samplers[counters.samplers as usize] =
                                    Some(AsNative::from(sam.raw.as_ref().unwrap().as_ref()));
                            }
                            pso::Descriptor::Image(view, il) => {
                                data.textures[counters.textures as usize] =
                                    Some((AsNative::from(view.texture.as_ref()), il));
                            }
                            pso::Descriptor::CombinedImageSampler(view, il, sam) => {
                                if !layout
                                    .content
                                    .contains(n::DescriptorContent::IMMUTABLE_SAMPLER)
                                {
                                    data.samplers[counters.samplers as usize] =
                                        Some(AsNative::from(sam.raw.as_ref().unwrap().as_ref()));
                                }
                                data.textures[counters.textures as usize] =
                                    Some((AsNative::from(view.texture.as_ref()), il));
                            }
                            pso::Descriptor::TexelBuffer(view) => {
                                data.textures[counters.textures as usize] = Some((
                                    AsNative::from(view.raw.as_ref()),
                                    image::Layout::General,
                                ));
                            }
                            pso::Descriptor::Buffer(buf, ref sub) => {
                                let (raw, range) = buf.as_bound();
                                debug_assert!(
                                    range.start + sub.offset + sub.size.unwrap_or(0) <= range.end
                                );
                                let pair = (AsNative::from(raw), range.start + sub.offset);
                                data.buffers[counters.buffers as usize] = Some(pair);
                            }
                        }
                        counters.add(layout.content);
                    }
                }
                n::DescriptorSet::ArgumentBuffer {
                    ref raw,
                    raw_offset,
                    ref pool,
                    ref range,
                    ref encoder,
                    ref bindings,
                    ..
                } => {
                    debug_assert!(self.shared.private_caps.argument_buffers);

                    encoder.set_argument_buffer(raw, raw_offset);
                    let mut arg_index = {
                        let binding = &bindings[&write.binding];
                        debug_assert!((write.array_offset as usize) < binding.count);
                        (binding.res_offset as NSUInteger) + (write.array_offset as NSUInteger)
                    };

                    for (data, descriptor) in pool.write().resources
                        [range.start as usize + arg_index as usize .. range.end as usize]
                        .iter_mut()
                        .zip(write.descriptors)
                    {
                        match *descriptor.borrow() {
                            pso::Descriptor::Sampler(sampler) => {
                                debug_assert!(!bindings[&write.binding]
                                    .content
                                    .contains(n::DescriptorContent::IMMUTABLE_SAMPLER));
                                encoder.set_sampler_state(arg_index, sampler.raw.as_ref().unwrap());
                                arg_index += 1;
                            }
                            pso::Descriptor::Image(image, _layout) => {
                                let tex_ref = image.texture.as_ref();
                                encoder.set_texture(arg_index, tex_ref);
                                data.ptr = (&**tex_ref).as_ptr();
                                arg_index += 1;
                            }
                            pso::Descriptor::CombinedImageSampler(image, _il, sampler) => {
                                let binding = &bindings[&write.binding];
                                if !binding
                                    .content
                                    .contains(n::DescriptorContent::IMMUTABLE_SAMPLER)
                                {
                                    //TODO: supporting arrays of combined image-samplers can be tricky.
                                    // We need to scan both sampler and image sections of the encoder
                                    // at the same time.
                                    assert!(
                                        arg_index
                                            < (binding.res_offset as NSUInteger)
                                                + (binding.count as NSUInteger)
                                    );
                                    encoder.set_sampler_state(
                                        arg_index + binding.count as NSUInteger,
                                        sampler.raw.as_ref().unwrap(),
                                    );
                                }
                                let tex_ref = image.texture.as_ref();
                                encoder.set_texture(arg_index, tex_ref);
                                data.ptr = (&**tex_ref).as_ptr();
                            }
                            pso::Descriptor::TexelBuffer(view) => {
                                encoder.set_texture(arg_index, &view.raw);
                                data.ptr = (&**view.raw).as_ptr();
                                arg_index += 1;
                            }
                            pso::Descriptor::Buffer(buffer, ref sub) => {
                                let (buf_raw, buf_range) = buffer.as_bound();
                                encoder.set_buffer(
                                    arg_index,
                                    buf_raw,
                                    buf_range.start + sub.offset,
                                );
                                data.ptr = (&**buf_raw).as_ptr();
                                arg_index += 1;
                            }
                        }
                    }
                }
            }
        }
    }

    unsafe fn copy_descriptor_sets<'a, I>(&self, copies: I)
    where
        I: IntoIterator,
        I::Item: Borrow<pso::DescriptorSetCopy<'a, Backend>>,
    {
        for _copy in copies {
            unimplemented!()
        }
    }

    unsafe fn destroy_descriptor_pool(&self, _pool: n::DescriptorPool) {}

    unsafe fn destroy_descriptor_set_layout(&self, _layout: n::DescriptorSetLayout) {}

    unsafe fn destroy_pipeline_layout(&self, _pipeline_layout: n::PipelineLayout) {}

    unsafe fn destroy_shader_module(&self, _module: n::ShaderModule) {}

    unsafe fn destroy_render_pass(&self, _pass: n::RenderPass) {}

    unsafe fn destroy_graphics_pipeline(&self, _pipeline: n::GraphicsPipeline) {}

    unsafe fn destroy_compute_pipeline(&self, _pipeline: n::ComputePipeline) {}

    unsafe fn destroy_framebuffer(&self, _buffer: n::Framebuffer) {}

    unsafe fn destroy_semaphore(&self, _semaphore: n::Semaphore) {}

    unsafe fn allocate_memory(
        &self,
        memory_type: hal::MemoryTypeId,
        size: u64,
    ) -> Result<n::Memory, AllocationError> {
        let (storage, cache) = MemoryTypes::describe(memory_type.0);
        let device = self.shared.device.lock();
        debug!("allocate_memory type {:?} of size {}", memory_type, size);

        // Heaps cannot be used for CPU coherent resources
        //TEMP: MacOS supports Private only, iOS and tvOS can do private/shared
        let heap = if self.shared.private_caps.resource_heaps
            && storage != MTLStorageMode::Shared
            && false
        {
            let descriptor = metal::HeapDescriptor::new();
            descriptor.set_storage_mode(storage);
            descriptor.set_cpu_cache_mode(cache);
            descriptor.set_size(size);
            let heap_raw = device.new_heap(&descriptor);
            n::MemoryHeap::Native(heap_raw)
        } else if storage == MTLStorageMode::Private {
            n::MemoryHeap::Private
        } else {
            let options = conv::resource_options_from_storage_and_cache(storage, cache);
            let cpu_buffer = device.new_buffer(size, options);
            debug!("\tbacked by cpu buffer {:?}", cpu_buffer.as_ptr());
            n::MemoryHeap::Public(memory_type, cpu_buffer)
        };

        Ok(n::Memory::new(heap, size))
    }

    unsafe fn free_memory(&self, memory: n::Memory) {
        debug!("free_memory of size {}", memory.size);
        if let n::MemoryHeap::Public(_, ref cpu_buffer) = memory.heap {
            debug!("\tbacked by cpu buffer {:?}", cpu_buffer.as_ptr());
        }
    }

    unsafe fn create_buffer(
        &self,
        size: u64,
        usage: buffer::Usage,
    ) -> Result<n::Buffer, buffer::CreationError> {
        debug!("create_buffer of size {} and usage {:?}", size, usage);
        Ok(n::Buffer::Unbound {
            usage,
            size,
            name: String::new(),
        })
    }

    unsafe fn get_buffer_requirements(&self, buffer: &n::Buffer) -> memory::Requirements {
        let (size, usage) = match *buffer {
            n::Buffer::Unbound { size, usage, .. } => (size, usage),
            n::Buffer::Bound { .. } => panic!("Unexpected Buffer::Bound"),
        };
        let mut max_size = size;
        let mut max_alignment = self.shared.private_caps.buffer_alignment;

        if self.shared.private_caps.resource_heaps {
            // We don't know what memory type the user will try to allocate the buffer with, so we test them
            // all get the most stringent ones.
            for (i, _mt) in self.memory_types.iter().enumerate() {
                let (storage, cache) = MemoryTypes::describe(i);
                let options = conv::resource_options_from_storage_and_cache(storage, cache);
                let requirements = self
                    .shared
                    .device
                    .lock()
                    .heap_buffer_size_and_align(size, options);
                max_size = cmp::max(max_size, requirements.size);
                max_alignment = cmp::max(max_alignment, requirements.align);
            }
        }

        // based on Metal validation error for view creation:
        // failed assertion `BytesPerRow of a buffer-backed texture with pixelFormat(XXX) must be aligned to 256 bytes
        const SIZE_MASK: u64 = 0xFF;
        let supports_texel_view =
            usage.intersects(buffer::Usage::UNIFORM_TEXEL | buffer::Usage::STORAGE_TEXEL);

        memory::Requirements {
            size: (max_size + SIZE_MASK) & !SIZE_MASK,
            alignment: max_alignment,
            type_mask: if !supports_texel_view || self.shared.private_caps.shared_textures {
                MemoryTypes::all().bits()
            } else {
                (MemoryTypes::all() ^ MemoryTypes::SHARED).bits()
            },
        }
    }

    unsafe fn bind_buffer_memory(
        &self,
        memory: &n::Memory,
        offset: u64,
        buffer: &mut n::Buffer,
    ) -> Result<(), BindError> {
        let (size, name) = match buffer {
            n::Buffer::Unbound { size, name, .. } => (*size, name),
            n::Buffer::Bound { .. } => panic!("Unexpected Buffer::Bound"),
        };
        debug!("bind_buffer_memory of size {} at offset {}", size, offset);
        *buffer = match memory.heap {
            n::MemoryHeap::Native(ref heap) => {
                let options = conv::resource_options_from_storage_and_cache(
                    heap.storage_mode(),
                    heap.cpu_cache_mode(),
                );
                let raw = heap.new_buffer(size, options).unwrap_or_else(|| {
                    // TODO: disable hazard tracking?
                    self.shared.device.lock().new_buffer(size, options)
                });
                raw.set_label(name);
                n::Buffer::Bound {
                    raw,
                    options,
                    range: 0 .. size, //TODO?
                }
            }
            n::MemoryHeap::Public(mt, ref cpu_buffer) => {
                debug!(
                    "\tmapped to public heap with address {:?}",
                    cpu_buffer.as_ptr()
                );
                let (storage, cache) = MemoryTypes::describe(mt.0);
                let options = conv::resource_options_from_storage_and_cache(storage, cache);
                if offset == 0x0 && size == cpu_buffer.length() {
                    cpu_buffer.set_label(name);
                } else if self.shared.private_caps.supports_debug_markers {
                    cpu_buffer.add_debug_marker(
                        name,
                        NSRange {
                            location: offset,
                            length: size,
                        },
                    );
                }
                n::Buffer::Bound {
                    raw: cpu_buffer.clone(),
                    options,
                    range: offset .. offset + size,
                }
            }
            n::MemoryHeap::Private => {
                //TODO: check for aliasing
                let options = MTLResourceOptions::StorageModePrivate
                    | MTLResourceOptions::CPUCacheModeDefaultCache;
                let raw = self.shared.device.lock().new_buffer(size, options);
                raw.set_label(name);
                n::Buffer::Bound {
                    raw,
                    options,
                    range: 0 .. size,
                }
            }
        };

        Ok(())
    }

    unsafe fn destroy_buffer(&self, buffer: n::Buffer) {
        if let n::Buffer::Bound { raw, range, .. } = buffer {
            debug!(
                "destroy_buffer {:?} occupying memory {:?}",
                raw.as_ptr(),
                range
            );
        }
    }

    unsafe fn create_buffer_view(
        &self,
        buffer: &n::Buffer,
        format_maybe: Option<format::Format>,
        sub: buffer::SubRange,
    ) -> Result<n::BufferView, buffer::ViewCreationError> {
        let (raw, base_range, options) = match *buffer {
            n::Buffer::Bound {
                ref raw,
                ref range,
                options,
            } => (raw, range, options),
            n::Buffer::Unbound { .. } => panic!("Unexpected Buffer::Unbound"),
        };
        let start = base_range.start + sub.offset;
        let size_rough = sub.size.unwrap_or(base_range.end - start);
        let format = match format_maybe {
            Some(fmt) => fmt,
            None => {
                return Err(buffer::ViewCreationError::UnsupportedFormat(format_maybe));
            }
        };
        let format_desc = format.surface_desc();
        if format_desc.aspects != format::Aspects::COLOR || format_desc.is_compressed() {
            // Vadlidator says "Linear texture: cannot create compressed, depth, or stencil textures"
            return Err(buffer::ViewCreationError::UnsupportedFormat(format_maybe));
        }

        //Note: we rely on SPIRV-Cross to use the proper 2D texel indexing here
        let texel_count = size_rough * 8 / format_desc.bits as u64;
        let col_count = cmp::min(texel_count, self.shared.private_caps.max_texture_size);
        let row_count = (texel_count + self.shared.private_caps.max_texture_size - 1)
            / self.shared.private_caps.max_texture_size;
        let mtl_format = self
            .shared
            .private_caps
            .map_format(format)
            .ok_or(buffer::ViewCreationError::UnsupportedFormat(format_maybe))?;

        let descriptor = metal::TextureDescriptor::new();
        descriptor.set_texture_type(MTLTextureType::D2);
        descriptor.set_width(col_count);
        descriptor.set_height(row_count);
        descriptor.set_mipmap_level_count(1);
        descriptor.set_pixel_format(mtl_format);
        descriptor.set_resource_options(options);
        descriptor.set_storage_mode(raw.storage_mode());
        descriptor.set_usage(metal::MTLTextureUsage::ShaderRead);

        let align_mask = self.shared.private_caps.buffer_alignment - 1;
        let stride = (col_count * (format_desc.bits as u64 / 8) + align_mask) & !align_mask;

        Ok(n::BufferView {
            raw: raw.new_texture_from_contents(&descriptor, start, stride),
        })
    }

    unsafe fn destroy_buffer_view(&self, _view: n::BufferView) {
        //nothing to do
    }

    unsafe fn create_image(
        &self,
        kind: image::Kind,
        mip_levels: image::Level,
        format: format::Format,
        tiling: image::Tiling,
        usage: image::Usage,
        view_caps: image::ViewCapabilities,
    ) -> Result<n::Image, image::CreationError> {
        debug!(
            "create_image {:?} with {} mips of {:?} {:?} and usage {:?}",
            kind, mip_levels, format, tiling, usage
        );

        let is_cube = view_caps.contains(image::ViewCapabilities::KIND_CUBE);
        let mtl_format = self
            .shared
            .private_caps
            .map_format(format)
            .ok_or_else(|| image::CreationError::Format(format))?;

        let descriptor = metal::TextureDescriptor::new();

        let (mtl_type, num_layers) = match kind {
            image::Kind::D1(_, 1) => {
                assert!(!is_cube);
                (MTLTextureType::D1, None)
            }
            image::Kind::D1(_, layers) => {
                assert!(!is_cube);
                (MTLTextureType::D1Array, Some(layers))
            }
            image::Kind::D2(_, _, layers, 1) => {
                if is_cube && layers > 6 {
                    assert_eq!(layers % 6, 0);
                    (MTLTextureType::CubeArray, Some(layers / 6))
                } else if is_cube {
                    assert_eq!(layers, 6);
                    (MTLTextureType::Cube, None)
                } else if layers > 1 {
                    (MTLTextureType::D2Array, Some(layers))
                } else {
                    (MTLTextureType::D2, None)
                }
            }
            image::Kind::D2(_, _, 1, samples) if !is_cube => {
                descriptor.set_sample_count(samples as u64);
                (MTLTextureType::D2Multisample, None)
            }
            image::Kind::D2(..) => {
                error!(
                    "Multi-sampled array textures or cubes are not supported: {:?}",
                    kind
                );
                return Err(image::CreationError::Kind);
            }
            image::Kind::D3(..) => {
                assert!(!is_cube && !view_caps.contains(image::ViewCapabilities::KIND_2D_ARRAY));
                (MTLTextureType::D3, None)
            }
        };

        descriptor.set_texture_type(mtl_type);
        if let Some(count) = num_layers {
            descriptor.set_array_length(count as u64);
        }
        let extent = kind.extent();
        descriptor.set_width(extent.width as u64);
        descriptor.set_height(extent.height as u64);
        descriptor.set_depth(extent.depth as u64);
        descriptor.set_mipmap_level_count(mip_levels as u64);
        descriptor.set_pixel_format(mtl_format);
        descriptor.set_usage(conv::map_texture_usage(usage, tiling));

        let base = format.base_format();
        let format_desc = base.0.desc();
        let mip_sizes = (0 .. mip_levels)
            .map(|level| {
                let pitches = n::Image::pitches_impl(extent.at_level(level), format_desc);
                num_layers.unwrap_or(1) as buffer::Offset * pitches[3]
            })
            .collect();

        let host_usage = image::Usage::TRANSFER_SRC | image::Usage::TRANSFER_DST;
        let host_visible = mtl_type == MTLTextureType::D2
            && mip_levels == 1
            && num_layers.is_none()
            && format_desc.aspects.contains(format::Aspects::COLOR)
            && tiling == image::Tiling::Linear
            && host_usage.contains(usage);

        Ok(n::Image {
            like: n::ImageLike::Unbound {
                descriptor,
                mip_sizes,
                host_visible,
                name: String::new(),
            },
            kind,
            format_desc,
            shader_channel: base.1.into(),
            mtl_format,
            mtl_type,
        })
    }

    unsafe fn get_image_requirements(&self, image: &n::Image) -> memory::Requirements {
        let (descriptor, mip_sizes, host_visible) = match image.like {
            n::ImageLike::Unbound {
                ref descriptor,
                ref mip_sizes,
                host_visible,
                ..
            } => (descriptor, mip_sizes, host_visible),
            n::ImageLike::Texture(..) | n::ImageLike::Buffer(..) => {
                panic!("Expected Image::Unbound")
            }
        };

        if self.shared.private_caps.resource_heaps {
            // We don't know what memory type the user will try to allocate the image with, so we test them
            // all get the most stringent ones. Note we don't check Shared because heaps can't use it
            let mut max_size = 0;
            let mut max_alignment = 0;
            let types = if host_visible {
                MemoryTypes::all()
            } else {
                MemoryTypes::PRIVATE
            };
            for (i, _) in self.memory_types.iter().enumerate() {
                if !types.contains(MemoryTypes::from_bits(1 << i).unwrap()) {
                    continue;
                }
                let (storage, cache_mode) = MemoryTypes::describe(i);
                descriptor.set_storage_mode(storage);
                descriptor.set_cpu_cache_mode(cache_mode);

                let requirements = self
                    .shared
                    .device
                    .lock()
                    .heap_texture_size_and_align(descriptor);
                max_size = cmp::max(max_size, requirements.size);
                max_alignment = cmp::max(max_alignment, requirements.align);
            }
            memory::Requirements {
                size: max_size,
                alignment: max_alignment,
                type_mask: types.bits(),
            }
        } else if host_visible {
            assert_eq!(mip_sizes.len(), 1);
            let mask = self.shared.private_caps.buffer_alignment - 1;
            memory::Requirements {
                size: (mip_sizes[0] + mask) & !mask,
                alignment: self.shared.private_caps.buffer_alignment,
                type_mask: MemoryTypes::all().bits(),
            }
        } else {
            memory::Requirements {
                size: mip_sizes.iter().sum(),
                alignment: 4,
                type_mask: MemoryTypes::PRIVATE.bits(),
            }
        }
    }

    unsafe fn get_image_subresource_footprint(
        &self,
        image: &n::Image,
        sub: image::Subresource,
    ) -> image::SubresourceFootprint {
        let num_layers = image.kind.num_layers() as buffer::Offset;
        let level_offset = (0 .. sub.level).fold(0, |offset, level| {
            let pitches = image.pitches(level);
            offset + num_layers * pitches[3]
        });
        let pitches = image.pitches(sub.level);
        let layer_offset = level_offset + sub.layer as buffer::Offset * pitches[3];
        image::SubresourceFootprint {
            slice: layer_offset .. layer_offset + pitches[3],
            row_pitch: pitches[1] as _,
            depth_pitch: pitches[2] as _,
            array_pitch: pitches[3] as _,
        }
    }

    unsafe fn bind_image_memory(
        &self,
        memory: &n::Memory,
        offset: u64,
        image: &mut n::Image,
    ) -> Result<(), BindError> {
        let like = {
            let (descriptor, mip_sizes, name) = match image.like {
                n::ImageLike::Unbound {
                    ref descriptor,
                    ref mip_sizes,
                    ref name,
                    ..
                } => (descriptor, mip_sizes, name),
                n::ImageLike::Texture(..) | n::ImageLike::Buffer(..) => {
                    panic!("Expected Image::Unbound")
                }
            };

            match memory.heap {
                n::MemoryHeap::Native(ref heap) => {
                    let resource_options = conv::resource_options_from_storage_and_cache(
                        heap.storage_mode(),
                        heap.cpu_cache_mode(),
                    );
                    descriptor.set_resource_options(resource_options);
                    n::ImageLike::Texture(heap.new_texture(descriptor).unwrap_or_else(|| {
                        // TODO: disable hazard tracking?
                        let texture = self.shared.device.lock().new_texture(&descriptor);
                        texture.set_label(name);
                        texture
                    }))
                }
                n::MemoryHeap::Public(_memory_type, ref cpu_buffer) => {
                    assert_eq!(mip_sizes.len(), 1);
                    if offset == 0x0 && cpu_buffer.length() == mip_sizes[0] {
                        cpu_buffer.set_label(name);
                    } else if self.shared.private_caps.supports_debug_markers {
                        cpu_buffer.add_debug_marker(
                            name,
                            NSRange {
                                location: offset,
                                length: mip_sizes[0],
                            },
                        );
                    }
                    n::ImageLike::Buffer(n::Buffer::Bound {
                        raw: cpu_buffer.clone(),
                        range: offset .. offset + mip_sizes[0] as u64,
                        options: MTLResourceOptions::StorageModeShared,
                    })
                }
                n::MemoryHeap::Private => {
                    descriptor.set_storage_mode(MTLStorageMode::Private);
                    let texture = self.shared.device.lock().new_texture(descriptor);
                    texture.set_label(name);
                    n::ImageLike::Texture(texture)
                }
            }
        };

        Ok(image.like = like)
    }

    unsafe fn destroy_image(&self, _image: n::Image) {
        //nothing to do
    }

    unsafe fn create_image_view(
        &self,
        image: &n::Image,
        kind: image::ViewKind,
        format: format::Format,
        swizzle: format::Swizzle,
        range: image::SubresourceRange,
    ) -> Result<n::ImageView, image::ViewCreationError> {
        let mtl_format = match self
            .shared
            .private_caps
            .map_format_with_swizzle(format, swizzle)
        {
            Some(f) => f,
            None => {
                error!("failed to swizzle format {:?} with {:?}", format, swizzle);
                return Err(image::ViewCreationError::BadFormat(format));
            }
        };
        let raw = image.like.as_texture();
        let full_range = image::SubresourceRange {
            aspects: image.format_desc.aspects,
            levels: 0 .. raw.mipmap_level_count() as image::Level,
            layers: 0 .. image.kind.num_layers(),
        };
        let mtl_type = if image.mtl_type == MTLTextureType::D2Multisample {
            if kind != image::ViewKind::D2 {
                error!("Requested {:?} for MSAA texture", kind);
            }
            image.mtl_type
        } else {
            conv::map_texture_type(kind)
        };

        let texture = if mtl_format == image.mtl_format
            && mtl_type == image.mtl_type
            && swizzle == format::Swizzle::NO
            && range == full_range
        {
            // Some images are marked as framebuffer-only, and we can't create aliases of them.
            // Also helps working around Metal bugs with aliased array textures.
            raw.to_owned()
        } else {
            raw.new_texture_view_from_slice(
                mtl_format,
                mtl_type,
                NSRange {
                    location: range.levels.start as _,
                    length: (range.levels.end - range.levels.start) as _,
                },
                NSRange {
                    location: range.layers.start as _,
                    length: (range.layers.end - range.layers.start) as _,
                },
            )
        };

        Ok(n::ImageView {
            texture,
            mtl_format,
        })
    }

    unsafe fn destroy_image_view(&self, _view: n::ImageView) {}

    fn create_fence(&self, signaled: bool) -> Result<n::Fence, OutOfMemory> {
        let cell = RefCell::new(n::FenceInner::Idle { signaled });
        debug!(
            "Creating fence ptr {:?} with signal={}",
            cell.as_ptr(),
            signaled
        );
        Ok(n::Fence(cell))
    }

    unsafe fn reset_fence(&self, fence: &n::Fence) -> Result<(), OutOfMemory> {
        debug!("Resetting fence ptr {:?}", fence.0.as_ptr());
        fence.0.replace(n::FenceInner::Idle { signaled: false });
        Ok(())
    }

    unsafe fn wait_for_fence(
        &self,
        fence: &n::Fence,
        timeout_ns: u64,
    ) -> Result<bool, OomOrDeviceLost> {
        unsafe fn to_ns(duration: time::Duration) -> u64 {
            duration.as_secs() * 1_000_000_000 + duration.subsec_nanos() as u64
        }

        debug!("wait_for_fence {:?} for {} ms", fence, timeout_ns);
        match *fence.0.borrow() {
            n::FenceInner::Idle { signaled } => {
                if !signaled {
                    warn!(
                        "Fence ptr {:?} is not pending, waiting not possible",
                        fence.0.as_ptr()
                    );
                }
                Ok(signaled)
            }
            n::FenceInner::PendingSubmission(ref cmd_buf) => {
                if timeout_ns == !0 {
                    cmd_buf.wait_until_completed();
                    return Ok(true);
                }
                let start = time::Instant::now();
                loop {
                    if let metal::MTLCommandBufferStatus::Completed = cmd_buf.status() {
                        return Ok(true);
                    }
                    if to_ns(start.elapsed()) >= timeout_ns {
                        return Ok(false);
                    }
                    thread::sleep(time::Duration::from_millis(1));
                    self.shared.queue_blocker.lock().triage();
                }
            }
            n::FenceInner::AcquireFrame {
                ref swapchain_image,
                iteration,
            } => {
                if swapchain_image.iteration() > iteration {
                    Ok(true)
                } else if timeout_ns == 0 {
                    Ok(false)
                } else {
                    swapchain_image.wait_until_ready();
                    Ok(true)
                }
            }
        }
    }

    unsafe fn get_fence_status(&self, fence: &n::Fence) -> Result<bool, DeviceLost> {
        Ok(match *fence.0.borrow() {
            n::FenceInner::Idle { signaled } => signaled,
            n::FenceInner::PendingSubmission(ref cmd_buf) => match cmd_buf.status() {
                metal::MTLCommandBufferStatus::Completed => true,
                _ => false,
            },
            n::FenceInner::AcquireFrame {
                ref swapchain_image,
                iteration,
            } => swapchain_image.iteration() > iteration,
        })
    }

    unsafe fn destroy_fence(&self, _fence: n::Fence) {
        //empty
    }

    fn create_event(&self) -> Result<n::Event, OutOfMemory> {
        Ok(n::Event(Arc::new(AtomicBool::new(false))))
    }

    unsafe fn get_event_status(&self, event: &n::Event) -> Result<bool, OomOrDeviceLost> {
        Ok(event.0.load(Ordering::Acquire))
    }

    unsafe fn set_event(&self, event: &n::Event) -> Result<(), OutOfMemory> {
        event.0.store(true, Ordering::Release);
        self.shared.queue_blocker.lock().triage();
        Ok(())
    }

    unsafe fn reset_event(&self, event: &n::Event) -> Result<(), OutOfMemory> {
        Ok(event.0.store(false, Ordering::Release))
    }

    unsafe fn destroy_event(&self, _event: n::Event) {
        //empty
    }

    unsafe fn create_query_pool(
        &self,
        ty: query::Type,
        count: query::Id,
    ) -> Result<n::QueryPool, query::CreationError> {
        match ty {
            query::Type::Occlusion => {
                let range = self
                    .shared
                    .visibility
                    .allocator
                    .lock()
                    .allocate_range(count)
                    .map_err(|_| {
                        error!("Not enough space to allocate an occlusion query pool");
                        OutOfMemory::Host
                    })?;
                Ok(n::QueryPool::Occlusion(range))
            }
            _ => {
                error!("Only occlusion queries are currently supported");
                Err(query::CreationError::Unsupported(ty))
            }
        }
    }

    unsafe fn destroy_query_pool(&self, pool: n::QueryPool) {
        match pool {
            n::QueryPool::Occlusion(range) => {
                self.shared.visibility.allocator.lock().free_range(range);
            }
        }
    }

    unsafe fn get_query_pool_results(
        &self,
        pool: &n::QueryPool,
        queries: Range<query::Id>,
        data: &mut [u8],
        stride: buffer::Offset,
        flags: query::ResultFlags,
    ) -> Result<bool, OomOrDeviceLost> {
        let is_ready = match *pool {
            n::QueryPool::Occlusion(ref pool_range) => {
                let visibility = &self.shared.visibility;
                let is_ready = if flags.contains(query::ResultFlags::WAIT) {
                    let mut guard = visibility.allocator.lock();
                    while !visibility.are_available(pool_range.start, &queries) {
                        visibility.condvar.wait(&mut guard);
                    }
                    true
                } else {
                    visibility.are_available(pool_range.start, &queries)
                };

                let size_data = mem::size_of::<u64>() as buffer::Offset;
                if stride == size_data
                    && flags.contains(query::ResultFlags::BITS_64)
                    && !flags.contains(query::ResultFlags::WITH_AVAILABILITY)
                {
                    // if stride is matching, copy everything in one go
                    ptr::copy_nonoverlapping(
                        (visibility.buffer.contents() as *const u8).offset(
                            (pool_range.start + queries.start) as isize * size_data as isize,
                        ),
                        data.as_mut_ptr(),
                        stride as usize * (queries.end - queries.start) as usize,
                    );
                } else {
                    // copy parts of individual entries
                    for i in 0 .. queries.end - queries.start {
                        let absolute_index = (pool_range.start + queries.start + i) as isize;
                        let value =
                            *(visibility.buffer.contents() as *const u64).offset(absolute_index);
                        let base = (visibility.buffer.contents() as *const u8)
                            .offset(visibility.availability_offset as isize);
                        let availability = *(base as *const u32).offset(absolute_index);
                        let data_ptr = data[i as usize * stride as usize ..].as_mut_ptr();
                        if flags.contains(query::ResultFlags::BITS_64) {
                            *(data_ptr as *mut u64) = value;
                            if flags.contains(query::ResultFlags::WITH_AVAILABILITY) {
                                *(data_ptr as *mut u64).offset(1) = availability as u64;
                            }
                        } else {
                            *(data_ptr as *mut u32) = value as u32;
                            if flags.contains(query::ResultFlags::WITH_AVAILABILITY) {
                                *(data_ptr as *mut u32).offset(1) = availability;
                            }
                        }
                    }
                }

                is_ready
            }
        };

        Ok(is_ready)
    }

    unsafe fn create_swapchain(
        &self,
        surface: &mut Surface,
        config: window::SwapchainConfig,
        old_swapchain: Option<Swapchain>,
    ) -> Result<(Swapchain, Vec<n::Image>), window::CreationError> {
        Ok(self.build_swapchain(surface, config, old_swapchain))
    }

    unsafe fn destroy_swapchain(&self, _swapchain: Swapchain) {}

    fn wait_idle(&self) -> Result<(), OutOfMemory> {
        command::QueueInner::wait_idle(&self.shared.queue);
        Ok(())
    }

    unsafe fn set_image_name(&self, image: &mut n::Image, name: &str) {
        match image {
            n::Image {
                like: n::ImageLike::Buffer(ref mut buf),
                ..
            } => self.set_buffer_name(buf, name),
            n::Image {
                like: n::ImageLike::Texture(ref tex),
                ..
            } => tex.set_label(name),
            n::Image {
                like:
                    n::ImageLike::Unbound {
                        name: ref mut unbound_name,
                        ..
                    },
                ..
            } => {
                *unbound_name = name.to_string();
            }
        };
    }

    unsafe fn set_buffer_name(&self, buffer: &mut n::Buffer, name: &str) {
        match buffer {
            n::Buffer::Unbound {
                name: ref mut unbound_name,
                ..
            } => {
                *unbound_name = name.to_string();
            }
            n::Buffer::Bound {
                ref raw, ref range, ..
            } => {
                if self.shared.private_caps.supports_debug_markers {
                    raw.add_debug_marker(
                        name,
                        NSRange {
                            location: range.start,
                            length: range.end - range.start,
                        },
                    );
                }
            }
        }
    }

    unsafe fn set_command_buffer_name(
        &self,
        command_buffer: &mut command::CommandBuffer,
        name: &str,
    ) {
        command_buffer.name = name.to_string();
    }

    unsafe fn set_semaphore_name(&self, _semaphore: &mut n::Semaphore, _name: &str) {}

    unsafe fn set_fence_name(&self, _fence: &mut n::Fence, _name: &str) {}

    unsafe fn set_framebuffer_name(&self, _framebuffer: &mut n::Framebuffer, _name: &str) {}

    unsafe fn set_render_pass_name(&self, render_pass: &mut n::RenderPass, name: &str) {
        render_pass.name = name.to_string();
    }

    unsafe fn set_descriptor_set_name(&self, _descriptor_set: &mut n::DescriptorSet, _name: &str) {
        // TODO
    }

    unsafe fn set_descriptor_set_layout_name(
        &self,
        _descriptor_set_layout: &mut n::DescriptorSetLayout,
        _name: &str,
    ) {
        // TODO
    }
}

#[test]
fn test_send_sync() {
    fn foo<T: Send + Sync>() {}
    foo::<Device>()
}
