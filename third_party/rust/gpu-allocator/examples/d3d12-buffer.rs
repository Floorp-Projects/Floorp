//! Example showcasing [`winapi`] interop with [`gpu-allocator`] which is driven by the [`windows`] crate.
use winapi::shared::{dxgiformat, winerror};
use winapi::um::{d3d12, d3dcommon};
use winapi::Interface;

mod all_dxgi {
    pub use winapi::shared::{dxgi1_3::*, dxgi1_6::*, dxgitype::*};
}

use log::*;

use gpu_allocator::d3d12::{
    AllocationCreateDesc, Allocator, AllocatorCreateDesc, ID3D12DeviceVersion, ResourceCategory,
    ToWinapi, ToWindows,
};
use gpu_allocator::MemoryLocation;

fn create_d3d12_device(
    dxgi_factory: *mut all_dxgi::IDXGIFactory6,
) -> Option<*mut d3d12::ID3D12Device> {
    for idx in 0.. {
        let mut adapter4: *mut all_dxgi::IDXGIAdapter4 = std::ptr::null_mut();
        let hr = unsafe {
            dxgi_factory.as_ref().unwrap().EnumAdapters1(
                idx,
                <*mut *mut all_dxgi::IDXGIAdapter4>::cast(&mut adapter4),
            )
        };

        if hr == winerror::DXGI_ERROR_NOT_FOUND {
            break;
        }

        assert_eq!(hr, winerror::S_OK);

        let mut desc = all_dxgi::DXGI_ADAPTER_DESC3::default();
        let hr = unsafe { adapter4.as_ref().unwrap().GetDesc3(&mut desc) };
        if hr != winerror::S_OK {
            error!("Failed to get adapter description for adapter");
            continue;
        }

        // Skip software adapters
        if (desc.Flags & all_dxgi::DXGI_ADAPTER_FLAG3_SOFTWARE)
            == all_dxgi::DXGI_ADAPTER_FLAG3_SOFTWARE
        {
            continue;
        }

        let feature_levels = [
            (d3dcommon::D3D_FEATURE_LEVEL_11_0, "D3D_FEATURE_LEVEL_11_0"),
            (d3dcommon::D3D_FEATURE_LEVEL_11_1, "D3D_FEATURE_LEVEL_11_1"),
            (d3dcommon::D3D_FEATURE_LEVEL_12_0, "D3D_FEATURE_LEVEL_12_0"),
        ];

        let device =
            feature_levels
                .iter()
                .rev()
                .find_map(|&(feature_level, feature_level_name)| {
                    let mut device: *mut d3d12::ID3D12Device = std::ptr::null_mut();
                    let hr = unsafe {
                        d3d12::D3D12CreateDevice(
                            adapter4.cast(),
                            feature_level,
                            &d3d12::ID3D12Device::uuidof(),
                            <*mut *mut d3d12::ID3D12Device>::cast(&mut device),
                        )
                    };
                    match hr {
                        winapi::shared::winerror::S_OK => {
                            info!("Using D3D12 feature level: {}.", feature_level_name);
                            Some(device)
                        }
                        winapi::shared::winerror::E_NOINTERFACE => {
                            error!("ID3D12Device interface not supported.");
                            None
                        }
                        _ => {
                            info!(
                                "D3D12 feature level: {} not supported: {:x}",
                                feature_level_name, hr
                            );
                            None
                        }
                    }
                });
        if device.is_some() {
            return device;
        }
    }

    None
}

fn main() {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("trace")).init();

    let dxgi_factory = {
        let mut dxgi_factory: *mut all_dxgi::IDXGIFactory6 = std::ptr::null_mut();
        let hr = unsafe {
            all_dxgi::CreateDXGIFactory2(
                0,
                &all_dxgi::IID_IDXGIFactory6,
                <*mut *mut all_dxgi::IDXGIFactory6>::cast(&mut dxgi_factory),
            )
        };

        assert_eq!(
            hr,
            winapi::shared::winerror::S_OK,
            "Failed to create DXGI factory",
        );
        dxgi_factory
    };

    let device = create_d3d12_device(dxgi_factory).expect("Failed to create D3D12 device.");

    // Setting up the allocator
    let mut allocator = Allocator::new(&AllocatorCreateDesc {
        device: ID3D12DeviceVersion::Device(device.as_windows().clone()),
        debug_settings: Default::default(),
        allocation_sizes: Default::default(),
    })
    .unwrap();

    let device = unsafe { device.as_ref() }.unwrap();

    // Test allocating Gpu Only memory
    {
        let test_buffer_desc = d3d12::D3D12_RESOURCE_DESC {
            Dimension: d3d12::D3D12_RESOURCE_DIMENSION_BUFFER,
            Alignment: d3d12::D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT as u64,
            Width: 512,
            Height: 1,
            DepthOrArraySize: 1,
            MipLevels: 1,
            Format: dxgiformat::DXGI_FORMAT_UNKNOWN,
            SampleDesc: all_dxgi::DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            Layout: d3d12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            Flags: d3d12::D3D12_RESOURCE_FLAG_NONE,
        };

        let allocation_desc = AllocationCreateDesc::from_winapi_d3d12_resource_desc(
            device,
            &test_buffer_desc,
            "Test allocation (Gpu Only)",
            MemoryLocation::GpuOnly,
        );
        let allocation = allocator.allocate(&allocation_desc).unwrap();

        let mut resource: *mut d3d12::ID3D12Resource = std::ptr::null_mut();
        let hr = unsafe {
            device.CreatePlacedResource(
                allocation.heap().as_winapi() as *mut _,
                allocation.offset(),
                &test_buffer_desc,
                d3d12::D3D12_RESOURCE_STATE_COMMON,
                std::ptr::null(),
                &d3d12::IID_ID3D12Resource,
                <*mut *mut d3d12::ID3D12Resource>::cast(&mut resource),
            )
        };
        if hr != winerror::S_OK {
            panic!("Failed to create placed resource.");
        }

        unsafe { resource.as_ref().unwrap().Release() };

        allocator.free(allocation).unwrap();
        info!("Allocation and deallocation of GpuOnly memory was successful.");
    }

    // Test allocating Cpu to Gpu memory
    {
        let test_buffer_desc = d3d12::D3D12_RESOURCE_DESC {
            Dimension: d3d12::D3D12_RESOURCE_DIMENSION_BUFFER,
            Alignment: d3d12::D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT as u64,
            Width: 512,
            Height: 1,
            DepthOrArraySize: 1,
            MipLevels: 1,
            Format: dxgiformat::DXGI_FORMAT_UNKNOWN,
            SampleDesc: all_dxgi::DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            Layout: d3d12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            Flags: d3d12::D3D12_RESOURCE_FLAG_NONE,
        };

        let alloc_info = unsafe { device.GetResourceAllocationInfo(0, 1, &test_buffer_desc) };

        let allocation = allocator
            .allocate(&AllocationCreateDesc {
                name: "Test allocation (Cpu to Gpu)",
                location: MemoryLocation::CpuToGpu,
                size: alloc_info.SizeInBytes,
                alignment: alloc_info.Alignment,
                resource_category: ResourceCategory::Buffer,
            })
            .unwrap();

        let mut resource: *mut d3d12::ID3D12Resource = std::ptr::null_mut();
        let hr = unsafe {
            device.CreatePlacedResource(
                allocation.heap().as_winapi() as *mut _,
                allocation.offset(),
                &test_buffer_desc,
                d3d12::D3D12_RESOURCE_STATE_COMMON,
                std::ptr::null(),
                &d3d12::IID_ID3D12Resource,
                <*mut *mut d3d12::ID3D12Resource>::cast(&mut resource),
            )
        };
        if hr != winerror::S_OK {
            panic!("Failed to create placed resource.");
        }

        unsafe { resource.as_ref().unwrap().Release() };

        allocator.free(allocation).unwrap();
        info!("Allocation and deallocation of CpuToGpu memory was successful.");
    }

    // Test allocating Gpu to Cpu memory
    {
        let test_buffer_desc = d3d12::D3D12_RESOURCE_DESC {
            Dimension: d3d12::D3D12_RESOURCE_DIMENSION_BUFFER,
            Alignment: d3d12::D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT as u64,
            Width: 512,
            Height: 1,
            DepthOrArraySize: 1,
            MipLevels: 1,
            Format: dxgiformat::DXGI_FORMAT_UNKNOWN,
            SampleDesc: all_dxgi::DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            Layout: d3d12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            Flags: d3d12::D3D12_RESOURCE_FLAG_NONE,
        };

        let alloc_info = unsafe { device.GetResourceAllocationInfo(0, 1, &test_buffer_desc) };

        let allocation = allocator
            .allocate(&AllocationCreateDesc {
                name: "Test allocation (Gpu to Cpu)",
                location: MemoryLocation::GpuToCpu,
                size: alloc_info.SizeInBytes,
                alignment: alloc_info.Alignment,
                resource_category: ResourceCategory::Buffer,
            })
            .unwrap();

        let mut resource: *mut d3d12::ID3D12Resource = std::ptr::null_mut();
        let hr = unsafe {
            device.CreatePlacedResource(
                allocation.heap().as_winapi() as *mut _,
                allocation.offset(),
                &test_buffer_desc,
                d3d12::D3D12_RESOURCE_STATE_COMMON,
                std::ptr::null(),
                &d3d12::IID_ID3D12Resource,
                <*mut *mut d3d12::ID3D12Resource>::cast(&mut resource),
            )
        };
        if hr != winerror::S_OK {
            panic!("Failed to create placed resource.");
        }

        unsafe { resource.as_ref().unwrap().Release() };

        allocator.free(allocation).unwrap();
        info!("Allocation and deallocation of CpuToGpu memory was successful.");
    }

    drop(allocator); // Explicitly drop before destruction of device.
    unsafe { device.Release() };
    unsafe { dxgi_factory.as_ref().unwrap().Release() };
}
