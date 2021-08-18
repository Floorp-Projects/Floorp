use hal::adapter::{AdapterInfo, DeviceType};

use winapi::{
    shared::{
        dxgi, dxgi1_2, dxgi1_3, dxgi1_4, dxgi1_5,
        guiddef::{GUID, REFIID},
        winerror,
    },
    um::{d3d11, unknwnbase::IUnknown},
    Interface,
};

use wio::com::ComPtr;

use std::{ffi::OsString, mem, os::windows::ffi::OsStringExt, ptr};

#[derive(Debug, Copy, Clone)]
pub(crate) enum DxgiVersion {
    /// Capable of the following interfaces:
    ///   * IDXGIObject
    ///   * IDXGIDeviceSubObject
    ///   * IDXGIResource
    ///   * IDXGIKeyedMutex
    ///   * IDXGISurface
    ///   * IDXGISurface1
    ///   * IDXGIOutput
    ///   * IDXGISwapChain
    ///   * IDXGIFactory
    ///   * IDXGIDevice
    ///   * IDXGIFactory1
    ///   * IDXGIAdapter1
    ///   * IDXGIDevice1
    Dxgi1_0,

    /// Capable of the following interfaces:
    ///   * IDXGIDisplayControl
    ///   * IDXGIOutputDuplication
    ///   * IDXGISurface2
    ///   * IDXGIResource1
    ///   * IDXGIDevice2
    ///   * IDXGISwapChain1
    ///   * IDXGIFactory2
    ///   * IDXGIAdapter2
    ///   * IDXGIOutput1
    Dxgi1_2,

    /// Capable of the following interfaces:
    ///   * IDXGIDevice3
    ///   * IDXGISwapChain2
    ///   * IDXGIOutput2
    ///   * IDXGIDecodeSwapChain
    ///   * IDXGIFactoryMedia
    ///   * IDXGISwapChainMedia
    ///   * IDXGIOutput3
    Dxgi1_3,

    /// Capable of the following interfaces:
    ///   * IDXGISwapChain3
    ///   * IDXGIOutput4
    ///   * IDXGIFactory4
    ///   * IDXGIAdapter3
    Dxgi1_4,

    /// Capable of the following interfaces:
    ///   * IDXGIOutput5
    ///   * IDXGISwapChain4
    ///   * IDXGIDevice4
    ///   * IDXGIFactory5
    Dxgi1_5,
}

type DxgiFun =
    unsafe extern "system" fn(REFIID, *mut *mut winapi::ctypes::c_void) -> winerror::HRESULT;

fn create_dxgi_factory1(
    func: &DxgiFun,
    guid: &GUID,
) -> Result<ComPtr<dxgi::IDXGIFactory>, winerror::HRESULT> {
    let mut factory: *mut IUnknown = ptr::null_mut();

    let hr = unsafe { func(guid, &mut factory as *mut *mut _ as *mut *mut _) };

    if winerror::SUCCEEDED(hr) {
        Ok(unsafe { ComPtr::from_raw(factory as *mut _) })
    } else {
        Err(hr)
    }
}

pub(crate) fn get_dxgi_factory(
) -> Result<(libloading::Library, ComPtr<dxgi::IDXGIFactory>, DxgiVersion), winerror::HRESULT> {
    // The returned Com-pointer is only safe to use for the lifetime of the Library.
    let library = unsafe { libloading::Library::new("dxgi.dll").map_err(|_| -1)? };
    let func: libloading::Symbol<DxgiFun> =
        unsafe { library.get(b"CreateDXGIFactory1") }.map_err(|_| -1)?;

    // TODO: do we even need `create_dxgi_factory2`?
    if let Ok(factory) = create_dxgi_factory1(&func, &dxgi1_5::IDXGIFactory5::uuidof()) {
        return Ok((library, factory, DxgiVersion::Dxgi1_5));
    }

    if let Ok(factory) = create_dxgi_factory1(&func, &dxgi1_4::IDXGIFactory4::uuidof()) {
        return Ok((library, factory, DxgiVersion::Dxgi1_4));
    }

    if let Ok(factory) = create_dxgi_factory1(&func, &dxgi1_3::IDXGIFactory3::uuidof()) {
        return Ok((library, factory, DxgiVersion::Dxgi1_3));
    }

    if let Ok(factory) = create_dxgi_factory1(&func, &dxgi1_2::IDXGIFactory2::uuidof()) {
        return Ok((library, factory, DxgiVersion::Dxgi1_2));
    }

    if let Ok(factory) = create_dxgi_factory1(&func, &dxgi::IDXGIFactory1::uuidof()) {
        return Ok((library, factory, DxgiVersion::Dxgi1_0));
    }

    // TODO: any reason why above would fail and this wouldnt?
    match create_dxgi_factory1(&func, &dxgi::IDXGIFactory::uuidof()) {
        Ok(factory) => Ok((library, factory, DxgiVersion::Dxgi1_0)),
        Err(hr) => Err(hr),
    }
}

fn enum_adapters1(
    idx: u32,
    factory: *mut dxgi::IDXGIFactory,
) -> Result<ComPtr<dxgi::IDXGIAdapter>, winerror::HRESULT> {
    let mut adapter: *mut dxgi::IDXGIAdapter = ptr::null_mut();

    let hr = unsafe {
        (*(factory as *mut dxgi::IDXGIFactory1))
            .EnumAdapters1(idx, &mut adapter as *mut *mut _ as *mut *mut _)
    };

    if winerror::SUCCEEDED(hr) {
        Ok(unsafe { ComPtr::from_raw(adapter) })
    } else {
        Err(hr)
    }
}

fn build_adapter_info(
    description: &[u16],
    vendor_id: u32,
    device_id: u32,
    flags: u32,
    video_mem: usize,
    device: &ComPtr<d3d11::ID3D11Device>,
) -> AdapterInfo {
    let mut features_options2: d3d11::D3D11_FEATURE_DATA_D3D11_OPTIONS2 = unsafe { mem::zeroed() };
    let discovered_gpu_type = winerror::SUCCEEDED(unsafe {
        device.CheckFeatureSupport(
            d3d11::D3D11_FEATURE_D3D11_OPTIONS2,
            &mut features_options2 as *mut _ as *mut _,
            mem::size_of::<d3d11::D3D11_FEATURE_DATA_D3D11_OPTIONS2>() as _,
        )
    });

    let device_name = {
        let len = description.iter().take_while(|&&c| c != 0).count();
        let name = <OsString as OsStringExt>::from_wide(&description[..len]);
        name.to_string_lossy().into_owned()
    };

    AdapterInfo {
        name: device_name,
        vendor: vendor_id as usize,
        device: device_id as usize,
        device_type: if (flags & dxgi::DXGI_ADAPTER_FLAG_SOFTWARE) != 0 {
            DeviceType::VirtualGpu
        } else if discovered_gpu_type {
            if features_options2.UnifiedMemoryArchitecture == 1 {
                DeviceType::IntegratedGpu
            } else {
                DeviceType::DiscreteGpu
            }
        } else if video_mem <= 512_000_000 {
            DeviceType::IntegratedGpu
        } else {
            DeviceType::DiscreteGpu
        },
    }
}

pub(crate) fn get_adapter_desc(
    adapter: &ComPtr<dxgi::IDXGIAdapter>,
    device: &ComPtr<d3d11::ID3D11Device>,
    version: DxgiVersion,
) -> AdapterInfo {
    let adapter = adapter.as_raw();

    match version {
        DxgiVersion::Dxgi1_0 => {
            let mut desc: dxgi::DXGI_ADAPTER_DESC1 = unsafe { mem::zeroed() };
            unsafe {
                (*(adapter as *mut dxgi::IDXGIAdapter1)).GetDesc1(&mut desc);
            }

            build_adapter_info(
                &desc.Description,
                desc.VendorId,
                desc.DeviceId,
                desc.Flags,
                desc.DedicatedVideoMemory,
                device,
            )
        }
        DxgiVersion::Dxgi1_2
        | DxgiVersion::Dxgi1_3
        | DxgiVersion::Dxgi1_4
        | DxgiVersion::Dxgi1_5 => {
            let mut desc: dxgi1_2::DXGI_ADAPTER_DESC2 = unsafe { mem::zeroed() };
            unsafe {
                (*(adapter as *mut dxgi1_2::IDXGIAdapter2)).GetDesc2(&mut desc);
            }

            build_adapter_info(
                &desc.Description,
                desc.VendorId,
                desc.DeviceId,
                desc.Flags,
                desc.DedicatedVideoMemory,
                device,
            )
        }
    }
}

pub(crate) fn get_adapter(
    idx: u32,
    factory: *mut dxgi::IDXGIFactory,
    version: DxgiVersion,
) -> Result<ComPtr<dxgi::IDXGIAdapter>, winerror::HRESULT> {
    let adapter = match version {
        DxgiVersion::Dxgi1_0
        | DxgiVersion::Dxgi1_2
        | DxgiVersion::Dxgi1_3
        | DxgiVersion::Dxgi1_4
        | DxgiVersion::Dxgi1_5 => enum_adapters1(idx, factory)?,
    };

    Ok(adapter)
}
