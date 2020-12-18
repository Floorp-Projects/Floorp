use auxil::FastHashMap;
use std::{ffi::CStr, mem, ptr, sync::Arc};

use parking_lot::Mutex;
use winapi::{
    shared::{
        dxgiformat, dxgitype,
        minwindef::{FALSE, TRUE},
        winerror,
    },
    um::d3d12::{self, *},
    Interface,
};

use native;

#[derive(Clone, Debug)]
pub struct BlitPipe {
    pub pipeline: native::PipelineState,
    pub signature: native::RootSignature,
}

impl BlitPipe {
    pub unsafe fn destroy(&self) {
        self.pipeline.destroy();
        self.signature.destroy();
    }
}

// Information to pass to the shader
#[repr(C)]
#[derive(Debug)]
pub struct BlitData {
    pub src_offset: [f32; 2],
    pub src_extent: [f32; 2],
    pub layer: f32,
    pub level: f32,
}

pub type BlitKey = (dxgiformat::DXGI_FORMAT, d3d12::D3D12_FILTER);
type BlitMap = FastHashMap<BlitKey, BlitPipe>;

#[derive(Debug)]
pub(crate) struct ServicePipes {
    pub(crate) device: native::Device,
    library: Arc<native::D3D12Lib>,
    blits_2d_color: Mutex<BlitMap>,
}

impl ServicePipes {
    pub fn new(device: native::Device, library: Arc<native::D3D12Lib>) -> Self {
        ServicePipes {
            device,
            library,
            blits_2d_color: Mutex::new(FastHashMap::default()),
        }
    }

    pub unsafe fn destroy(&self) {
        let blits = self.blits_2d_color.lock();
        for (_, pipe) in &*blits {
            pipe.destroy();
        }
    }

    pub fn get_blit_2d_color(&self, key: BlitKey) -> BlitPipe {
        let mut blits = self.blits_2d_color.lock();
        blits
            .entry(key)
            .or_insert_with(|| self.create_blit_2d_color(key))
            .clone()
    }

    fn create_blit_2d_color(&self, (dst_format, filter): BlitKey) -> BlitPipe {
        let descriptor_range = [native::DescriptorRange::new(
            native::DescriptorRangeType::SRV,
            1,
            native::Binding {
                register: 0,
                space: 0,
            },
            0,
        )];

        let root_parameters = [
            native::RootParameter::descriptor_table(
                native::ShaderVisibility::All,
                &descriptor_range,
            ),
            native::RootParameter::constants(
                native::ShaderVisibility::All,
                native::Binding {
                    register: 0,
                    space: 0,
                },
                (mem::size_of::<BlitData>() / 4) as _,
            ),
        ];

        let static_samplers = [native::StaticSampler::new(
            native::ShaderVisibility::PS,
            native::Binding {
                register: 0,
                space: 0,
            },
            filter,
            [
                d3d12::D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                d3d12::D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                d3d12::D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            ],
            0.0,
            0,
            d3d12::D3D12_COMPARISON_FUNC_ALWAYS,
            native::StaticBorderColor::TransparentBlack,
            0.0..d3d12::D3D12_FLOAT32_MAX,
        )];

        let (signature_raw, error) = match self.library.serialize_root_signature(
            native::RootSignatureVersion::V1_0,
            &root_parameters,
            &static_samplers,
            native::RootSignatureFlags::empty(),
        ) {
            Ok((pair, hr)) if winerror::SUCCEEDED(hr) => pair,
            Ok((_, hr)) => panic!("Can't serialize internal root signature: {:?}", hr),
            Err(e) => panic!("Can't find serialization function: {:?}", e),
        };

        if !error.is_null() {
            error!("D3D12SerializeRootSignature error: {:?}", unsafe {
                error.as_c_str().to_str().unwrap()
            });
            unsafe { error.destroy() };
        }

        let (signature, _hr) = self.device.create_root_signature(signature_raw, 0);
        unsafe {
            signature_raw.destroy();
        }

        let shader_src = include_bytes!("../shaders/blit.hlsl");
        // TODO: check results
        let ((vs, _), _hr_vs) = native::Shader::compile(
            shader_src,
            unsafe { CStr::from_bytes_with_nul_unchecked(b"vs_5_0\0") },
            unsafe { CStr::from_bytes_with_nul_unchecked(b"vs_blit_2d\0") },
            native::ShaderCompileFlags::empty(),
        );
        let ((ps, _), _hr_ps) = native::Shader::compile(
            shader_src,
            unsafe { CStr::from_bytes_with_nul_unchecked(b"ps_5_0\0") },
            unsafe { CStr::from_bytes_with_nul_unchecked(b"ps_blit_2d\0") },
            native::ShaderCompileFlags::empty(),
        );

        let mut rtvs = [dxgiformat::DXGI_FORMAT_UNKNOWN; 8];
        rtvs[0] = dst_format;

        let dummy_target = D3D12_RENDER_TARGET_BLEND_DESC {
            BlendEnable: FALSE,
            LogicOpEnable: FALSE,
            SrcBlend: D3D12_BLEND_ZERO,
            DestBlend: D3D12_BLEND_ZERO,
            BlendOp: D3D12_BLEND_OP_ADD,
            SrcBlendAlpha: D3D12_BLEND_ZERO,
            DestBlendAlpha: D3D12_BLEND_ZERO,
            BlendOpAlpha: D3D12_BLEND_OP_ADD,
            LogicOp: D3D12_LOGIC_OP_CLEAR,
            RenderTargetWriteMask: D3D12_COLOR_WRITE_ENABLE_ALL as _,
        };
        let render_targets = [dummy_target; 8];

        let pso_desc = d3d12::D3D12_GRAPHICS_PIPELINE_STATE_DESC {
            pRootSignature: signature.as_mut_ptr(),
            VS: *native::Shader::from_blob(vs),
            PS: *native::Shader::from_blob(ps),
            GS: *native::Shader::null(),
            DS: *native::Shader::null(),
            HS: *native::Shader::null(),
            StreamOutput: d3d12::D3D12_STREAM_OUTPUT_DESC {
                pSODeclaration: ptr::null(),
                NumEntries: 0,
                pBufferStrides: ptr::null(),
                NumStrides: 0,
                RasterizedStream: 0,
            },
            BlendState: d3d12::D3D12_BLEND_DESC {
                AlphaToCoverageEnable: FALSE,
                IndependentBlendEnable: FALSE,
                RenderTarget: render_targets,
            },
            SampleMask: !0,
            RasterizerState: D3D12_RASTERIZER_DESC {
                FillMode: D3D12_FILL_MODE_SOLID,
                CullMode: D3D12_CULL_MODE_NONE,
                FrontCounterClockwise: TRUE,
                DepthBias: 0,
                DepthBiasClamp: 0.0,
                SlopeScaledDepthBias: 0.0,
                DepthClipEnable: FALSE,
                MultisampleEnable: FALSE,
                ForcedSampleCount: 0,
                AntialiasedLineEnable: FALSE,
                ConservativeRaster: D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
            },
            DepthStencilState: unsafe { mem::zeroed() },
            InputLayout: d3d12::D3D12_INPUT_LAYOUT_DESC {
                pInputElementDescs: ptr::null(),
                NumElements: 0,
            },
            IBStripCutValue: d3d12::D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            PrimitiveTopologyType: D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            NumRenderTargets: 1,
            RTVFormats: rtvs,
            DSVFormat: dxgiformat::DXGI_FORMAT_UNKNOWN,
            SampleDesc: dxgitype::DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            NodeMask: 0,
            CachedPSO: d3d12::D3D12_CACHED_PIPELINE_STATE {
                pCachedBlob: ptr::null(),
                CachedBlobSizeInBytes: 0,
            },
            Flags: d3d12::D3D12_PIPELINE_STATE_FLAG_NONE,
        };

        let mut pipeline = native::PipelineState::null();
        let hr = unsafe {
            self.device.CreateGraphicsPipelineState(
                &pso_desc,
                &d3d12::ID3D12PipelineState::uuidof(),
                pipeline.mut_void(),
            )
        };
        assert_eq!(hr, winerror::S_OK);

        BlitPipe {
            pipeline,
            signature,
        }
    }
}
