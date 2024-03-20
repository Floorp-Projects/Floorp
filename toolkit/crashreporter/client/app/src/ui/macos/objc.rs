/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Objective-C bindings and helpers.

// Forward all exports from the `objc` crate.
pub use objc::*;

/// An objc class instance which contains rust data `T`.
#[derive(Clone, Copy)]
#[repr(transparent)]
pub struct Objc<T> {
    pub instance: cocoa::id,
    _phantom: std::marker::PhantomData<*mut T>,
}

impl<T> Objc<T> {
    pub fn new(instance: cocoa::id) -> Self {
        Objc {
            instance,
            _phantom: std::marker::PhantomData,
        }
    }

    pub fn data(&self) -> &T {
        let data = *unsafe { (*self.instance).get_ivar::<usize>("rust_self") } as *mut T;
        unsafe { &*data }
    }

    pub fn data_mut(&mut self) -> &mut T {
        let data = *unsafe { (*self.instance).get_ivar::<usize>("rust_self") } as *mut T;
        unsafe { &mut *data }
    }
}

impl<T> std::ops::Deref for Objc<T> {
    type Target = T;
    fn deref(&self) -> &Self::Target {
        self.data()
    }
}

impl<T> std::ops::DerefMut for Objc<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.data_mut()
    }
}

unsafe impl<T> Encode for Objc<T> {
    fn encode() -> Encoding {
        cocoa::id::encode()
    }
}

/// Wrapper to provide `Encode` for bindgen-generated types (bindgen should do this in the future).
#[repr(transparent)]
pub struct Ptr<T>(pub T);

unsafe impl<T> Encode for Ptr<T> {
    fn encode() -> Encoding {
        cocoa::id::encode()
    }
}

/// A strong objective-c reference to `T`.
#[repr(transparent)]
pub struct StrongRef<T> {
    ptr: rc::StrongPtr,
    _phantom: std::marker::PhantomData<T>,
}

impl<T> Clone for StrongRef<T> {
    fn clone(&self) -> Self {
        StrongRef {
            ptr: self.ptr.clone(),
            _phantom: self._phantom,
        }
    }
}

impl<T> std::ops::Deref for StrongRef<T> {
    type Target = T;
    fn deref(&self) -> &Self::Target {
        let obj: &cocoa::id = &*self.ptr;
        unsafe { std::mem::transmute(obj) }
    }
}

impl<T> StrongRef<T> {
    /// Assume the given pointer-wrapper is an already-retained strong reference.
    ///
    /// # Safety
    /// The type _must_ be the same size as cocoa::id and contain only a cocoa::id.
    pub unsafe fn new(v: T) -> Self {
        std::mem::transmute_copy(&v)
    }

    /// Retain the given pointer-wrapper.
    ///
    /// # Safety
    /// The type _must_ be the same size as cocoa::id and contain only a cocoa::id.
    #[allow(dead_code)]
    pub unsafe fn retain(v: T) -> Self {
        let obj: cocoa::id = std::mem::transmute_copy(&v);
        StrongRef {
            ptr: rc::StrongPtr::retain(obj),
            _phantom: std::marker::PhantomData,
        }
    }

    pub fn autorelease(self) -> T {
        let obj = self.ptr.autorelease();
        unsafe { std::mem::transmute_copy(&obj) }
    }

    pub fn weak(&self) -> WeakRef<T> {
        WeakRef {
            ptr: self.ptr.weak(),
            _phantom: std::marker::PhantomData,
        }
    }

    /// Unwrap the StrongRef value without affecting reference counts.
    ///
    /// This is the opposite of `new`.
    #[allow(dead_code)]
    pub fn unwrap(self: Self) -> T {
        let v = unsafe { std::mem::transmute_copy(&self) };
        std::mem::forget(self);
        v
    }

    /// Cast to a base class.
    ///
    /// Bindgen pointer-wrappers have trival `From<Derived> for Base` implementations.
    pub fn cast<U: From<T>>(self) -> StrongRef<U> {
        StrongRef {
            ptr: self.ptr,
            _phantom: std::marker::PhantomData,
        }
    }
}

/// A weak objective-c reference to `T`.
#[derive(Clone)]
#[repr(transparent)]
pub struct WeakRef<T> {
    ptr: rc::WeakPtr,
    _phantom: std::marker::PhantomData<T>,
}

impl<T> WeakRef<T> {
    pub fn lock(&self) -> Option<StrongRef<T>> {
        let ptr = self.ptr.load();
        if ptr.is_null() {
            None
        } else {
            Some(StrongRef {
                ptr,
                _phantom: std::marker::PhantomData,
            })
        }
    }
}

/// A macro for creating an objc class.
///
/// Classes _must_ be registered before use (`Objc<T>::register()`).
///
/// Example:
/// ```
/// struct Foo(u8);
///
/// objc_class! {
///     impl Foo: NSObject {
///         #[sel(mySelector:)]
///         fn my_selector(&mut self, arg: u8) -> u8 {
///             self.0 + arg
///         }
///     }
/// }
///
/// fn make_foo() -> StrongRef<Objc<Foo>> {
///     Foo(42).into_object()
/// }
/// ```
///
/// Call `T::into_object()` to create the objective-c class instance.
macro_rules! objc_class {
    ( impl $name:ident : $base:ident $(<$($protocol:ident),+>)? {
        $(
            #[sel($($sel:tt)+)]
            fn $mname:ident (&mut $self:ident $(, $argname:ident : $argtype:ty )*) $(-> $rettype:ty)? $body:block
        )*
    }) => {
        impl Objc<$name> {
            pub fn register() {
                let mut decl = declare::ClassDecl::new(concat!("CR", stringify!($name)), class!($base)).expect(concat!("failed to declare ", stringify!($name), " class"));
                $($(decl.add_protocol(runtime::Protocol::get(stringify!($protocol)).expect(concat!("failed to find ",stringify!($protocol)," protocol")));)+)?
                decl.add_ivar::<usize>("rust_self");
                $({
                    extern fn method_impl(obj: &mut runtime::Object, _: runtime::Sel $(, $argname: $argtype )*) $(-> $rettype)? {
                        Objc::<$name>::new(obj).$mname($($argname),*)
                    }
                    unsafe {
                        decl.add_method(sel!($($sel)+), method_impl as extern fn(&mut runtime::Object, runtime::Sel $(, $argname: $argtype )*) $(-> $rettype)?);
                    }
                })*
                {
                    extern fn dealloc_impl(obj: &runtime::Object, _: runtime::Sel) {
                        drop(unsafe { Box::from_raw(*obj.get_ivar::<usize>("rust_self") as *mut $name) });
                        unsafe {
                            let _: () = msg_send![super(obj, class!(NSObject)), dealloc];
                        }
                    }
                    unsafe {
                        decl.add_method(sel!(dealloc), dealloc_impl as extern fn(&runtime::Object, runtime::Sel));
                    }
                }
                decl.register();
            }

            pub fn class() -> &'static runtime::Class {
                runtime::Class::get(concat!("CR", stringify!($name))).expect("class not registered")
            }

            $(fn $mname (&mut $self $(, $argname : $argtype )*) $(-> $rettype)? $body)*
        }

        impl $name {
            pub fn into_object(self) -> StrongRef<Objc<$name>> {
                let obj: *mut runtime::Object = unsafe { msg_send![Objc::<Self>::class(), alloc] };
                unsafe { (*obj).set_ivar("rust_self", Box::into_raw(Box::new(self)) as usize) };
                let obj: *mut runtime::Object = unsafe { msg_send![obj, init] };
                unsafe { StrongRef::new(Objc::new(obj)) }
            }
        }
    }
}

pub(crate) use objc_class;
