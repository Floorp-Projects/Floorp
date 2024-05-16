use std::sync::Arc;

use gpu_allocator::metal::{AllocationCreateDesc, Allocator, AllocatorCreateDesc};
use log::info;

fn main() {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("trace")).init();

    let device = Arc::new(metal::Device::system_default().unwrap());

    // Setting up the allocator
    let mut allocator = Allocator::new(&AllocatorCreateDesc {
        device: device.clone(),
        debug_settings: Default::default(),
        allocation_sizes: Default::default(),
    })
    .unwrap();

    // Test allocating Gpu Only memory
    {
        let allocation_desc = AllocationCreateDesc::buffer(
            &device,
            "Test allocation (Gpu Only)",
            512,
            gpu_allocator::MemoryLocation::GpuOnly,
        );
        let allocation = allocator.allocate(&allocation_desc).unwrap();
        let _buffer = allocation.make_buffer().unwrap();
        allocator.free(&allocation).unwrap();
        info!("Allocation and deallocation of GpuOnly memory was successful.");
    }

    // Test allocating Cpu to Gpu memory
    {
        let allocation_desc = AllocationCreateDesc::buffer(
            &device,
            "Test allocation (Cpu to Gpu)",
            512,
            gpu_allocator::MemoryLocation::CpuToGpu,
        );
        let allocation = allocator.allocate(&allocation_desc).unwrap();
        let _buffer = allocation.make_buffer().unwrap();
        allocator.free(&allocation).unwrap();
        info!("Allocation and deallocation of CpuToGpu memory was successful.");
    }

    // Test allocating Gpu to Cpu memory
    {
        let allocation_desc = AllocationCreateDesc::buffer(
            &device,
            "Test allocation (Gpu to Cpu)",
            512,
            gpu_allocator::MemoryLocation::GpuToCpu,
        );
        let allocation = allocator.allocate(&allocation_desc).unwrap();
        let _buffer = allocation.make_buffer().unwrap();
        allocator.free(&allocation).unwrap();
        info!("Allocation and deallocation of GpuToCpu memory was successful.");
    }

    // Test allocating texture
    {
        let texture_desc = metal::TextureDescriptor::new();
        texture_desc.set_pixel_format(metal::MTLPixelFormat::RGBA8Unorm);
        texture_desc.set_width(64);
        texture_desc.set_height(64);
        texture_desc.set_storage_mode(metal::MTLStorageMode::Private);
        let allocation_desc =
            AllocationCreateDesc::texture(&device, "Test allocation (Texture)", &texture_desc);
        let allocation = allocator.allocate(&allocation_desc).unwrap();
        let _texture = allocation.make_texture(&texture_desc).unwrap();
        allocator.free(&allocation).unwrap();
        info!("Allocation and deallocation of Texture was successful.");
    }

    // Test allocating acceleration structure
    {
        let empty_array = metal::Array::from_slice(&[]);
        let acc_desc = metal::PrimitiveAccelerationStructureDescriptor::descriptor();
        acc_desc.set_geometry_descriptors(empty_array);
        let sizes = device.acceleration_structure_sizes_with_descriptor(&acc_desc);
        let allocation_desc = AllocationCreateDesc::acceleration_structure_with_size(
            &device,
            "Test allocation (Acceleration structure)",
            sizes.acceleration_structure_size,
            gpu_allocator::MemoryLocation::GpuOnly,
        );
        let allocation = allocator.allocate(&allocation_desc).unwrap();
        let _acc_structure = allocation.make_acceleration_structure();
        allocator.free(&allocation).unwrap();
        info!("Allocation and deallocation of Acceleration structure was successful.");
    }
}
