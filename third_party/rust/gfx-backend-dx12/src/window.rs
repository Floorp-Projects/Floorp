use std::{borrow::Borrow, fmt, mem, os::raw::c_void};

use winapi::{
    shared::{
        dxgi, dxgi1_4, dxgi1_5, dxgitype,
        minwindef::{BOOL, FALSE, TRUE},
        windef::{HWND, RECT},
        winerror,
    },
    um::{d3d12, synchapi, winbase, winnt::HANDLE, winuser::GetClientRect},
};

use crate::{conv, resource as r, Backend, Device, Instance, PhysicalDevice, QueueFamily};
use hal::{device::Device as _, format as f, image as i, window as w};

impl Instance {
    pub fn create_surface_from_hwnd(&self, hwnd: *mut c_void) -> Surface {
        Surface {
            factory: self.factory,
            wnd_handle: hwnd as *mut _,
            presentation: None,
        }
    }
}

#[derive(Debug)]
struct Presentation {
    swapchain: Swapchain,
    format: f::Format,
    size: w::Extent2D,
    mode: w::PresentMode,
}

pub struct Surface {
    pub(crate) factory: native::WeakPtr<dxgi1_4::IDXGIFactory4>,
    pub(crate) wnd_handle: HWND,
    presentation: Option<Presentation>,
}

impl fmt::Debug for Surface {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Surface")
    }
}

unsafe impl Send for Surface {}
unsafe impl Sync for Surface {}

impl Surface {
    pub(crate) unsafe fn present(&mut self, image: SwapchainImage) -> Result<(), w::PresentError> {
        let present = self.presentation.as_mut().unwrap();
        let sc = &mut present.swapchain;
        sc.acquired_count -= 1;

        let expected_index = sc.inner.GetCurrentBackBufferIndex() as w::SwapImageIndex;
        if image.index != expected_index {
            log::warn!(
                "Expected frame {}, got frame {}",
                expected_index,
                image.index
            );
            return Err(w::OutOfDate.into());
        }

        let (interval, flags) = match present.mode {
            w::PresentMode::IMMEDIATE => (0, dxgi::DXGI_PRESENT_ALLOW_TEARING),
            w::PresentMode::FIFO => (1, 0),
            _ => (1, 0), // Surface was created with an unsupported present mode, fall back to FIFO
        };

        sc.inner.Present(interval, flags);
        Ok(())
    }
}

impl w::Surface<Backend> for Surface {
    fn supports_queue_family(&self, queue_family: &QueueFamily) -> bool {
        match queue_family {
            &QueueFamily::Present => true,
            _ => false,
        }
    }

    fn capabilities(&self, _physical_device: &PhysicalDevice) -> w::SurfaceCapabilities {
        let current_extent = unsafe {
            let mut rect: RECT = mem::zeroed();
            if GetClientRect(self.wnd_handle as *mut _, &mut rect as *mut RECT) == 0 {
                panic!("GetClientRect failed");
            }
            Some(w::Extent2D {
                width: (rect.right - rect.left) as u32,
                height: (rect.bottom - rect.top) as u32,
            })
        };

        let allow_tearing = unsafe {
            let (f5, hr) = self.factory.cast::<dxgi1_5::IDXGIFactory5>();
            if winerror::SUCCEEDED(hr) {
                let mut allow_tearing: BOOL = FALSE;
                let hr = f5.CheckFeatureSupport(
                    dxgi1_5::DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                    &mut allow_tearing as *mut _ as *mut _,
                    mem::size_of::<BOOL>() as _,
                );

                f5.destroy();

                winerror::SUCCEEDED(hr) && allow_tearing == TRUE
            } else {
                false
            }
        };

        let mut present_modes = w::PresentMode::FIFO;
        if allow_tearing {
            present_modes |= w::PresentMode::IMMEDIATE;
        }

        w::SurfaceCapabilities {
            present_modes,
            composite_alpha_modes: w::CompositeAlphaMode::OPAQUE, //TODO
            image_count: 2..=16, // we currently use a flip effect which supports 2..=16 buffers
            current_extent,
            extents: w::Extent2D {
                width: 16,
                height: 16,
            }..=w::Extent2D {
                width: 4096,
                height: 4096,
            },
            max_image_layers: 1,
            usage: i::Usage::COLOR_ATTACHMENT | i::Usage::TRANSFER_SRC | i::Usage::TRANSFER_DST,
        }
    }

    fn supported_formats(&self, _physical_device: &PhysicalDevice) -> Option<Vec<f::Format>> {
        Some(vec![
            f::Format::Bgra8Srgb,
            f::Format::Bgra8Unorm,
            f::Format::Rgba8Srgb,
            f::Format::Rgba8Unorm,
            f::Format::A2b10g10r10Unorm,
            f::Format::Rgba16Sfloat,
        ])
    }
}

#[derive(Debug)]
pub struct SwapchainImage {
    index: w::SwapImageIndex,
    image: r::Image,
    view: r::ImageView,
}

impl Borrow<r::Image> for SwapchainImage {
    fn borrow(&self) -> &r::Image {
        &self.image
    }
}

impl Borrow<r::ImageView> for SwapchainImage {
    fn borrow(&self) -> &r::ImageView {
        &self.view
    }
}

impl w::PresentationSurface<Backend> for Surface {
    type SwapchainImage = SwapchainImage;

    unsafe fn configure_swapchain(
        &mut self,
        device: &Device,
        config: w::SwapchainConfig,
    ) -> Result<(), w::SwapchainError> {
        assert!(i::Usage::COLOR_ATTACHMENT.contains(config.image_usage));

        let swapchain = match self.presentation.take() {
            Some(present) => {
                if present.format == config.format && present.size == config.extent {
                    self.presentation = Some(present);
                    return Ok(());
                }
                // can't have image resources in flight used by GPU
                device.wait_idle().unwrap();

                let mut flags = dxgi::DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
                if config.present_mode.contains(w::PresentMode::IMMEDIATE) {
                    flags |= dxgi::DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
                }

                let inner = present.swapchain.release_resources();
                let result = inner.ResizeBuffers(
                    config.image_count,
                    config.extent.width,
                    config.extent.height,
                    conv::map_format_nosrgb(config.format).unwrap(),
                    flags,
                );
                if result != winerror::S_OK {
                    error!("ResizeBuffers failed with 0x{:x}", result as u32);
                    return Err(w::SwapchainError::WindowInUse);
                }
                inner
            }
            None => {
                let (swapchain, _) =
                    device.create_swapchain_impl(&config, self.wnd_handle, self.factory.clone())?;
                swapchain
            }
        };

        // Disable automatic Alt+Enter handling by DXGI.
        const DXGI_MWA_NO_WINDOW_CHANGES: u32 = 1;
        const DXGI_MWA_NO_ALT_ENTER: u32 = 2;
        self.factory.MakeWindowAssociation(
            self.wnd_handle,
            DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER,
        );

        self.presentation = Some(Presentation {
            swapchain: device.wrap_swapchain(swapchain, &config),
            format: config.format,
            size: config.extent,
            mode: config.present_mode,
        });
        Ok(())
    }

    unsafe fn unconfigure_swapchain(&mut self, device: &Device) {
        if let Some(mut present) = self.presentation.take() {
            let _ = present.swapchain.wait(winbase::INFINITE);
            let _ = device.wait_idle(); //TODO: this shouldn't be needed,
                                        // but it complains that the queue is still used otherwise
            let inner = present.swapchain.release_resources();
            inner.destroy();
        }
    }

    unsafe fn acquire_image(
        &mut self,
        timeout_ns: u64,
    ) -> Result<(SwapchainImage, Option<w::Suboptimal>), w::AcquireError> {
        let present = self.presentation.as_mut().unwrap();
        let sc = &mut present.swapchain;

        sc.wait((timeout_ns / 1_000_000) as u32)?;

        let base_index = sc.inner.GetCurrentBackBufferIndex() as usize;
        let index = (base_index + sc.acquired_count) % sc.resources.len();
        sc.acquired_count += 1;
        let resource = sc.resources[index];

        let kind = i::Kind::D2(present.size.width, present.size.height, 1, 1);
        let base_format = present.format.base_format();
        let dxgi_format = conv::map_format(present.format).unwrap();
        let rtv = sc.rtv_heap.at(index as _, 0).cpu;

        let descriptor = d3d12::D3D12_RESOURCE_DESC {
            Dimension: d3d12::D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            Alignment: 0,
            Width: present.size.width as _,
            Height: present.size.height as _,
            DepthOrArraySize: 1,
            MipLevels: 1,
            Format: dxgi_format,
            SampleDesc: dxgitype::DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            Layout: d3d12::D3D12_TEXTURE_LAYOUT_UNKNOWN,
            Flags: d3d12::D3D12_RESOURCE_FLAG_NONE, //TODO?
        };

        let image = r::ImageBound {
            resource,
            place: r::Place::Swapchain {},
            surface_type: base_format.0,
            kind,
            mip_levels: 1,
            default_view_format: None,
            view_caps: i::ViewCapabilities::empty(),
            descriptor,
            clear_cv: Vec::new(), //TODO
            clear_dv: Vec::new(),
            clear_sv: Vec::new(),
            requirements: hal::memory::Requirements {
                size: 0,
                alignment: 1,
                type_mask: 0,
            },
        };

        let swapchain_image = SwapchainImage {
            index: index as _,
            image: r::Image::Bound(image),
            view: r::ImageView {
                resource,
                handle_srv: None,
                handle_rtv: r::RenderTargetHandle::Swapchain(rtv),
                handle_uav: None,
                handle_dsv: None,
                dxgi_format,
                num_levels: 1,
                mip_levels: (0, 1),
                layers: (0, 1),
                kind,
            },
        };

        Ok((swapchain_image, None))
    }
}

#[derive(Debug)]
pub struct Swapchain {
    pub(crate) inner: native::WeakPtr<dxgi1_4::IDXGISwapChain3>,
    #[allow(dead_code)]
    pub(crate) rtv_heap: r::DescriptorHeap,
    // need to associate raw image pointers with the swapchain so they can be properly released
    // when the swapchain is destroyed
    pub(crate) resources: Vec<native::Resource>,
    pub(crate) waitable: HANDLE,
    pub(crate) usage: i::Usage,
    pub(crate) acquired_count: usize,
}

impl Swapchain {
    pub(crate) unsafe fn release_resources(self) -> native::WeakPtr<dxgi1_4::IDXGISwapChain3> {
        for resource in &self.resources {
            resource.destroy();
        }
        self.rtv_heap.destroy();
        self.inner
    }

    pub(crate) fn wait(&mut self, timeout_ms: u32) -> Result<(), w::AcquireError> {
        match unsafe { synchapi::WaitForSingleObject(self.waitable, timeout_ms) } {
            winbase::WAIT_ABANDONED | winbase::WAIT_FAILED => {
                Err(w::AcquireError::DeviceLost(hal::device::DeviceLost))
            }
            winbase::WAIT_OBJECT_0 => Ok(()),
            winerror::WAIT_TIMEOUT => Err(w::AcquireError::NotReady { timeout: true }),
            hr => panic!("Unexpected wait status 0x{:X}", hr),
        }
    }
}

unsafe impl Send for Swapchain {}
unsafe impl Sync for Swapchain {}
