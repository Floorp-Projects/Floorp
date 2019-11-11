//! Interfaces for accessing instance data from hostcalls.
//!
//! This module contains both a Rust-friendly API ([`Vmctx`](struct.Vmctx.html)) as well as C-style
//! exports for compatibility with hostcalls written against `lucet-runtime-c`.

pub use crate::c_api::lucet_vmctx;

use crate::alloc::instance_heap_offset;
use crate::context::Context;
use crate::error::Error;
use crate::instance::{
    EmptyYieldVal, Instance, InstanceInternal, State, TerminationDetails, YieldedVal,
    CURRENT_INSTANCE, HOST_CTX,
};
use lucet_module::{FunctionHandle, GlobalValue};
use std::any::Any;
use std::borrow::{Borrow, BorrowMut};
use std::cell::{Ref, RefCell, RefMut};
use std::marker::PhantomData;

/// An opaque handle to a running instance's context.
#[derive(Debug)]
pub struct Vmctx {
    vmctx: *mut lucet_vmctx,
    /// A view of the underlying instance's heap.
    ///
    /// This must never be dropped automatically, as the view does not own the heap. Rather, this is
    /// a value used to implement dynamic borrowing of the heap contents that are owned and managed
    /// by the instance and its `Alloc`.
    heap_view: RefCell<Box<[u8]>>,
    /// A view of the underlying instance's globals.
    ///
    /// This must never be dropped automatically, as the view does not own the globals. Rather, this
    /// is a value used to implement dynamic borrowing of the globals that are owned and managed by
    /// the instance and its `Alloc`.
    globals_view: RefCell<Box<[GlobalValue]>>,
}

impl Drop for Vmctx {
    fn drop(&mut self) {
        let heap_view = self.heap_view.replace(Box::new([]));
        let globals_view = self.globals_view.replace(Box::new([]));
        // as described in the definition of `Vmctx`, we cannot allow the boxed views of the heap
        // and globals to be dropped
        Box::leak(heap_view);
        Box::leak(globals_view);
    }
}

pub trait VmctxInternal {
    /// Get a reference to the `Instance` for this guest.
    fn instance(&self) -> &Instance;

    /// Get a mutable reference to the `Instance` for this guest.
    ///
    /// ### Safety
    ///
    /// Using this method, you could hold on to multiple mutable references to the same
    /// `Instance`. Only use one at a time! This method does not take `&mut self` because otherwise
    /// you could not use orthogonal `&mut` refs that come from `Vmctx`, like the heap or
    /// terminating the instance.
    unsafe fn instance_mut(&self) -> &mut Instance;

    /// Try to take and return the value passed to `Instance::resume_with_val()`.
    ///
    /// If there is no resumed value, or if the dynamic type check of the value fails, this returns
    /// `None`.
    fn try_take_resumed_val<R: Any + 'static>(&self) -> Option<R>;

    /// Suspend the instance, returning a value in
    /// [`RunResult::Yielded`](../enum.RunResult.html#variant.Yielded) to where the instance was run
    /// or resumed.
    ///
    /// After suspending, the instance may be resumed by calling
    /// [`Instance::resume_with_val()`](../struct.Instance.html#method.resume_with_val) from the
    /// host with a value of type `R`. If resumed with a value of some other type, this returns
    /// `None`.
    ///
    /// The dynamic type checks used by the other yield methods should make this explicit option
    /// type redundant, however this interface is used to avoid exposing a panic to the C API.
    fn yield_val_try_val<A: Any + 'static, R: Any + 'static>(&self, val: A) -> Option<R>;
}

impl VmctxInternal for Vmctx {
    fn instance(&self) -> &Instance {
        unsafe { instance_from_vmctx(self.vmctx) }
    }

    unsafe fn instance_mut(&self) -> &mut Instance {
        instance_from_vmctx(self.vmctx)
    }

    fn try_take_resumed_val<R: Any + 'static>(&self) -> Option<R> {
        let inst = unsafe { self.instance_mut() };
        if let Some(val) = inst.resumed_val.take() {
            match val.downcast() {
                Ok(val) => Some(*val),
                Err(val) => {
                    inst.resumed_val = Some(val);
                    None
                }
            }
        } else {
            None
        }
    }

    fn yield_val_try_val<A: Any + 'static, R: Any + 'static>(&self, val: A) -> Option<R> {
        self.yield_impl::<A, R>(val);
        self.try_take_resumed_val()
    }
}

impl Vmctx {
    /// Create a `Vmctx` from the compiler-inserted `vmctx` argument in a guest function.
    ///
    /// This is almost certainly not what you want to use to get a `Vmctx`; instead use the `&mut
    /// Vmctx` argument to a `lucet_hostcalls!`-wrapped function.
    pub unsafe fn from_raw(vmctx: *mut lucet_vmctx) -> Vmctx {
        let inst = instance_from_vmctx(vmctx);
        assert!(inst.valid_magic());

        let res = Vmctx {
            vmctx,
            heap_view: RefCell::new(Box::<[u8]>::from_raw(inst.heap_mut())),
            globals_view: RefCell::new(Box::<[GlobalValue]>::from_raw(inst.globals_mut())),
        };
        res
    }

    /// Return the underlying `vmctx` pointer.
    pub fn as_raw(&self) -> *mut lucet_vmctx {
        self.vmctx
    }

    /// Return the WebAssembly heap as a slice of bytes.
    ///
    /// If the heap is already mutably borrowed by `heap_mut()`, the instance will
    /// terminate with `TerminationDetails::BorrowError`.
    pub fn heap(&self) -> Ref<'_, [u8]> {
        unsafe {
            self.reconstitute_heap_view_if_needed();
        }
        let r = self
            .heap_view
            .try_borrow()
            .unwrap_or_else(|_| panic!(TerminationDetails::BorrowError("heap")));
        Ref::map(r, |b| b.borrow())
    }

    /// Return the WebAssembly heap as a mutable slice of bytes.
    ///
    /// If the heap is already borrowed by `heap()` or `heap_mut()`, the instance will terminate
    /// with `TerminationDetails::BorrowError`.
    pub fn heap_mut(&self) -> RefMut<'_, [u8]> {
        unsafe {
            self.reconstitute_heap_view_if_needed();
        }
        let r = self
            .heap_view
            .try_borrow_mut()
            .unwrap_or_else(|_| panic!(TerminationDetails::BorrowError("heap_mut")));
        RefMut::map(r, |b| b.borrow_mut())
    }

    /// Check whether the heap has grown, and replace the heap view if it has.
    ///
    /// This handles the case where `Vmctx::grow_memory()` and `Vmctx::heap()` are called in
    /// sequence. Since `Vmctx::grow_memory()` takes `&mut self`, heap references cannot live across
    /// it.
    ///
    /// TODO: There is still an unsound case, though, when a heap reference is held across a call
    /// back into the guest via `Vmctx::get_func_from_idx()`. That guest code may grow the heap as
    /// well, causing any outstanding heap references to become invalid. We will address this when
    /// we rework the interface for calling back into the guest.
    unsafe fn reconstitute_heap_view_if_needed(&self) {
        let inst = self.instance_mut();
        if inst.heap_mut().len() != self.heap_view.borrow().len() {
            let old_heap_view = self
                .heap_view
                .replace(Box::<[u8]>::from_raw(inst.heap_mut()));
            // as described in the definition of `Vmctx`, we cannot allow the boxed view of the heap
            // to be dropped
            Box::leak(old_heap_view);
        }
    }

    /// Check whether a given range in the host address space overlaps with the memory that backs
    /// the instance heap.
    pub fn check_heap<T>(&self, ptr: *const T, len: usize) -> bool {
        self.instance().check_heap(ptr, len)
    }

    /// Check whether a context value of a particular type exists.
    pub fn contains_embed_ctx<T: Any>(&self) -> bool {
        self.instance().contains_embed_ctx::<T>()
    }

    /// Get a reference to a context value of a particular type.
    ///
    /// If a context of that type does not exist, the instance will terminate with
    /// `TerminationDetails::CtxNotFound`.
    ///
    /// If the context is already mutably borrowed by `get_embed_ctx_mut`, the instance will
    /// terminate with `TerminationDetails::BorrowError`.
    pub fn get_embed_ctx<T: Any>(&self) -> Ref<'_, T> {
        match self.instance().embed_ctx.try_get::<T>() {
            Some(Ok(t)) => t,
            Some(Err(_)) => panic!(TerminationDetails::BorrowError("get_embed_ctx")),
            None => panic!(TerminationDetails::CtxNotFound),
        }
    }

    /// Get a mutable reference to a context value of a particular type.
    ///
    /// If a context of that type does not exist, the instance will terminate with
    /// `TerminationDetails::CtxNotFound`.
    ///
    /// If the context is already borrowed by some other use of `get_embed_ctx` or
    /// `get_embed_ctx_mut`, the instance will terminate with `TerminationDetails::BorrowError`.
    pub fn get_embed_ctx_mut<T: Any>(&self) -> RefMut<'_, T> {
        match unsafe { self.instance_mut().embed_ctx.try_get_mut::<T>() } {
            Some(Ok(t)) => t,
            Some(Err(_)) => panic!(TerminationDetails::BorrowError("get_embed_ctx_mut")),
            None => panic!(TerminationDetails::CtxNotFound),
        }
    }

    /// Terminate this guest and return to the host context without unwinding.
    ///
    /// This is almost certainly not what you want to use to terminate an instance from a hostcall,
    /// as any resources currently in scope will not be dropped. Instead, use
    /// `lucet_hostcall_terminate!` which unwinds to the enclosing hostcall body.
    pub unsafe fn terminate_no_unwind(&mut self, details: TerminationDetails) -> ! {
        self.instance_mut().terminate(details)
    }

    /// Grow the guest memory by the given number of WebAssembly pages.
    ///
    /// On success, returns the number of pages that existed before the call.
    pub fn grow_memory(&mut self, additional_pages: u32) -> Result<u32, Error> {
        unsafe { self.instance_mut().grow_memory(additional_pages) }
    }

    /// Return the WebAssembly globals as a slice of `i64`s.
    ///
    /// If the globals are already mutably borrowed by `globals_mut()`, the instance will terminate
    /// with `TerminationDetails::BorrowError`.
    pub fn globals(&self) -> Ref<'_, [GlobalValue]> {
        let r = self
            .globals_view
            .try_borrow()
            .unwrap_or_else(|_| panic!(TerminationDetails::BorrowError("globals")));
        Ref::map(r, |b| b.borrow())
    }

    /// Return the WebAssembly globals as a mutable slice of `i64`s.
    ///
    /// If the globals are already borrowed by `globals()` or `globals_mut()`, the instance will
    /// terminate with `TerminationDetails::BorrowError`.
    pub fn globals_mut(&self) -> RefMut<'_, [GlobalValue]> {
        let r = self
            .globals_view
            .try_borrow_mut()
            .unwrap_or_else(|_| panic!(TerminationDetails::BorrowError("globals_mut")));
        RefMut::map(r, |b| b.borrow_mut())
    }

    /// Get a function pointer by WebAssembly table and function index.
    ///
    /// This is useful when a hostcall takes a function pointer as its argument, as WebAssembly uses
    /// table indices as its runtime representation of function pointers.
    ///
    /// We do not currently reflect function type information into the Rust type system, so callers
    /// of the returned function must take care to cast it to the correct type before calling. The
    /// correct type will include the `vmctx` argument, which the caller is responsible for passing
    /// from its own context.
    ///
    /// ```no_run
    /// use lucet_runtime_internals::{lucet_hostcalls, lucet_hostcall_terminate};
    /// use lucet_runtime_internals::vmctx::{lucet_vmctx, Vmctx};
    ///
    /// lucet_hostcalls! {
    ///     #[no_mangle]
    ///     pub unsafe extern "C" fn hostcall_call_binop(
    ///         &mut vmctx,
    ///         binop_table_idx: u32,
    ///         binop_func_idx: u32,
    ///         operand1: u32,
    ///         operand2: u32,
    ///     ) -> u32 {
    ///         if let Ok(binop) = vmctx.get_func_from_idx(binop_table_idx, binop_func_idx) {
    ///             let typed_binop = std::mem::transmute::<
    ///                 usize,
    ///                 extern "C" fn(*mut lucet_vmctx, u32, u32) -> u32
    ///             >(binop.ptr.as_usize());
    ///             unsafe { (typed_binop)(vmctx.as_raw(), operand1, operand2) }
    ///         } else {
    ///             lucet_hostcall_terminate!("invalid function index")
    ///         }
    ///     }
    /// }
    pub fn get_func_from_idx(
        &self,
        table_idx: u32,
        func_idx: u32,
    ) -> Result<FunctionHandle, Error> {
        self.instance()
            .module()
            .get_func_from_idx(table_idx, func_idx)
    }

    /// Suspend the instance, returning an empty
    /// [`RunResult::Yielded`](../enum.RunResult.html#variant.Yielded) to where the instance was run
    /// or resumed.
    ///
    /// After suspending, the instance may be resumed by the host using
    /// [`Instance::resume()`](../struct.Instance.html#method.resume).
    ///
    /// (The reason for the trailing underscore in the name is that Rust reserves `yield` as a
    /// keyword for future use.)
    pub fn yield_(&self) {
        self.yield_val_expecting_val::<EmptyYieldVal, EmptyYieldVal>(EmptyYieldVal);
    }

    /// Suspend the instance, returning an empty
    /// [`RunResult::Yielded`](../enum.RunResult.html#variant.Yielded) to where the instance was run
    /// or resumed.
    ///
    /// After suspending, the instance may be resumed by calling
    /// [`Instance::resume_with_val()`](../struct.Instance.html#method.resume_with_val) from the
    /// host with a value of type `R`.
    pub fn yield_expecting_val<R: Any + 'static>(&self) -> R {
        self.yield_val_expecting_val::<EmptyYieldVal, R>(EmptyYieldVal)
    }

    /// Suspend the instance, returning a value in
    /// [`RunResult::Yielded`](../enum.RunResult.html#variant.Yielded) to where the instance was run
    /// or resumed.
    ///
    /// After suspending, the instance may be resumed by the host using
    /// [`Instance::resume()`](../struct.Instance.html#method.resume).
    pub fn yield_val<A: Any + 'static>(&self, val: A) {
        self.yield_val_expecting_val::<A, EmptyYieldVal>(val);
    }

    /// Suspend the instance, returning a value in
    /// [`RunResult::Yielded`](../enum.RunResult.html#variant.Yielded) to where the instance was run
    /// or resumed.
    ///
    /// After suspending, the instance may be resumed by calling
    /// [`Instance::resume_with_val()`](../struct.Instance.html#method.resume_with_val) from the
    /// host with a value of type `R`.
    pub fn yield_val_expecting_val<A: Any + 'static, R: Any + 'static>(&self, val: A) -> R {
        self.yield_impl::<A, R>(val);
        self.take_resumed_val()
    }

    fn yield_impl<A: Any + 'static, R: Any + 'static>(&self, val: A) {
        let inst = unsafe { self.instance_mut() };
        let expecting: Box<PhantomData<R>> = Box::new(PhantomData);
        inst.state = State::Yielding {
            val: YieldedVal::new(val),
            expecting: expecting as Box<dyn Any>,
        };
        HOST_CTX.with(|host_ctx| unsafe { Context::swap(&mut inst.ctx, &mut *host_ctx.get()) });
    }

    /// Take and return the value passed to
    /// [`Instance::resume_with_val()`](../struct.Instance.html#method.resume_with_val), terminating
    /// the instance if there is no value present, or the dynamic type check of the value fails.
    fn take_resumed_val<R: Any + 'static>(&self) -> R {
        self.try_take_resumed_val()
            .unwrap_or_else(|| panic!(TerminationDetails::YieldTypeMismatch))
    }
}

/// Get an `Instance` from the `vmctx` pointer.
///
/// Only safe to call from within the guest context.
pub unsafe fn instance_from_vmctx<'a>(vmctx: *mut lucet_vmctx) -> &'a mut Instance {
    assert!(!vmctx.is_null(), "vmctx is not null");

    let inst_ptr = (vmctx as usize - instance_heap_offset()) as *mut Instance;

    // We shouldn't actually need to access the thread local, only the exception handler should
    // need to. But, as long as the thread local exists, we should make sure that the guest
    // hasn't pulled any shenanigans and passed a bad vmctx. (Codegen should ensure the guest
    // cant pull any shenanigans but there have been bugs before.)
    CURRENT_INSTANCE.with(|current_instance| {
        if let Some(current_inst_ptr) = current_instance.borrow().map(|nn| nn.as_ptr()) {
            assert_eq!(
                inst_ptr, current_inst_ptr,
                "vmctx corresponds to current instance"
            );
        } else {
            panic!(
                "current instance is not set; thread local storage failure can indicate \
                 dynamic linking issues"
            );
        }
    });

    let inst = inst_ptr.as_mut().unwrap();
    assert!(inst.valid_magic());
    inst
}

impl Instance {
    /// Terminate the guest and swap back to the host context without unwinding.
    ///
    /// This is almost certainly not what you want to use to terminate from a hostcall; use panics
    /// with `TerminationDetails` instead.
    unsafe fn terminate(&mut self, details: TerminationDetails) -> ! {
        self.state = State::Terminating { details };
        #[allow(unused_unsafe)] // The following unsafe will be incorrectly warned as unused
        HOST_CTX.with(|host_ctx| unsafe { Context::set(&*host_ctx.get()) })
    }
}
