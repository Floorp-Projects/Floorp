use winapi::um::{d3d11, d3d11_1, d3dcommon};

use wio::{com::ComPtr, wide::ToWide};

use std::{env, ffi::OsStr, fmt};

#[must_use]
pub struct DebugScope {
    annotation: ComPtr<d3d11_1::ID3DUserDefinedAnnotation>,
}

impl DebugScope {
    // TODO: Not used currently in release, will be used in the future
    #[allow(dead_code)]
    pub fn with_name(
        context: &ComPtr<d3d11::ID3D11DeviceContext>,
        args: fmt::Arguments,
    ) -> Option<Self> {
        let name = format!("{}", args);

        // debugging with visual studio and its ilk *really* doesn't like calling this on a
        // deferred context when replaying a capture, compared to renderdoc
        if unsafe { context.GetType() } == d3d11::D3D11_DEVICE_CONTEXT_DEFERRED {
            // TODO: find a better way to detect either if RD or VS is active debugger
            if env::var("GFX_NO_RENDERDOC").is_ok() {
                return None;
            }
        }

        let annotation = context
            .cast::<d3d11_1::ID3DUserDefinedAnnotation>()
            .unwrap();
        let msg: &OsStr = name.as_ref();
        let msg: Vec<u16> = msg.to_wide_null();

        unsafe {
            annotation.BeginEvent(msg.as_ptr() as _);
        }

        Some(DebugScope { annotation })
    }
}

impl Drop for DebugScope {
    fn drop(&mut self) {
        unsafe {
            self.annotation.EndEvent();
        }
    }
}

pub fn debug_marker(context: &ComPtr<d3d11::ID3D11DeviceContext>, name: &str) {
    // same here
    if unsafe { context.GetType() } == d3d11::D3D11_DEVICE_CONTEXT_DEFERRED {
        if env::var("GFX_NO_RENDERDOC").is_ok() {
            return;
        }
    }

    let annotation = context
        .cast::<d3d11_1::ID3DUserDefinedAnnotation>()
        .unwrap();
    let msg: &OsStr = name.as_ref();
    let msg: Vec<u16> = msg.to_wide_null();

    unsafe {
        annotation.SetMarker(msg.as_ptr() as _);
    }
}

pub fn verify_debug_ascii(name: &str) -> bool {
    let res = name.is_ascii();
    if !res {
        error!("DX11 buffer names must be ASCII");
    }
    res
}

/// Set the debug name of a resource.
///
/// Must be ASCII.
///
/// SetPrivateData will copy the data internally so the data doesn't need to live.
pub fn set_debug_name(resource: &d3d11::ID3D11DeviceChild, name: &str) {
    unsafe {
        resource.SetPrivateData(
            (&d3dcommon::WKPDID_D3DDebugObjectName) as *const _,
            name.len() as _,
            name.as_ptr() as *const _,
        );
    }
}

/// Set the debug name of a resource with a given suffix.
///
/// Must be ASCII.
///
/// The given string will be mutated to add the suffix, then restored to it's original state.
/// This saves an allocation. SetPrivateData will copy the data internally so the data doesn't need to live.
pub fn set_debug_name_with_suffix(
    resource: &d3d11::ID3D11DeviceChild,
    name: &mut String,
    suffix: &str,
) {
    name.push_str(suffix);
    unsafe {
        resource.SetPrivateData(
            (&d3dcommon::WKPDID_D3DDebugObjectName) as *const _,
            name.len() as _,
            name.as_ptr() as *const _,
        );
    }
    name.drain((name.len() - suffix.len())..);
}
