use crate::{conversions as conv, PrivateCapabilities};

use auxil::FastHashMap;
use hal::{
    command::ClearColor,
    format::{Aspects, ChannelType},
    image::Filter,
    pso,
};

use metal;
use parking_lot::{Mutex, RawRwLock};
use storage_map::{StorageMap, StorageMapGuard};

use std::mem;


pub type FastStorageMap<K, V> = StorageMap<RawRwLock, FastHashMap<K, V>>;
pub type FastStorageGuard<'a, V> = StorageMapGuard<'a, RawRwLock, V>;

#[derive(Clone, Debug)]
pub struct ClearVertex {
    pub pos: [f32; 4],
}

#[derive(Clone, Debug)]
pub struct BlitVertex {
    pub uv: [f32; 4],
    pub pos: [f32; 4],
}

#[derive(Debug, Clone, Copy, Hash, PartialEq, Eq)]
pub enum Channel {
    Float,
    Int,
    Uint,
}

impl From<ChannelType> for Channel {
    fn from(channel_type: ChannelType) -> Self {
        match channel_type {
            ChannelType::Unorm
            | ChannelType::Snorm
            | ChannelType::Ufloat
            | ChannelType::Sfloat
            | ChannelType::Uscaled
            | ChannelType::Sscaled
            | ChannelType::Srgb => Channel::Float,
            ChannelType::Uint => Channel::Uint,
            ChannelType::Sint => Channel::Int,
        }
    }
}

impl Channel {
    pub fn interpret(self, raw: ClearColor) -> metal::MTLClearColor {
        unsafe {
            match self {
                Channel::Float => metal::MTLClearColor::new(
                    raw.float32[0] as _,
                    raw.float32[1] as _,
                    raw.float32[2] as _,
                    raw.float32[3] as _,
                ),
                Channel::Int => metal::MTLClearColor::new(
                    raw.sint32[0] as _,
                    raw.sint32[1] as _,
                    raw.sint32[2] as _,
                    raw.sint32[3] as _,
                ),
                Channel::Uint => metal::MTLClearColor::new(
                    raw.uint32[0] as _,
                    raw.uint32[1] as _,
                    raw.uint32[2] as _,
                    raw.uint32[3] as _,
                ),
            }
        }
    }
}

#[derive(Debug)]
pub struct SamplerStates {
    nearest: metal::SamplerState,
    linear: metal::SamplerState,
}

impl SamplerStates {
    fn new(device: &metal::DeviceRef) -> Self {
        let desc = metal::SamplerDescriptor::new();
        desc.set_min_filter(metal::MTLSamplerMinMagFilter::Nearest);
        desc.set_mag_filter(metal::MTLSamplerMinMagFilter::Nearest);
        desc.set_mip_filter(metal::MTLSamplerMipFilter::Nearest);
        let nearest = device.new_sampler(&desc);
        desc.set_min_filter(metal::MTLSamplerMinMagFilter::Linear);
        desc.set_mag_filter(metal::MTLSamplerMinMagFilter::Linear);
        let linear = device.new_sampler(&desc);

        SamplerStates { nearest, linear }
    }

    pub fn get(&self, filter: Filter) -> &metal::SamplerStateRef {
        match filter {
            Filter::Nearest => &self.nearest,
            Filter::Linear => &self.linear,
        }
    }
}

#[derive(Debug)]
pub struct DepthStencilStates {
    map: FastStorageMap<pso::DepthStencilDesc, metal::DepthStencilState>,
    write_none: pso::DepthStencilDesc,
    write_depth: pso::DepthStencilDesc,
    write_stencil: pso::DepthStencilDesc,
    write_all: pso::DepthStencilDesc,
}

impl DepthStencilStates {
    fn new(device: &metal::DeviceRef) -> Self {
        let write_none = pso::DepthStencilDesc {
            depth: None,
            depth_bounds: false,
            stencil: None,
        };
        let write_depth = pso::DepthStencilDesc {
            depth: Some(pso::DepthTest {
                fun: pso::Comparison::Always,
                write: true,
            }),
            depth_bounds: false,
            stencil: None,
        };
        let face = pso::StencilFace {
            fun: pso::Comparison::Always,
            op_fail: pso::StencilOp::Replace,
            op_depth_fail: pso::StencilOp::Replace,
            op_pass: pso::StencilOp::Replace,
        };
        let write_stencil = pso::DepthStencilDesc {
            depth: None,
            depth_bounds: false,
            stencil: Some(pso::StencilTest {
                faces: pso::Sided::new(face),
                ..pso::StencilTest::default()
            }),
        };
        let write_all = pso::DepthStencilDesc {
            depth: Some(pso::DepthTest {
                fun: pso::Comparison::Always,
                write: true,
            }),
            depth_bounds: false,
            stencil: Some(pso::StencilTest {
                faces: pso::Sided::new(face),
                ..pso::StencilTest::default()
            }),
        };

        let map = FastStorageMap::default();
        for desc in &[&write_none, &write_depth, &write_stencil, &write_all] {
            map.get_or_create_with(*desc, || {
                let raw_desc = Self::create_desc(desc).unwrap();
                device.new_depth_stencil_state(&raw_desc)
            });
        }

        DepthStencilStates {
            map,
            write_none,
            write_depth,
            write_stencil,
            write_all,
        }
    }

    pub fn get_write(&self, aspects: Aspects) -> FastStorageGuard<metal::DepthStencilState> {
        let key = if aspects.contains(Aspects::DEPTH | Aspects::STENCIL) {
            &self.write_all
        } else if aspects.contains(Aspects::DEPTH) {
            &self.write_depth
        } else if aspects.contains(Aspects::STENCIL) {
            &self.write_stencil
        } else {
            &self.write_none
        };
        self.map.get_or_create_with(key, || unreachable!())
    }

    pub fn prepare(&self, desc: &pso::DepthStencilDesc, device: &metal::DeviceRef) {
        self.map.prepare_maybe(desc, || {
            Self::create_desc(desc).map(|raw_desc| device.new_depth_stencil_state(&raw_desc))
        });
    }

    // TODO: avoid locking for writes every time
    pub fn get(
        &self,
        desc: pso::DepthStencilDesc,
        device: &Mutex<metal::Device>,
    ) -> FastStorageGuard<metal::DepthStencilState> {
        self.map.get_or_create_with(&desc, || {
            let raw_desc = Self::create_desc(&desc).expect("Incomplete descriptor provided");
            device.lock().new_depth_stencil_state(&raw_desc)
        })
    }

    fn create_stencil(
        face: &pso::StencilFace,
        read_mask: pso::StencilValue,
        write_mask: pso::StencilValue,
    ) -> metal::StencilDescriptor {
        let desc = metal::StencilDescriptor::new();
        desc.set_stencil_compare_function(conv::map_compare_function(face.fun));
        desc.set_read_mask(read_mask);
        desc.set_write_mask(write_mask);
        desc.set_stencil_failure_operation(conv::map_stencil_op(face.op_fail));
        desc.set_depth_failure_operation(conv::map_stencil_op(face.op_depth_fail));
        desc.set_depth_stencil_pass_operation(conv::map_stencil_op(face.op_pass));
        desc
    }

    fn create_desc(desc: &pso::DepthStencilDesc) -> Option<metal::DepthStencilDescriptor> {
        let raw = metal::DepthStencilDescriptor::new();

        if let Some(ref stencil) = desc.stencil {
            let read_masks = match stencil.read_masks {
                pso::State::Static(value) => value,
                pso::State::Dynamic => return None,
            };
            let write_masks = match stencil.write_masks {
                pso::State::Static(value) => value,
                pso::State::Dynamic => return None,
            };
            let front_desc =
                Self::create_stencil(&stencil.faces.front, read_masks.front, write_masks.front);
            raw.set_front_face_stencil(Some(&front_desc));
            let back_desc = if stencil.faces.front == stencil.faces.back
                && read_masks.front == read_masks.back
                && write_masks.front == write_masks.back
            {
                front_desc
            } else {
                Self::create_stencil(&stencil.faces.back, read_masks.back, write_masks.back)
            };
            raw.set_back_face_stencil(Some(&back_desc));
        }

        if let Some(ref depth) = desc.depth {
            raw.set_depth_compare_function(conv::map_compare_function(depth.fun));
            raw.set_depth_write_enabled(depth.write);
        }

        Some(raw)
    }
}

#[derive(Debug, Clone, Copy, Hash, PartialEq, Eq)]
pub struct ClearKey {
    pub framebuffer_aspects: Aspects,
    pub color_formats: [metal::MTLPixelFormat; 1],
    pub depth_stencil_format: metal::MTLPixelFormat,
    pub target_index: Option<(u8, Channel)>,
}

#[derive(Debug)]
pub struct ImageClearPipes {
    map: FastStorageMap<ClearKey, metal::RenderPipelineState>,
}

impl ImageClearPipes {
    pub(crate) fn get(
        &self,
        key: ClearKey,
        library: &Mutex<metal::Library>,
        device: &Mutex<metal::Device>,
        private_caps: &PrivateCapabilities,
    ) -> FastStorageGuard<metal::RenderPipelineState> {
        self.map.get_or_create_with(&key, || {
            Self::create(key, &*library.lock(), &*device.lock(), private_caps)
        })
    }

    fn create(
        key: ClearKey,
        library: &metal::LibraryRef,
        device: &metal::DeviceRef,
        private_caps: &PrivateCapabilities,
    ) -> metal::RenderPipelineState {
        let pipeline = metal::RenderPipelineDescriptor::new();
        if private_caps.layered_rendering {
            pipeline.set_input_primitive_topology(metal::MTLPrimitiveTopologyClass::Triangle);
        }

        let vs_clear = library.get_function("vs_clear", None).unwrap();
        pipeline.set_vertex_function(Some(&vs_clear));

        if key.framebuffer_aspects.contains(Aspects::COLOR) {
            for (i, &format) in key.color_formats.iter().enumerate() {
                pipeline
                    .color_attachments()
                    .object_at(i)
                    .unwrap()
                    .set_pixel_format(format);
            }
        }
        if key.framebuffer_aspects.contains(Aspects::DEPTH) {
            pipeline.set_depth_attachment_pixel_format(key.depth_stencil_format);
        }
        if key.framebuffer_aspects.contains(Aspects::STENCIL) {
            pipeline.set_stencil_attachment_pixel_format(key.depth_stencil_format);
        }

        if let Some((index, channel)) = key.target_index {
            assert!(key.framebuffer_aspects.contains(Aspects::COLOR));
            let s_channel = match channel {
                Channel::Float => "float",
                Channel::Int => "int",
                Channel::Uint => "uint",
            };
            let ps_name = format!("ps_clear{}_{}", index, s_channel);
            let ps_fun = library.get_function(&ps_name, None).unwrap();
            pipeline.set_fragment_function(Some(&ps_fun));
        }

        // Vertex buffers
        let vertex_descriptor = metal::VertexDescriptor::new();
        let mtl_buffer_desc = vertex_descriptor.layouts().object_at(0).unwrap();
        mtl_buffer_desc.set_stride(mem::size_of::<ClearVertex>() as _);
        for i in 0 .. 1 {
            let mtl_attribute_desc = vertex_descriptor
                .attributes()
                .object_at(i)
                .expect("too many vertex attributes");
            mtl_attribute_desc.set_buffer_index(0);
            mtl_attribute_desc.set_offset((i * mem::size_of::<[f32; 4]>()) as _);
            mtl_attribute_desc.set_format(metal::MTLVertexFormat::Float4);
        }
        pipeline.set_vertex_descriptor(Some(&vertex_descriptor));

        device.new_render_pipeline_state(&pipeline).unwrap()
    }
}

pub type BlitKey = (
    metal::MTLTextureType,
    metal::MTLPixelFormat,
    Aspects,
    Channel,
);

#[derive(Debug)]
pub struct ImageBlitPipes {
    map: FastStorageMap<BlitKey, metal::RenderPipelineState>,
}

impl ImageBlitPipes {
    pub(crate) fn get(
        &self,
        key: BlitKey,
        library: &Mutex<metal::Library>,
        device: &Mutex<metal::Device>,
        private_caps: &PrivateCapabilities,
    ) -> FastStorageGuard<metal::RenderPipelineState> {
        self.map.get_or_create_with(&key, || {
            Self::create(key, &*library.lock(), &*device.lock(), private_caps)
        })
    }

    fn create(
        key: BlitKey,
        library: &metal::LibraryRef,
        device: &metal::DeviceRef,
        private_caps: &PrivateCapabilities,
    ) -> metal::RenderPipelineState {
        use metal::MTLTextureType as Tt;

        let pipeline = metal::RenderPipelineDescriptor::new();
        if private_caps.layered_rendering {
            pipeline.set_input_primitive_topology(metal::MTLPrimitiveTopologyClass::Triangle);
        }

        let s_type = match key.0 {
            Tt::D1 => "1d",
            Tt::D1Array => "1d_array",
            Tt::D2 => "2d",
            Tt::D2Array => "2d_array",
            Tt::D3 => "3d",
            Tt::D2Multisample => panic!("Can't blit MSAA surfaces"),
            Tt::Cube | Tt::CubeArray => unimplemented!(),
        };
        let s_channel = if key.2.contains(Aspects::COLOR) {
            match key.3 {
                Channel::Float => "float",
                Channel::Int => "int",
                Channel::Uint => "uint",
            }
        } else {
            "depth" //TODO: stencil
        };
        let ps_name = format!("ps_blit_{}_{}", s_type, s_channel);

        let vs_blit = library.get_function("vs_blit", None).unwrap();
        let ps_blit = library.get_function(&ps_name, None).unwrap();
        pipeline.set_vertex_function(Some(&vs_blit));
        pipeline.set_fragment_function(Some(&ps_blit));

        if key.2.contains(Aspects::COLOR) {
            pipeline
                .color_attachments()
                .object_at(0)
                .unwrap()
                .set_pixel_format(key.1);
        }
        if key.2.contains(Aspects::DEPTH) {
            pipeline.set_depth_attachment_pixel_format(key.1);
        }
        if key.2.contains(Aspects::STENCIL) {
            pipeline.set_stencil_attachment_pixel_format(key.1);
        }

        // Vertex buffers
        let vertex_descriptor = metal::VertexDescriptor::new();
        let mtl_buffer_desc = vertex_descriptor.layouts().object_at(0).unwrap();
        mtl_buffer_desc.set_stride(mem::size_of::<BlitVertex>() as _);
        for i in 0 .. 2 {
            let mtl_attribute_desc = vertex_descriptor
                .attributes()
                .object_at(i)
                .expect("too many vertex attributes");
            mtl_attribute_desc.set_buffer_index(0);
            mtl_attribute_desc.set_offset((i * mem::size_of::<[f32; 4]>()) as _);
            mtl_attribute_desc.set_format(metal::MTLVertexFormat::Float4);
        }
        pipeline.set_vertex_descriptor(Some(&vertex_descriptor));

        device.new_render_pipeline_state(&pipeline).unwrap()
    }
}

#[derive(Debug)]
pub struct ServicePipes {
    pub library: Mutex<metal::Library>,
    pub sampler_states: SamplerStates,
    pub depth_stencil_states: DepthStencilStates,
    pub clears: ImageClearPipes,
    pub blits: ImageBlitPipes,
    pub copy_buffer: metal::ComputePipelineState,
    pub fill_buffer: metal::ComputePipelineState,
}

impl ServicePipes {
    pub fn new(device: &metal::DeviceRef) -> Self {
        let data = include_bytes!("./../shaders/gfx_shaders.metallib");
        let library = device.new_library_with_data(data).unwrap();

        let copy_buffer = Self::create_copy_buffer(&library, device);
        let fill_buffer = Self::create_fill_buffer(&library, device);

        ServicePipes {
            library: Mutex::new(library),
            sampler_states: SamplerStates::new(device),
            depth_stencil_states: DepthStencilStates::new(device),
            clears: ImageClearPipes {
                map: FastStorageMap::default(),
            },
            blits: ImageBlitPipes {
                map: FastStorageMap::default(),
            },
            copy_buffer,
            fill_buffer,
        }
    }

    fn create_copy_buffer(
        library: &metal::LibraryRef,
        device: &metal::DeviceRef,
    ) -> metal::ComputePipelineState {
        let pipeline = metal::ComputePipelineDescriptor::new();

        let cs_copy_buffer = library.get_function("cs_copy_buffer", None).unwrap();
        pipeline.set_compute_function(Some(&cs_copy_buffer));
        pipeline.set_thread_group_size_is_multiple_of_thread_execution_width(true);

        /*TODO: check MacOS version
        if let Some(buffers) = pipeline.buffers() {
            buffers.object_at(0).unwrap().set_mutability(metal::MTLMutability::Mutable);
            buffers.object_at(1).unwrap().set_mutability(metal::MTLMutability::Immutable);
            buffers.object_at(2).unwrap().set_mutability(metal::MTLMutability::Immutable);
        }*/

        unsafe { device.new_compute_pipeline_state(&pipeline) }.unwrap()
    }

    fn create_fill_buffer(
        library: &metal::LibraryRef,
        device: &metal::DeviceRef,
    ) -> metal::ComputePipelineState {
        let pipeline = metal::ComputePipelineDescriptor::new();

        let cs_fill_buffer = library.get_function("cs_fill_buffer", None).unwrap();
        pipeline.set_compute_function(Some(&cs_fill_buffer));
        pipeline.set_thread_group_size_is_multiple_of_thread_execution_width(true);

        /*TODO: check MacOS version
        if let Some(buffers) = pipeline.buffers() {
            buffers.object_at(0).unwrap().set_mutability(metal::MTLMutability::Mutable);
            buffers.object_at(1).unwrap().set_mutability(metal::MTLMutability::Immutable);
        }*/

        unsafe { device.new_compute_pipeline_state(&pipeline) }.unwrap()
    }

    pub(crate) fn simple_blit(
        &self,
        device: &Mutex<metal::Device>,
        cmd_buffer: &metal::CommandBufferRef,
        src: &metal::TextureRef,
        dst: &metal::TextureRef,
        private_caps: &PrivateCapabilities,
    ) {
        let key = (
            metal::MTLTextureType::D2,
            dst.pixel_format(),
            Aspects::COLOR,
            Channel::Float,
        );
        let pso = self.blits.get(key, &self.library, device, private_caps);
        let vertices = [
            BlitVertex {
                uv: [0.0, 1.0, 0.0, 0.0],
                pos: [0.0, 0.0, 0.0, 0.0],
            },
            BlitVertex {
                uv: [0.0, 0.0, 0.0, 0.0],
                pos: [0.0, 1.0, 0.0, 0.0],
            },
            BlitVertex {
                uv: [1.0, 1.0, 0.0, 0.0],
                pos: [1.0, 0.0, 0.0, 0.0],
            },
            BlitVertex {
                uv: [1.0, 0.0, 0.0, 0.0],
                pos: [1.0, 1.0, 0.0, 0.0],
            },
        ];

        let descriptor = metal::RenderPassDescriptor::new();
        if private_caps.layered_rendering {
            descriptor.set_render_target_array_length(1);
        }
        let attachment = descriptor.color_attachments().object_at(0).unwrap();
        attachment.set_texture(Some(dst));
        attachment.set_load_action(metal::MTLLoadAction::DontCare);
        attachment.set_store_action(metal::MTLStoreAction::Store);

        let encoder = cmd_buffer.new_render_command_encoder(descriptor);
        encoder.set_render_pipeline_state(pso.as_ref());
        encoder.set_fragment_sampler_state(0, Some(&self.sampler_states.linear));
        encoder.set_fragment_texture(0, Some(src));
        encoder.set_vertex_bytes(
            0,
            (vertices.len() * mem::size_of::<BlitVertex>()) as u64,
            vertices.as_ptr() as *const _,
        );
        encoder.draw_primitives(metal::MTLPrimitiveType::TriangleStrip, 0, 4);
        encoder.end_encoding();
    }
}
