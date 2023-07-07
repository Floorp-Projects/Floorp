# Ash

A very lightweight wrapper around Vulkan

[![Crates.io Version](https://img.shields.io/crates/v/ash.svg)](https://crates.io/crates/ash)
[![Documentation](https://docs.rs/ash/badge.svg)](https://docs.rs/ash)
[![Build Status](https://github.com/MaikKlein/ash/workflows/CI/badge.svg)](https://github.com/MaikKlein/ash/actions?workflow=CI)
[![LICENSE](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE-MIT)
[![LICENSE](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE-APACHE)
[![Join the chat at https://gitter.im/MaikKlein/ash](https://badges.gitter.im/MaikKlein/ash.svg)](https://gitter.im/MaikKlein/ash?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![MSRV](https://img.shields.io/badge/rustc-1.60.0+-ab6000.svg)](https://blog.rust-lang.org/2022/04/07/Rust-1.60.0.html)

## Overview
- [x] A true Vulkan API without compromises
- [x] Convenience features without limiting functionality
- [x] Additional type safety
- [x] Device local function pointer loading
- [x] No validation, everything is **unsafe**
- [x] Generated from `vk.xml`
- [x] Support for Vulkan `1.1`, `1.2`, `1.3`

## ⚠️ Semver compatibility warning
The Vulkan Video bindings are experimental and still seeing breaking changes in their upstream specification, and are only provided by Ash for early adopters. All related functions and types are semver-exempt [^1] (we allow breaking API changes while releasing Ash with non-breaking semver bumps).

[^1]: `generator` complexity makes it so that we cannot easily hide these bindings behind a non-`default` feature flag, and they are widespread across the generated codebase.

## Features
### Explicit returns with `Result`
```rust
// function signature
pub fn create_instance(&self,
                       create_info: &vk::InstanceCreateInfo,
                       allocation_callbacks: Option<&vk::AllocationCallbacks>)
                       -> Result<Instance, InstanceError> { .. }
let instance = entry.create_instance(&create_info, None)
    .expect("Instance creation error");
```


### `Vec<T>` instead of mutable slices

```rust
pub fn get_swapchain_images(&self,
                            swapchain: vk::SwapchainKHR)
                            -> VkResult<Vec<vk::Image>>;
let present_images = swapchain_loader.get_swapchain_images_khr(swapchain).unwrap();
```
*Note*: Functions don't return `Vec<T>` if this would limit the functionality. See `p_next`.

### Slices
```rust
pub fn cmd_pipeline_barrier(&self,
                            command_buffer: vk::CommandBuffer,
                            src_stage_mask: vk::PipelineStageFlags,
                            dst_stage_mask: vk::PipelineStageFlags,
                            dependency_flags: vk::DependencyFlags,
                            memory_barriers: &[vk::MemoryBarrier],
                            buffer_memory_barriers: &[vk::BufferMemoryBarrier],
                            image_memory_barriers: &[vk::ImageMemoryBarrier]);
```

### Strongly typed handles

Each Vulkan handle type is exposed as a newtyped struct for improved type safety. Null handles can be constructed with
`T::null()`, and handles may be freely converted to and from `u64` with `Handle::from_raw` and `Handle::as_raw` for
interop with non-Ash Vulkan code.

### Default implementation for all types
```rust
// No need to manually set the structure type
let desc_alloc_info = vk::DescriptorSetAllocateInfo {
    descriptor_pool: self.pool,
    descriptor_set_count: self.layouts.len() as u32,
    p_set_layouts: self.layouts.as_ptr(),
    ..Default::default()
};
```
### Builder pattern

```rust
// We lose all lifetime information when we call `.build()`. Be careful!
let queue_info = [vk::DeviceQueueCreateInfo::builder()
    .queue_family_index(queue_family_index)
    .queue_priorities(&priorities)
    .build()];

// We don't need to call `.build()` here because builders implement `Deref`.
let device_create_info = vk::DeviceCreateInfo::builder()
    .queue_create_infos(&queue_info)
    .enabled_extension_names(&device_extension_names_raw)
    .enabled_features(&features);

let device: Device = instance
    .create_device(pdevice, &device_create_info, None)
    .unwrap();
```

To not lose this lifetime single items can be "cast" to a slice of length _one_ with `std::slice::from_ref` while still taking advantage of `Deref`:

```rust
let queue_info = vk::DeviceQueueCreateInfo::builder()
    .queue_family_index(queue_family_index)
    .queue_priorities(&priorities);

let device_create_info = vk::DeviceCreateInfo::builder()
    .queue_create_infos(std::slice::from_ref(&queue_info))
    ...;
```

Builders have an explicit lifetime, and are marked as `#[repr(transparent)]`.
```rust
#[repr(transparent)]
pub struct DeviceCreateInfoBuilder<'a> {
    inner: DeviceCreateInfo,
    marker: ::std::marker::PhantomData<&'a ()>,
}
impl<'a> DeviceCreateInfoBuilder<'a> {
    //...
    pub fn queue_create_infos(
        mut self,
        queue_create_infos: &'a [DeviceQueueCreateInfo],
    ) -> DeviceCreateInfoBuilder<'a> {...}
    //...
```

Every reference has to live as long as the builder itself. Builders implement `Deref` targeting their corresponding Vulkan struct, so references to builders can be passed directly
to Vulkan functions.

Calling `.build()` will **discard** that lifetime because Vulkan structs use raw pointers internally. This should be avoided as much as possible because this can easily lead to dangling pointers. If `.build()` has to be called, it should be called as late as possible. [Lifetimes of temporaries](https://doc.rust-lang.org/reference/expressions.html#temporary-lifetimes) are extended to the enclosing statement, ensuring they are valid for the duration of a Vulkan call occurring in the same statement.


### Pointer chains

```rust
let mut variable_pointers = vk::PhysicalDeviceVariablePointerFeatures::builder();
let mut corner =
    vk::PhysicalDeviceCornerSampledImageFeaturesNV::builder();
;
let mut device_create_info = vk::DeviceCreateInfo::builder()
    .push_next(&mut corner)
    .push_next(&mut variable_pointers);
```

Pointer chains in builders differ from raw Vulkan. Instead of chaining every struct manually, you instead use `.push_next` on the struct that you are going to pass into the function. Those structs then get *prepended* into the chain.

`push_next` is also type checked, you can only add valid structs to the chain. Both the structs and the builders can be passed into `push_next`. Only builders for structs that can be passed into functions will implement a `push_next`.


### Flags and constants as associated constants

```rust
// Bitflag
vk::AccessFlags::COLOR_ATTACHMENT_READ | vk::AccessFlags::COLOR_ATTACHMENT_WRITE
```

```rust
// Constant
vk::PipelineBindPoint::GRAPHICS,
```
### Debug/Display for Flags

```rust
let flag = vk::AccessFlags::COLOR_ATTACHMENT_READ
        | vk::AccessFlags::COLOR_ATTACHMENT_WRITE;
println!("Debug: {:?}", flag);
println!("Display: {}", flag);
// Prints:
// Debug: AccessFlags(110000000)
// Display: COLOR_ATTACHMENT_READ | COLOR_ATTACHMENT_WRITE
```

### Function pointer loading
Ash also takes care of loading the function pointers. Function pointers are split into 3 categories.

* Entry: Loads the Vulkan library. Needs to outlive `Instance` and `Device`.
* Instance: Loads instance level functions. Needs to outlive the `Device`s it has created.
* Device: Loads device **local** functions.

The loader is just one possible implementation:

* Device level functions are retrieved on a per device basis.
* Everything is loaded by default, functions that failed to load are initialized to a function that always panics.
* Do not call Vulkan 1.1 functions if you have created a 1.0 instance. Doing so will result in a panic.

Custom loaders can be implemented.

### Extension loading
Additionally, every Vulkan extension has to be loaded explicitly. You can find all extensions under [ash::extensions](https://github.com/MaikKlein/ash/tree/master/ash/src/extensions).
```rust
use ash::extensions::khr::Swapchain;
let swapchain_loader = Swapchain::new(&instance, &device);
let swapchain = swapchain_loader.create_swapchain(&swapchain_create_info).unwrap();
```

### Raw function pointers

Raw function pointers are available, if something hasn't been exposed yet in the higher level API. Please open an issue if anything is missing.

```rust
device.fp_v1_0().destroy_device(...);
```

### Support for extension names
```rust
use ash::extensions::{Swapchain, XlibSurface, Surface, DebugReport};
#[cfg(all(unix, not(target_os = "android")))]
fn extension_names() -> Vec<*const i8> {
    vec![
        Surface::name().as_ptr(),
        XlibSurface::name().as_ptr(),
        DebugReport::name().as_ptr()
    ]
}
```

### Implicit handles
Handles from Instance or Device are passed implicitly.
```rust
pub fn create_command_pool(&self,
                           create_info: &vk::CommandPoolCreateInfo)
                           -> VkResult<vk::CommandPool>;

let pool = device.create_command_pool(&pool_create_info).unwrap();
```

### Optional linking

The default `loaded` cargo feature will dynamically load the default Vulkan library for the current platform with `Entry::load`, meaning that the build environment does not have to have Vulkan development packages installed.

If, on the other hand, your application cannot handle Vulkan being missing at runtime, you can instead enable the `linked` feature, which will link your binary with the Vulkan loader directly and expose the infallible `Entry::linked`.

## Example
You can find the examples [here](https://github.com/MaikKlein/ash/tree/master/examples).
All examples currently require: the LunarG Validation layers and a Vulkan library that is visible in your `PATH`. An easy way to get started is to use the [LunarG Vulkan SDK](https://lunarg.com/vulkan-sdk/)
#### Windows
Make sure that you have a Vulkan ready driver and install the [LunarG Vulkan SDK](https://lunarg.com/vulkan-sdk/).
#### Linux
Make sure that you have a Vulkan ready driver and install the [LunarG Vulkan SDK](https://lunarg.com/vulkan-sdk/). You also have to add the library and layers to your path. Have a look at my [post](http://askubuntu.com/a/803110/77183) if you are unsure how to do that.
#### macOS
Install the [LunarG Vulkan SDK](https://lunarg.com/vulkan-sdk/). The installer puts the SDK in `$HOME/VulkanSDK/<version>` by default. You will need to set the following environment variables when running cargo:
```sh
VULKAN_SDK=$HOME/VulkanSDK/<version>/macOS \
DYLD_FALLBACK_LIBRARY_PATH=$VULKAN_SDK/lib \
VK_ICD_FILENAMES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json \
VK_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d \
cargo run ...
```

### [Triangle](https://github.com/MaikKlein/ash/blob/master/examples/src/bin/triangle.rs)
Displays a triangle with vertex colors.
```
cd examples
cargo run --bin triangle
```

![screenshot](http://i.imgur.com/PQZcL6w.jpg)

### [Texture](https://github.com/MaikKlein/ash/blob/master/examples/src/bin/texture.rs)
Displays a texture on a quad.
```
cd examples
cargo run --bin texture
```
![texture](http://i.imgur.com/trow00H.png)

## Useful resources

### Examples

* [vulkan-tutorial-rust](https://github.com/Usami-Renko/vulkan-tutorial-rust) - A port of [vulkan-tutorial.com](https://vulkan-tutorial.com).
* [ash-sample-progression](https://github.com/bzm3r/ash-sample-progression) - A port of the LunarG examples.
* [ash-nv-rt](https://github.com/gwihlidal/ash-nv-rt) A raytracing example for ash.

### Utility libraries
* [vk-sync](https://github.com/gwihlidal/vk-sync-rs) - Simplified Vulkan synchronization logic, written in rust.
* [vk-mem-rs](https://github.com/gwihlidal/vk-mem-rs) - This crate provides an FFI layer and idiomatic rust wrappers for the excellent AMD Vulkan Memory Allocator (VMA) C/C++ library.
* [gpu-allocator](https://github.com/Traverse-Research/gpu-allocator) - Memory allocator written in pure Rust for GPU memory in Vulkan and in the future DirectX 12
* [lahar](https://github.com/Ralith/lahar) - Tools for asynchronously uploading data to a Vulkan device.

### Libraries that use ash
* [gfx-rs](https://github.com/gfx-rs/gfx) - gfx-rs is a low-level, cross-platform graphics abstraction library in Rust.

## A thanks to

* [Api with no secrets](https://software.intel.com/en-us/articles/api-without-secrets-introduction-to-vulkan-part-1)
* [Vulkan tutorial](http://jhenriques.net/development.html)
* [Vulkan examples](https://github.com/SaschaWillems/Vulkan)
* [Vulkan tutorial](https://vulkan-tutorial.com/)
* [Vulkano](https://github.com/vulkano-rs/vulkano)
* [vk-rs](https://github.com/Osspial/vk-rs)
