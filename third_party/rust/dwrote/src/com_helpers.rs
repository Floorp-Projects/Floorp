
// This is only handy for implementing a single-interface-implementing IUnknown.
//
// it assumes that there's a UuidOf$interface GUID globally defined

DEFINE_GUID!{UuidOfIUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}

macro_rules! guid_equals {
    ($left:expr, $right:expr) => {
        $left.Data1 == $right.Data1 &&
        $left.Data2 == $right.Data2 &&
        $left.Data3 == $right.Data3 &&
        $left.Data4 == $right.Data4
    }
}

macro_rules! implement_iunknown {
    ($interface:ident, $iuud:ident, $typ:ident) => {
        IUnknownVtbl {
            QueryInterface: {
                unsafe extern "system" fn QueryInterface(This: *mut IUnknown,
                                                         riid: REFIID,
                                                         ppvObject: *mut *mut c_void) -> HRESULT {
                    let this = if guid_equals!(*riid, $iuud) {
                        mem::transmute(This)
                    } else if guid_equals!(*riid, UuidOfIUnknown) {
                        mem::transmute(This)
                    } else {
                        return $crate::winapi::shared::winerror::E_NOINTERFACE;
                    };

                    (*This).AddRef();
                    *ppvObject = this;
                    return S_OK;
                }
                QueryInterface
            },
            AddRef: {
                unsafe extern "system" fn AddRef(This: *mut IUnknown) -> ULONG {
                    let this = $typ::from_interface(This);
                    let count = this.refcount.fetch_add(1, atomic::Ordering::Relaxed) + 1;
                    count as ULONG
                }
                AddRef
            },
            Release: {
                unsafe extern "system" fn Release(This: *mut IUnknown) -> ULONG {
                    let this = $typ::from_interface(This);
                    let count = this.refcount.fetch_sub(1, atomic::Ordering::Release) - 1;
                    if count == 0 {
                        <$typ as Com<$interface>>::destroy(This as *mut $interface);
                    }
                    count as ULONG
                }
                Release
            },
        }
    };
    (static $interface:ident, $iuud:ident, $typ:ident) => {
        IUnknownVtbl {
            QueryInterface: {
                unsafe extern "system" fn QueryInterface(This: *mut IUnknown,
                                                         riid: REFIID,
                                                         ppvObject: *mut *mut $crate::winapi::ctypes::c_void) -> HRESULT {
                    let this = if guid_equals!(*riid, $iuud) {
                        mem::transmute(This)
                    } else if guid_equals!(*riid, UuidOfIUnknown) {
                        mem::transmute(This)
                    } else {
                        return $crate::winapi::shared::winerror::E_NOINTERFACE;
                    };

                    (*This).AddRef();
                    *ppvObject = this;
                    return S_OK;
                }
                QueryInterface
            },
            AddRef: {
                unsafe extern "system" fn AddRef(_This: *mut IUnknown) -> ULONG {
                    1
                }
                AddRef
            },
            Release: {
                unsafe extern "system" fn Release(_This: *mut IUnknown) -> ULONG {
                    1
                }
                Release
            },
        }
    }
}

#[repr(C)]
pub struct ComRepr<Type, Vtbl>(*const Vtbl, Type);

pub trait Com<Interface> where Self: Sized {
    type Vtbl: 'static;

    fn vtbl() -> &'static Self::Vtbl;

    fn into_interface(self) -> *mut Interface {
        let com = Box::new(ComRepr(Self::vtbl(), self));
        Box::into_raw(com) as *mut Interface
    }

    unsafe fn from_interface<'a>(thing: *mut Interface) -> &'a mut Self {
        &mut (*(thing as *mut ComRepr<Self, Self::Vtbl>)).1
    }

    unsafe fn destroy(thing: *mut Interface) {
        Box::from_raw(thing as *mut ComRepr<Self, Self::Vtbl>);
    }
}

