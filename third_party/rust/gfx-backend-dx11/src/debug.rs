use winapi::um::d3d11;

use wio::com::ComPtr;
use wio::wide::ToWide;

use std::ffi::OsStr;
use std::{env, fmt};

// TODO: replace with new winapi version when available
#[allow(bad_style, unused)]
mod temp {
    use winapi::shared::minwindef::{BOOL, INT};
    use winapi::um::unknwnbase::{IUnknown, IUnknownVtbl};
    use winapi::um::winnt::LPCWSTR;

    RIDL! {#[uuid(0xb2daad8b, 0x03d4, 0x4dbf, 0x95, 0xeb, 0x32, 0xab, 0x4b, 0x63, 0xd0, 0xab)]
    interface ID3DUserDefinedAnnotation(ID3DUserDefinedAnnotationVtbl):
        IUnknown(IUnknownVtbl) {
        fn BeginEvent(
            Name: LPCWSTR,
        ) -> INT,
        fn EndEvent() -> INT,
        fn SetMarker(
            Name: LPCWSTR,
        ) -> (),
        fn GetStatus() -> BOOL,
    }}
}

#[must_use]
#[cfg(debug_assertions)]
pub struct DebugScope {
    annotation: ComPtr<temp::ID3DUserDefinedAnnotation>,
}

#[cfg(debug_assertions)]
impl DebugScope {
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

        let annotation = context.cast::<temp::ID3DUserDefinedAnnotation>().unwrap();
        let msg: &OsStr = name.as_ref();
        let msg: Vec<u16> = msg.to_wide_null();

        unsafe {
            annotation.BeginEvent(msg.as_ptr() as _);
        }

        Some(DebugScope { annotation })
    }
}

#[cfg(debug_assertions)]
impl Drop for DebugScope {
    fn drop(&mut self) {
        unsafe {
            self.annotation.EndEvent();
        }
    }
}

#[cfg(debug_assertions)]
pub fn debug_marker(context: &ComPtr<d3d11::ID3D11DeviceContext>, args: fmt::Arguments) {
    let name = format!("{}", args);

    // same here
    if unsafe { context.GetType() } == d3d11::D3D11_DEVICE_CONTEXT_DEFERRED {
        if env::var("GFX_NO_RENDERDOC").is_ok() {
            return;
        }
    }

    let annotation = context.cast::<temp::ID3DUserDefinedAnnotation>().unwrap();
    let msg: &OsStr = name.as_ref();
    let msg: Vec<u16> = msg.to_wide_null();

    unsafe {
        annotation.SetMarker(msg.as_ptr() as _);
    }
}
