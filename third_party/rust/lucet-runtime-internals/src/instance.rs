mod siginfo_ext;
pub mod signals;
pub mod state;

pub use crate::instance::signals::{signal_handler_none, SignalBehavior, SignalHandler};
pub use crate::instance::state::State;

use crate::alloc::{Alloc, HOST_PAGE_SIZE_EXPECTED};
use crate::context::Context;
use crate::embed_ctx::CtxMap;
use crate::error::Error;
use crate::module::{self, FunctionHandle, FunctionPointer, Global, GlobalValue, Module, TrapCode};
use crate::region::RegionInternal;
use crate::val::{UntypedRetVal, Val};
use crate::WASM_PAGE_SIZE;
use libc::{c_void, siginfo_t, uintptr_t};
use lucet_module::InstanceRuntimeData;
use memoffset::offset_of;
use std::any::Any;
use std::cell::{BorrowError, BorrowMutError, Ref, RefCell, RefMut, UnsafeCell};
use std::marker::PhantomData;
use std::mem;
use std::ops::{Deref, DerefMut};
use std::ptr::{self, NonNull};
use std::sync::Arc;

pub const LUCET_INSTANCE_MAGIC: u64 = 746932922;

thread_local! {
    /// The host context.
    ///
    /// Control returns here implicitly due to the setup in `Context::init()` when guest functions
    /// return normally. Control can return here explicitly from signal handlers when the guest
    /// program needs to be terminated.
    ///
    /// This is an `UnsafeCell` due to nested borrows. The context must be borrowed mutably when
    /// swapping to the guest context, which means that borrow exists for the entire time the guest
    /// function runs even though the mutation to the host context is done only at the beginning of
    /// the swap. Meanwhile, the signal handler can run at any point during the guest function, and
    /// so it also must be able to immutably borrow the host context if it needs to swap back. The
    /// runtime borrowing constraints for a `RefCell` are therefore too strict for this variable.
    pub(crate) static HOST_CTX: UnsafeCell<Context> = UnsafeCell::new(Context::new());

    /// The currently-running `Instance`, if one exists.
    pub(crate) static CURRENT_INSTANCE: RefCell<Option<NonNull<Instance>>> = RefCell::new(None);
}

/// A smart pointer to an [`Instance`](struct.Instance.html) that properly manages cleanup when dropped.
///
/// Instances are always stored in memory backed by a `Region`; we never want to create one directly
/// with the Rust allocator. This type allows us to abide by that rule while also having an owned
/// type that cleans up the instance when we are done with it.
///
/// Since this type implements `Deref` and `DerefMut` to `Instance`, it can usually be treated as
/// though it were a `&mut Instance`.
pub struct InstanceHandle {
    inst: NonNull<Instance>,
    needs_inst_drop: bool,
}

// raw pointer lint
unsafe impl Send for InstanceHandle {}

/// Create a new `InstanceHandle`.
///
/// This is not meant for public consumption, but rather is used to make implementations of
/// `Region`.
///
/// # Safety
///
/// This function runs the guest code for the WebAssembly `start` section, and running any guest
/// code is potentially unsafe; see [`Instance::run()`](struct.Instance.html#method.run).
pub fn new_instance_handle(
    instance: *mut Instance,
    module: Arc<dyn Module>,
    alloc: Alloc,
    embed_ctx: CtxMap,
) -> Result<InstanceHandle, Error> {
    let inst = NonNull::new(instance)
        .ok_or(lucet_format_err!("instance pointer is null; this is a bug"))?;

    lucet_ensure!(
        unsafe { inst.as_ref().magic } != LUCET_INSTANCE_MAGIC,
        "created a new instance handle in memory with existing instance magic; this is a bug"
    );

    let mut handle = InstanceHandle {
        inst,
        needs_inst_drop: false,
    };

    let inst = Instance::new(alloc, module, embed_ctx);

    unsafe {
        // this is wildly unsafe! you must be very careful to not let the drop impls run on the
        // uninitialized fields; see
        // <https://doc.rust-lang.org/std/mem/fn.forget.html#use-case-1>

        // write the whole struct into place over the uninitialized page
        ptr::write(&mut *handle, inst);
    };

    handle.needs_inst_drop = true;

    handle.reset()?;

    Ok(handle)
}

pub fn instance_handle_to_raw(mut inst: InstanceHandle) -> *mut Instance {
    inst.needs_inst_drop = false;
    inst.inst.as_ptr()
}

pub unsafe fn instance_handle_from_raw(
    ptr: *mut Instance,
    needs_inst_drop: bool,
) -> InstanceHandle {
    InstanceHandle {
        inst: NonNull::new_unchecked(ptr),
        needs_inst_drop,
    }
}

// Safety argument for these deref impls: the instance's `Alloc` field contains an `Arc` to the
// region that backs this memory, keeping the page containing the `Instance` alive as long as the
// region exists

impl Deref for InstanceHandle {
    type Target = Instance;
    fn deref(&self) -> &Self::Target {
        unsafe { self.inst.as_ref() }
    }
}

impl DerefMut for InstanceHandle {
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { self.inst.as_mut() }
    }
}

impl Drop for InstanceHandle {
    fn drop(&mut self) {
        if self.needs_inst_drop {
            unsafe {
                let inst = self.inst.as_mut();

                // Grab a handle to the region to ensure it outlives `inst`.
                //
                // This ensures that the region won't be dropped by `inst` being
                // dropped, which could result in `inst` being unmapped by the
                // Region *during* drop of the Instance's fields.
                let region: Arc<dyn RegionInternal> = inst.alloc().region.clone();

                // drop the actual instance
                std::ptr::drop_in_place(inst);

                // and now we can drop what may be the last Arc<Region>. If it is
                // it can safely do what it needs with memory; we're not running
                // destructors on it anymore.
                mem::drop(region);
            }
        }
    }
}

/// A Lucet program, together with its dedicated memory and signal handlers.
///
/// This is the primary interface for running programs, examining return values, and accessing the
/// WebAssembly heap.
///
/// `Instance`s are never created by runtime users directly, but rather are acquired from
/// [`Region`](../region/trait.Region.html)s and often accessed through
/// [`InstanceHandle`](../instance/struct.InstanceHandle.html) smart pointers. This guarantees that instances
/// and their fields are never moved in memory, otherwise raw pointers in the metadata could be
/// unsafely invalidated.
///
/// An instance occupies one 4096-byte page in memory, with a layout like:
/// ```text
/// 0xXXXXX000:
///   Instance {
///     .magic
///     .embed_ctx
///      ... etc ...
///   }
///
///   // unused space
///
///   InstanceInternals {
///     .globals
///     .instruction_counter
///   } // last address *inside* `InstanceInternals` is 0xXXXXXFFF
/// 0xXXXXY000: // start of next page, VMContext points here
///   Heap {
///     ..
///   }
/// ```
///
/// This layout allows modules to tightly couple to a handful of fields related to the instance,
/// rather than possibly requiring compiler-side changes (and recompiles) whenever `Instance`
/// changes.
///
/// It also obligates `Instance` to be immediately followed by the heap, but otherwise leaves the
/// locations of the stack, globals, and any other data, to be implementation-defined by the
/// `Region` that actually creates `Slot`s onto which `Instance` are mapped.
/// For information about the layout of all instance-related memory, see the documentation of
/// [MmapRegion](../region/mmap/struct.MmapRegion.html).
#[repr(C)]
#[repr(align(4096))]
pub struct Instance {
    /// Used to catch bugs in pointer math used to find the address of the instance
    magic: u64,

    /// The embedding context is a map containing embedder-specific values that are used to
    /// implement hostcalls
    pub(crate) embed_ctx: CtxMap,

    /// The program (WebAssembly module) that is the entrypoint for the instance.
    module: Arc<dyn Module>,

    /// The `Context` in which the guest program runs
    pub(crate) ctx: Context,

    /// Instance state and error information
    pub(crate) state: State,

    /// The memory allocated for this instance
    alloc: Alloc,

    /// Handler run for signals that do not arise from a known WebAssembly trap, or that involve
    /// memory outside of the current instance.
    fatal_handler: fn(&Instance) -> !,

    /// A fatal handler set from C
    c_fatal_handler: Option<unsafe extern "C" fn(*mut Instance)>,

    /// Handler run when `SIGBUS`, `SIGFPE`, `SIGILL`, or `SIGSEGV` are caught by the instance thread.
    signal_handler: Box<
        dyn Fn(
            &Instance,
            &Option<TrapCode>,
            libc::c_int,
            *const siginfo_t,
            *const c_void,
        ) -> SignalBehavior,
    >,

    /// Pointer to the function used as the entrypoint (for use in backtraces)
    entrypoint: Option<FunctionPointer>,

    /// The value passed back to the guest when resuming a yielded instance.
    pub(crate) resumed_val: Option<Box<dyn Any + 'static>>,

    /// `_padding` must be the last member of the structure.
    /// This marks where the padding starts to make the structure exactly 4096 bytes long.
    /// It is also used to compute the size of the structure up to that point, i.e. without padding.
    _padding: (),
}

/// Users of `Instance` must be very careful about when instances are dropped!
///
/// Typically you will not have to worry about this, as InstanceHandle will robustly handle
/// Instance drop semantics. If an instance is dropped, and the Region it's in has already dropped,
/// it may contain the last reference counted pointer to its Region. If so, when Instance's
/// destructor runs, Region will be dropped, and may free or otherwise invalidate the memory that
/// this Instance exists in, *while* the Instance destructor is executing.
impl Drop for Instance {
    fn drop(&mut self) {
        // Reset magic to indicate this instance
        // is no longer valid
        self.magic = 0;
    }
}

/// The result of running or resuming an [`Instance`](struct.Instance.html).
#[derive(Debug)]
pub enum RunResult {
    /// An instance returned with a value.
    ///
    /// The actual type of the contained value depends on the return type of the guest function that
    /// was called. For guest functions with no return value, it is undefined behavior to do
    /// anything with this value.
    Returned(UntypedRetVal),
    /// An instance yielded, potentially with a value.
    ///
    /// This arises when a hostcall invokes one of the
    /// [`Vmctx::yield_*()`](vmctx/struct.Vmctx.html#method.yield_) family of methods. Depending on which
    /// variant is used, the `YieldedVal` may contain a value passed from the guest context to the
    /// host.
    ///
    /// An instance that has yielded may only be resumed
    /// ([with](struct.Instance.html#method.resume_with_val) or
    /// [without](struct.Instance.html#method.resume) a value to returned to the guest),
    /// [reset](struct.Instance.html#method.reset), or dropped. Attempting to run an instance from a
    /// new entrypoint after it has yielded but without first resetting will result in an error.
    Yielded(YieldedVal),
}

impl RunResult {
    /// Try to get a return value from a run result, returning `Error::InstanceNotReturned` if the
    /// instance instead yielded.
    pub fn returned(self) -> Result<UntypedRetVal, Error> {
        match self {
            RunResult::Returned(rv) => Ok(rv),
            RunResult::Yielded(_) => Err(Error::InstanceNotReturned),
        }
    }

    /// Try to get a reference to a return value from a run result, returning
    /// `Error::InstanceNotReturned` if the instance instead yielded.
    pub fn returned_ref(&self) -> Result<&UntypedRetVal, Error> {
        match self {
            RunResult::Returned(rv) => Ok(rv),
            RunResult::Yielded(_) => Err(Error::InstanceNotReturned),
        }
    }

    /// Returns `true` if the instance returned a value.
    pub fn is_returned(&self) -> bool {
        self.returned_ref().is_ok()
    }

    /// Unwraps a run result into a return value.
    ///
    /// # Panics
    ///
    /// Panics if the instance instead yielded, with a panic message including the passed message.
    pub fn expect_returned(self, msg: &str) -> UntypedRetVal {
        self.returned().expect(msg)
    }

    /// Unwraps a run result into a returned value.
    ///
    /// # Panics
    ///
    /// Panics if the instance instead yielded.
    pub fn unwrap_returned(self) -> UntypedRetVal {
        self.returned().unwrap()
    }

    /// Try to get a yielded value from a run result, returning `Error::InstanceNotYielded` if the
    /// instance instead returned.
    pub fn yielded(self) -> Result<YieldedVal, Error> {
        match self {
            RunResult::Returned(_) => Err(Error::InstanceNotYielded),
            RunResult::Yielded(yv) => Ok(yv),
        }
    }

    /// Try to get a reference to a yielded value from a run result, returning
    /// `Error::InstanceNotYielded` if the instance instead returned.
    pub fn yielded_ref(&self) -> Result<&YieldedVal, Error> {
        match self {
            RunResult::Returned(_) => Err(Error::InstanceNotYielded),
            RunResult::Yielded(yv) => Ok(yv),
        }
    }

    /// Returns `true` if the instance yielded.
    pub fn is_yielded(&self) -> bool {
        self.yielded_ref().is_ok()
    }

    /// Unwraps a run result into a yielded value.
    ///
    /// # Panics
    ///
    /// Panics if the instance instead returned, with a panic message including the passed message.
    pub fn expect_yielded(self, msg: &str) -> YieldedVal {
        self.yielded().expect(msg)
    }

    /// Unwraps a run result into a yielded value.
    ///
    /// # Panics
    ///
    /// Panics if the instance instead returned.
    pub fn unwrap_yielded(self) -> YieldedVal {
        self.yielded().unwrap()
    }
}

/// APIs that are internal, but useful to implementors of extension modules; you probably don't want
/// this trait!
///
/// This is a trait rather than inherent `impl`s in order to keep the `lucet-runtime` API clean and
/// safe.
pub trait InstanceInternal {
    fn alloc(&self) -> &Alloc;
    fn alloc_mut(&mut self) -> &mut Alloc;
    fn module(&self) -> &dyn Module;
    fn state(&self) -> &State;
    fn valid_magic(&self) -> bool;
}

impl InstanceInternal for Instance {
    /// Get a reference to the instance's `Alloc`.
    fn alloc(&self) -> &Alloc {
        &self.alloc
    }

    /// Get a mutable reference to the instance's `Alloc`.
    fn alloc_mut(&mut self) -> &mut Alloc {
        &mut self.alloc
    }

    /// Get a reference to the instance's `Module`.
    fn module(&self) -> &dyn Module {
        self.module.deref()
    }

    /// Get a reference to the instance's `State`.
    fn state(&self) -> &State {
        &self.state
    }

    /// Check whether the instance magic is valid.
    fn valid_magic(&self) -> bool {
        self.magic == LUCET_INSTANCE_MAGIC
    }
}

// Public API
impl Instance {
    /// Run a function with arguments in the guest context at the given entrypoint.
    ///
    /// ```no_run
    /// # use lucet_runtime_internals::instance::InstanceHandle;
    /// # let instance: InstanceHandle = unimplemented!();
    /// // regular execution yields `Ok(UntypedRetVal)`
    /// let retval = instance.run("factorial", &[5u64.into()]).unwrap().unwrap_returned();
    /// assert_eq!(u64::from(retval), 120u64);
    ///
    /// // runtime faults yield `Err(Error)`
    /// let result = instance.run("faulting_function", &[]);
    /// assert!(result.is_err());
    /// ```
    ///
    /// # Safety
    ///
    /// This is unsafe in two ways:
    ///
    /// - The type of the entrypoint might not be correct. It might take a different number or
    /// different types of arguments than are provided to `args`. It might not even point to a
    /// function! We will likely add type information to `lucetc` output so we can dynamically check
    /// the type in the future.
    ///
    /// - The entrypoint is foreign code. While we may be convinced that WebAssembly compiled to
    /// native code by `lucetc` is safe, we do not have the same guarantee for the hostcalls that a
    /// guest may invoke. They might be implemented in an unsafe language, so we must treat this
    /// call as unsafe, just like any other FFI call.
    ///
    /// For the moment, we do not mark this as `unsafe` in the Rust type system, but that may change
    /// in the future.
    pub fn run(&mut self, entrypoint: &str, args: &[Val]) -> Result<RunResult, Error> {
        let func = self.module.get_export_func(entrypoint)?;
        self.run_func(func, &args)
    }

    /// Run a function with arguments in the guest context from the [WebAssembly function
    /// table](https://webassembly.github.io/spec/core/syntax/modules.html#tables).
    ///
    /// # Safety
    ///
    /// The same safety caveats of [`Instance::run()`](struct.Instance.html#method.run) apply.
    pub fn run_func_idx(
        &mut self,
        table_idx: u32,
        func_idx: u32,
        args: &[Val],
    ) -> Result<RunResult, Error> {
        let func = self.module.get_func_from_idx(table_idx, func_idx)?;
        self.run_func(func, &args)
    }

    /// Resume execution of an instance that has yielded without providing a value to the guest.
    ///
    /// This should only be used when the guest yielded with
    /// [`Vmctx::yield_()`](vmctx/struct.Vmctx.html#method.yield_) or
    /// [`Vmctx::yield_val()`](vmctx/struct.Vmctx.html#method.yield_val). Otherwise, this call will
    /// fail with `Error::InvalidArgument`.
    ///
    /// # Safety
    ///
    /// The foreign code safety caveat of [`Instance::run()`](struct.Instance.html#method.run)
    /// applies.
    pub fn resume(&mut self) -> Result<RunResult, Error> {
        self.resume_with_val(EmptyYieldVal)
    }

    /// Resume execution of an instance that has yielded, providing a value to the guest.
    ///
    /// The type of the provided value must match the type expected by
    /// [`Vmctx::yield_expecting_val()`](vmctx/struct.Vmctx.html#method.yield_expecting_val) or
    /// [`Vmctx::yield_val_expecting_val()`](vmctx/struct.Vmctx.html#method.yield_val_expecting_val).
    ///
    /// The provided value will be dynamically typechecked against the type the guest expects to
    /// receive, and if that check fails, this call will fail with `Error::InvalidArgument`.
    ///
    /// # Safety
    ///
    /// The foreign code safety caveat of [`Instance::run()`](struct.Instance.html#method.run)
    /// applies.
    pub fn resume_with_val<A: Any + 'static>(&mut self, val: A) -> Result<RunResult, Error> {
        match &self.state {
            State::Yielded { expecting, .. } => {
                // make sure the resumed value is of the right type
                if !expecting.is::<PhantomData<A>>() {
                    return Err(Error::InvalidArgument(
                        "type mismatch between yielded instance expected value and resumed value",
                    ));
                }
            }
            _ => return Err(Error::InvalidArgument("can only resume a yielded instance")),
        }

        self.resumed_val = Some(Box::new(val) as Box<dyn Any + 'static>);

        self.swap_and_return()
    }

    /// Reset the instance's heap and global variables to their initial state.
    ///
    /// The WebAssembly `start` section will also be run, if one exists.
    ///
    /// The embedder contexts present at instance creation or added with
    /// [`Instance::insert_embed_ctx()`](struct.Instance.html#method.insert_embed_ctx) are not
    /// modified by this call; it is the embedder's responsibility to clear or reset their state if
    /// necessary.
    ///
    /// # Safety
    ///
    /// This function runs the guest code for the WebAssembly `start` section, and running any guest
    /// code is potentially unsafe; see [`Instance::run()`](struct.Instance.html#method.run).
    pub fn reset(&mut self) -> Result<(), Error> {
        self.alloc.reset_heap(self.module.as_ref())?;
        let globals = unsafe { self.alloc.globals_mut() };
        let mod_globals = self.module.globals();
        for (i, v) in mod_globals.iter().enumerate() {
            globals[i] = match v.global() {
                Global::Import { .. } => {
                    return Err(Error::Unsupported(format!(
                        "global imports are unsupported; found: {:?}",
                        i
                    )));
                }
                Global::Def(def) => def.init_val(),
            };
        }

        self.state = State::Ready;

        self.run_start()?;

        Ok(())
    }

    /// Grow the guest memory by the given number of WebAssembly pages.
    ///
    /// On success, returns the number of pages that existed before the call.
    pub fn grow_memory(&mut self, additional_pages: u32) -> Result<u32, Error> {
        let additional_bytes =
            additional_pages
                .checked_mul(WASM_PAGE_SIZE)
                .ok_or(lucet_format_err!(
                    "additional pages larger than wasm address space",
                ))?;
        let orig_len = self
            .alloc
            .expand_heap(additional_bytes, self.module.as_ref())?;
        Ok(orig_len / WASM_PAGE_SIZE)
    }

    /// Return the WebAssembly heap as a slice of bytes.
    pub fn heap(&self) -> &[u8] {
        unsafe { self.alloc.heap() }
    }

    /// Return the WebAssembly heap as a mutable slice of bytes.
    pub fn heap_mut(&mut self) -> &mut [u8] {
        unsafe { self.alloc.heap_mut() }
    }

    /// Return the WebAssembly heap as a slice of `u32`s.
    pub fn heap_u32(&self) -> &[u32] {
        unsafe { self.alloc.heap_u32() }
    }

    /// Return the WebAssembly heap as a mutable slice of `u32`s.
    pub fn heap_u32_mut(&mut self) -> &mut [u32] {
        unsafe { self.alloc.heap_u32_mut() }
    }

    /// Return the WebAssembly globals as a slice of `i64`s.
    pub fn globals(&self) -> &[GlobalValue] {
        unsafe { self.alloc.globals() }
    }

    /// Return the WebAssembly globals as a mutable slice of `i64`s.
    pub fn globals_mut(&mut self) -> &mut [GlobalValue] {
        unsafe { self.alloc.globals_mut() }
    }

    /// Check whether a given range in the host address space overlaps with the memory that backs
    /// the instance heap.
    pub fn check_heap<T>(&self, ptr: *const T, len: usize) -> bool {
        self.alloc.mem_in_heap(ptr, len)
    }

    /// Check whether a context value of a particular type exists.
    pub fn contains_embed_ctx<T: Any>(&self) -> bool {
        self.embed_ctx.contains::<T>()
    }

    /// Get a reference to a context value of a particular type, if it exists.
    pub fn get_embed_ctx<T: Any>(&self) -> Option<Result<Ref<'_, T>, BorrowError>> {
        self.embed_ctx.try_get::<T>()
    }

    /// Get a mutable reference to a context value of a particular type, if it exists.
    pub fn get_embed_ctx_mut<T: Any>(&mut self) -> Option<Result<RefMut<'_, T>, BorrowMutError>> {
        self.embed_ctx.try_get_mut::<T>()
    }

    /// Insert a context value.
    ///
    /// If a context value of the same type already existed, it is returned.
    ///
    /// **Note**: this method is intended for embedder contexts that need to be added _after_ an
    /// instance is created and initialized. To add a context for an instance's entire lifetime,
    /// including the execution of its `start` section, see
    /// [`Region::new_instance_builder()`](trait.Region.html#method.new_instance_builder).
    pub fn insert_embed_ctx<T: Any>(&mut self, x: T) -> Option<T> {
        self.embed_ctx.insert(x)
    }

    /// Remove a context value of a particular type, returning it if it exists.
    pub fn remove_embed_ctx<T: Any>(&mut self) -> Option<T> {
        self.embed_ctx.remove::<T>()
    }

    /// Set the handler run when `SIGBUS`, `SIGFPE`, `SIGILL`, or `SIGSEGV` are caught by the
    /// instance thread.
    ///
    /// In most cases, these signals are unrecoverable for the instance that raised them, but do not
    /// affect the rest of the process.
    ///
    /// The default signal handler returns
    /// [`SignalBehavior::Default`](enum.SignalBehavior.html#variant.Default), which yields a
    /// runtime fault error.
    ///
    /// The signal handler must be
    /// [signal-safe](http://man7.org/linux/man-pages/man7/signal-safety.7.html).
    pub fn set_signal_handler<H>(&mut self, handler: H)
    where
        H: 'static
            + Fn(
                &Instance,
                &Option<TrapCode>,
                libc::c_int,
                *const siginfo_t,
                *const c_void,
            ) -> SignalBehavior,
    {
        self.signal_handler = Box::new(handler) as Box<SignalHandler>;
    }

    /// Set the handler run for signals that do not arise from a known WebAssembly trap, or that
    /// involve memory outside of the current instance.
    ///
    /// Fatal signals are not only unrecoverable for the instance that raised them, but may
    /// compromise the correctness of the rest of the process if unhandled.
    ///
    /// The default fatal handler calls `panic!()`.
    pub fn set_fatal_handler(&mut self, handler: fn(&Instance) -> !) {
        self.fatal_handler = handler;
    }

    /// Set the fatal handler to a C-compatible function.
    ///
    /// This is a separate interface, because C functions can't return the `!` type. Like the
    /// regular `fatal_handler`, it is not expected to return, but we cannot enforce that through
    /// types.
    ///
    /// When a fatal error occurs, this handler is run first, and then the regular `fatal_handler`
    /// runs in case it returns.
    pub fn set_c_fatal_handler(&mut self, handler: unsafe extern "C" fn(*mut Instance)) {
        self.c_fatal_handler = Some(handler);
    }

    #[inline]
    pub fn get_instruction_count(&self) -> u64 {
        self.get_instance_implicits().instruction_count
    }

    #[inline]
    pub fn set_instruction_count(&mut self, instruction_count: u64) {
        self.get_instance_implicits_mut().instruction_count = instruction_count;
    }
}

// Private API
impl Instance {
    fn new(alloc: Alloc, module: Arc<dyn Module>, embed_ctx: CtxMap) -> Self {
        let globals_ptr = alloc.slot().globals as *mut i64;
        let mut inst = Instance {
            magic: LUCET_INSTANCE_MAGIC,
            embed_ctx: embed_ctx,
            module,
            ctx: Context::new(),
            state: State::Ready,
            alloc,
            fatal_handler: default_fatal_handler,
            c_fatal_handler: None,
            signal_handler: Box::new(signal_handler_none) as Box<SignalHandler>,
            entrypoint: None,
            resumed_val: None,
            _padding: (),
        };
        inst.set_globals_ptr(globals_ptr);
        inst.set_instruction_count(0);

        assert_eq!(mem::size_of::<Instance>(), HOST_PAGE_SIZE_EXPECTED);
        let unpadded_size = offset_of!(Instance, _padding);
        assert!(unpadded_size <= HOST_PAGE_SIZE_EXPECTED - mem::size_of::<*mut i64>());
        inst
    }

    // The globals pointer must be stored right before the end of the structure, padded to the page size,
    // so that it is 8 bytes before the heap.
    // For this reason, the alignment of the structure is set to 4096, and we define accessors that
    // read/write the globals pointer as bytes [4096-8..4096] of that structure represented as raw bytes.
    // InstanceRuntimeData is placed such that it ends at the end of the page this `Instance` starts
    // on. So we can access it by *self + PAGE_SIZE - size_of::<InstanceRuntimeData>
    #[inline]
    fn get_instance_implicits(&self) -> &InstanceRuntimeData {
        unsafe {
            let implicits_ptr = (self as *const _ as *const u8)
                .offset((HOST_PAGE_SIZE_EXPECTED - mem::size_of::<InstanceRuntimeData>()) as isize)
                as *const InstanceRuntimeData;
            mem::transmute::<*const InstanceRuntimeData, &InstanceRuntimeData>(implicits_ptr)
        }
    }

    #[inline]
    fn get_instance_implicits_mut(&mut self) -> &mut InstanceRuntimeData {
        unsafe {
            let implicits_ptr = (self as *mut _ as *mut u8)
                .offset((HOST_PAGE_SIZE_EXPECTED - mem::size_of::<InstanceRuntimeData>()) as isize)
                as *mut InstanceRuntimeData;
            mem::transmute::<*mut InstanceRuntimeData, &mut InstanceRuntimeData>(implicits_ptr)
        }
    }

    #[allow(dead_code)]
    #[inline]
    fn get_globals_ptr(&self) -> *mut i64 {
        self.get_instance_implicits().globals_ptr
    }

    #[inline]
    fn set_globals_ptr(&mut self, globals_ptr: *mut i64) {
        self.get_instance_implicits_mut().globals_ptr = globals_ptr
    }

    /// Run a function in guest context at the given entrypoint.
    fn run_func(&mut self, func: FunctionHandle, args: &[Val]) -> Result<RunResult, Error> {
        if !(self.state.is_ready() || (self.state.is_fault() && !self.state.is_fatal())) {
            return Err(Error::InvalidArgument(
                "instance must be ready or non-fatally faulted",
            ));
        }
        if func.ptr.as_usize() == 0 {
            return Err(Error::InvalidArgument(
                "entrypoint function cannot be null; this is probably a malformed module",
            ));
        }

        let sig = self.module.get_signature(func.id);

        // in typechecking these values, we can only really check that arguments are correct.
        // in the future we might want to make return value use more type safe as well.

        if sig.params.len() != args.len() {
            return Err(Error::InvalidArgument(
                "entrypoint function signature mismatch (number of arguments is incorrect)",
            ));
        }

        for (param_ty, arg) in sig.params.iter().zip(args.iter()) {
            if param_ty != &arg.value_type() {
                return Err(Error::InvalidArgument(
                    "entrypoint function signature mismatch",
                ));
            }
        }

        self.entrypoint = Some(func.ptr);

        let mut args_with_vmctx = vec![Val::from(self.alloc.slot().heap)];
        args_with_vmctx.extend_from_slice(args);

        HOST_CTX.with(|host_ctx| {
            Context::init(
                unsafe { self.alloc.stack_u64_mut() },
                unsafe { &mut *host_ctx.get() },
                &mut self.ctx,
                func.ptr.as_usize(),
                &args_with_vmctx,
            )
        })?;

        self.swap_and_return()
    }

    /// The core routine for context switching into a guest, and extracting a result.
    ///
    /// This must only be called for an instance in a ready, non-fatally faulted, or yielded
    /// state. The public wrappers around this function should make sure the state is appropriate.
    fn swap_and_return(&mut self) -> Result<RunResult, Error> {
        debug_assert!(
            self.state.is_ready()
                || (self.state.is_fault() && !self.state.is_fatal())
                || self.state.is_yielded()
        );
        self.state = State::Running;

        // there should never be another instance running on this thread when we enter this function
        CURRENT_INSTANCE.with(|current_instance| {
            let mut current_instance = current_instance.borrow_mut();
            assert!(
                current_instance.is_none(),
                "no other instance is running on this thread"
            );
            // safety: `self` is not null if we are in this function
            *current_instance = Some(unsafe { NonNull::new_unchecked(self) });
        });

        self.with_signals_on(|i| {
            HOST_CTX.with(|host_ctx| {
                // Save the current context into `host_ctx`, and jump to the guest context. The
                // lucet context is linked to host_ctx, so it will return here after it finishes,
                // successfully or otherwise.
                unsafe { Context::swap(&mut *host_ctx.get(), &mut i.ctx) };
                Ok(())
            })
        })?;

        CURRENT_INSTANCE.with(|current_instance| {
            *current_instance.borrow_mut() = None;
        });

        // Sandbox has jumped back to the host process, indicating it has either:
        //
        // * returned: state should be `Running`; transition to `Ready` and return a RunResult
        // * yielded: state should be `Yielding`; transition to `Yielded` and return a RunResult
        // * trapped: state should be `Faulted`; populate details and return an error or call a handler as appropriate
        // * terminated: state should be `Terminating`; transition to `Terminated` and return the termination details as an Err
        //
        // The state should never be `Ready`, `Terminated`, `Yielded`, or `Transitioning` at this point

        // Set transitioning state temporarily so that we can move values out of the current state
        let st = mem::replace(&mut self.state, State::Transitioning);

        match st {
            State::Running => {
                let retval = self.ctx.get_untyped_retval();
                self.state = State::Ready;
                Ok(RunResult::Returned(retval))
            }
            State::Terminating { details, .. } => {
                self.state = State::Terminated;
                Err(Error::RuntimeTerminated(details))
            }
            State::Yielding { val, expecting } => {
                self.state = State::Yielded { expecting };
                Ok(RunResult::Yielded(val))
            }
            State::Faulted {
                mut details,
                siginfo,
                context,
            } => {
                // Sandbox is no longer runnable. It's unsafe to determine all error details in the signal
                // handler, so we fill in extra details here.
                //
                // FIXME after lucet-module is complete it should be possible to fill this in without
                // consulting the process symbol table
                details.rip_addr_details = self
                    .module
                    .addr_details(details.rip_addr as *const c_void)?;

                // fill the state back in with the updated details in case fatal handlers need it
                self.state = State::Faulted {
                    details: details.clone(),
                    siginfo,
                    context,
                };

                if details.fatal {
                    // Some errors indicate that the guest is not functioning correctly or that
                    // the loaded code violated some assumption, so bail out via the fatal
                    // handler.

                    // Run the C-style fatal handler, if it exists.
                    self.c_fatal_handler
                        .map(|h| unsafe { h(self as *mut Instance) });

                    // If there is no C-style fatal handler, or if it (erroneously) returns,
                    // call the Rust handler that we know will not return
                    (self.fatal_handler)(self)
                } else {
                    // leave the full fault details in the instance state, and return the
                    // higher-level info to the user
                    Err(Error::RuntimeFault(details))
                }
            }
            State::Ready | State::Terminated | State::Yielded { .. } | State::Transitioning => Err(
                lucet_format_err!("\"impossible\" state found in `swap_and_return()`: {}", st),
            ),
        }
    }

    pub fn set_current_instance(&mut self)
    {
        // there should never be another instance running on this thread when we enter this function
        CURRENT_INSTANCE.with(|current_instance| {
            let mut current_instance = current_instance.borrow_mut();
            // safety: `self` is not null if we are in this function
            *current_instance = Some(unsafe { NonNull::new_unchecked(self) });
        });
    }

    pub fn clear_current_instance(&mut self)
    {
        CURRENT_INSTANCE.with(|current_instance| {
            *current_instance.borrow_mut() = None;
        });
    }

    /// Run a function in guest context at the given entrypoint.
    pub fn unsafe_run_func_fast(
        &mut self,
        func_ptr: FunctionPointer,
        args: &[Val],
    ) -> Result<RunResult, Error> {
        let prev_entrypoint = self.entrypoint;
        self.entrypoint = Some(func_ptr);

        let mut args_with_vmctx = vec![Val::from(self.alloc.slot().heap)];
        args_with_vmctx.extend_from_slice(args);

        let re_entrant = match &self.state {
            State::Running => true,
            _ => false,
        };

        let saved_host_ctx = HOST_CTX.with(|host_ctx| {
            let chosen_stack_loc;

            if re_entrant {
                let curr_stack_ptr = unsafe { Context::get_current_stack_pointer() };
                // Add some padding as we have to account for the stack used by the rest of the current function body
                let padded_curr_stack_ptr = curr_stack_ptr - 2048;

                let stack_slice = unsafe { self.alloc.stack_mut() };
                let stack_ptr = stack_slice.as_ptr() as u64;

                assert!(padded_curr_stack_ptr >= stack_ptr);

                let mut rem_stack_len = ((padded_curr_stack_ptr - stack_ptr) / 8) as usize;
                // align to 8
                if rem_stack_len % 8 != 0 {
                    rem_stack_len += 8 - rem_stack_len % 8;
                }

                let computed_stack_loc = unsafe {
                    std::slice::from_raw_parts_mut(stack_slice.as_ptr() as *mut u64, rem_stack_len)
                };

                chosen_stack_loc = computed_stack_loc;
            } else {
                chosen_stack_loc = unsafe { self.alloc.stack_u64_mut() };
            }

            let host_ctx_copy = unsafe { (*host_ctx.get()).clone() };

            Context::init(
                chosen_stack_loc,
                unsafe { &mut *host_ctx.get() },
                &mut self.ctx,
                func_ptr.as_usize(),
                &args_with_vmctx,
            )
            .map(|_| host_ctx_copy)
        })?;

        debug_assert!(
            self.state.is_ready()
                || self.state.is_running()
                || (self.state.is_fault() && !self.state.is_fatal())
                || self.state.is_yielded()
        );

        if !re_entrant {
            self.state = State::Running;
        }

        // there should never be another instance running on this thread when we enter this function
        let mut prev_instance = None;
        CURRENT_INSTANCE.with(|current_instance| {
            let mut current_instance = current_instance.borrow_mut();
            prev_instance = *current_instance;
            // safety: `self` is not null if we are in this function
            *current_instance = Some(unsafe { NonNull::new_unchecked(self) });
        });

        // self.with_signals_on(|i| {
        HOST_CTX.with(|host_ctx| {
            // Save the current context into `host_ctx`, and jump to the guest context. The
            // lucet context is linked to host_ctx, so it will return here after it finishes,
            // successfully or otherwise.
            unsafe { Context::swap(&mut *host_ctx.get(), &mut self.ctx) };
            // Ok(())
        });
        // })?;

        if re_entrant {
            HOST_CTX.with(|host_ctx| {
                let host_ctx_ref = unsafe { &mut *host_ctx.get() };
                *host_ctx_ref = saved_host_ctx;
            });
        }

        CURRENT_INSTANCE.with(|current_instance| {
            *current_instance.borrow_mut() = prev_instance;
        });

        self.entrypoint = prev_entrypoint;

        // Sandbox has jumped back to the host process, indicating it has either:
        //
        // * returned: state should be `Running`; transition to `Ready` and return a RunResult
        // * yielded: state should be `Yielding`; transition to `Yielded` and return a RunResult
        // * trapped: state should be `Faulted`; populate details and return an error or call a handler as appropriate
        // * terminated: state should be `Terminating`; transition to `Terminated` and return the termination details as an Err
        //
        // The state should never be `Ready`, `Terminated`, `Yielded`, or `Transitioning` at this point

        // Set transitioning state temporarily so that we can move values out of the current state
        let st = mem::replace(&mut self.state, State::Transitioning);

        match st {
            State::Running => {
                let retval = self.ctx.get_untyped_retval();
                if !re_entrant {
                    self.state = State::Ready;
                } else {
                    self.state = State::Running;
                }
                Ok(RunResult::Returned(retval))
            }
            State::Terminating { details, .. } => {
                self.state = State::Terminated;
                Err(Error::RuntimeTerminated(details))
            }
            State::Yielding { val, expecting } => {
                self.state = State::Yielded { expecting };
                Ok(RunResult::Yielded(val))
            }
            State::Faulted {
                mut details,
                siginfo,
                context,
            } => {
                // Sandbox is no longer runnable. It's unsafe to determine all error details in the signal
                // handler, so we fill in extra details here.
                //
                // FIXME after lucet-module is complete it should be possible to fill this in without
                // consulting the process symbol table
                details.rip_addr_details = self
                    .module
                    .addr_details(details.rip_addr as *const c_void)?;

                // fill the state back in with the updated details in case fatal handlers need it
                self.state = State::Faulted {
                    details: details.clone(),
                    siginfo,
                    context,
                };

                if details.fatal {
                    // Some errors indicate that the guest is not functioning correctly or that
                    // the loaded code violated some assumption, so bail out via the fatal
                    // handler.

                    // Run the C-style fatal handler, if it exists.
                    self.c_fatal_handler
                        .map(|h| unsafe { h(self as *mut Instance) });

                    // If there is no C-style fatal handler, or if it (erroneously) returns,
                    // call the Rust handler that we know will not return
                    (self.fatal_handler)(self)
                } else {
                    // leave the full fault details in the instance state, and return the
                    // higher-level info to the user
                    Err(Error::RuntimeFault(details))
                }
            }
            State::Ready | State::Terminated | State::Yielded { .. } | State::Transitioning => Err(
                lucet_format_err!("\"impossible\" state found in `swap_and_return()`: {}", st),
            ),
        }
    }

    fn run_start(&mut self) -> Result<(), Error> {
        if let Some(start) = self.module.get_start_func()? {
            let res = self.run_func(start, &[])?;
            if res.is_yielded() {
                return Err(Error::StartYielded);
            }
        }
        Ok(())
    }
}

/// Information about a runtime fault.
///
/// Runtime faults are raised implictly by signal handlers that return `SignalBehavior::Default` in
/// response to signals arising while a guest is running.
#[derive(Clone, Debug)]
pub struct FaultDetails {
    /// If true, the instance's `fatal_handler` will be called.
    pub fatal: bool,
    /// Information about the type of fault that occurred.
    pub trapcode: Option<TrapCode>,
    /// The instruction pointer where the fault occurred.
    pub rip_addr: uintptr_t,
    /// Extra information about the instruction pointer's location, if available.
    pub rip_addr_details: Option<module::AddrDetails>,
}

impl std::fmt::Display for FaultDetails {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.fatal {
            write!(f, "fault FATAL ")?;
        } else {
            write!(f, "fault ")?;
        }

        if let Some(trapcode) = self.trapcode {
            write!(f, "{:?} ", trapcode)?;
        } else {
            write!(f, "TrapCode::UNKNOWN ")?;
        }

        write!(f, "code at address {:p}", self.rip_addr as *const c_void)?;

        if let Some(ref addr_details) = self.rip_addr_details {
            if let Some(ref fname) = addr_details.file_name {
                let sname = addr_details
                    .sym_name
                    .as_ref()
                    .map(String::as_str)
                    .unwrap_or("<unknown>");
                write!(f, " (symbol {}:{})", fname, sname)?;
            }
            if addr_details.in_module_code {
                write!(f, " (inside module code)")
            } else {
                write!(f, " (not inside module code)")
            }
        } else {
            write!(f, " (unknown whether in module)")
        }
    }
}

/// Information about a terminated guest.
///
/// Guests are terminated either explicitly by `Vmctx::terminate()`, or implicitly by signal
/// handlers that return `SignalBehavior::Terminate`. It usually indicates that an unrecoverable
/// error has occurred in a hostcall, rather than in WebAssembly code.
pub enum TerminationDetails {
    /// Returned when a signal handler terminates the instance.
    Signal,
    /// Returned when `get_embed_ctx` or `get_embed_ctx_mut` are used with a type that is not present.
    CtxNotFound,
    /// Returned when the type of the value passed to `Instance::resume_with_val()` does not match
    /// the type expected by `Vmctx::yield_expecting_val()` or `Vmctx::yield_val_expecting_val`, or
    /// if `Instance::resume()` was called when a value was expected.
    ///
    /// **Note**: If you see this termination value, please report it as a Lucet bug. The types of
    /// resumed values are dynamically checked by `Instance::resume()` and
    /// `Instance::resume_with_val()`, so this should never arise.
    YieldTypeMismatch,
    /// Returned when dynamic borrowing rules of methods like `Vmctx::heap()` are violated.
    BorrowError(&'static str),
    /// Calls to `lucet_hostcall_terminate` provide a payload for use by the embedder.
    Provided(Box<dyn Any + 'static>),
}

impl TerminationDetails {
    pub fn provide<A: Any + 'static>(details: A) -> Self {
        TerminationDetails::Provided(Box::new(details))
    }
    pub fn provided_details(&self) -> Option<&dyn Any> {
        match self {
            TerminationDetails::Provided(a) => Some(a.as_ref()),
            _ => None,
        }
    }
}

// Because of deref coercions, the code above was tricky to get right-
// test that a string makes it through
#[test]
fn termination_details_any_typing() {
    let hello = "hello, world".to_owned();
    let details = TerminationDetails::provide(hello.clone());
    let provided = details.provided_details().expect("got Provided");
    assert_eq!(
        provided.downcast_ref::<String>().expect("right type"),
        &hello
    );
}

impl PartialEq for TerminationDetails {
    fn eq(&self, rhs: &TerminationDetails) -> bool {
        use TerminationDetails::*;
        match (self, rhs) {
            (Signal, Signal) => true,
            (BorrowError(msg1), BorrowError(msg2)) => msg1 == msg2,
            (CtxNotFound, CtxNotFound) => true,
            // can't compare `Any`
            _ => false,
        }
    }
}

impl std::fmt::Debug for TerminationDetails {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "TerminationDetails::")?;
        match self {
            TerminationDetails::Signal => write!(f, "Signal"),
            TerminationDetails::BorrowError(msg) => write!(f, "BorrowError({})", msg),
            TerminationDetails::CtxNotFound => write!(f, "CtxNotFound"),
            TerminationDetails::YieldTypeMismatch => write!(f, "YieldTypeMismatch"),
            TerminationDetails::Provided(_) => write!(f, "Provided(Any)"),
        }
    }
}

unsafe impl Send for TerminationDetails {}
unsafe impl Sync for TerminationDetails {}

/// The value yielded by an instance through a [`Vmctx`](vmctx/struct.Vmctx.html) and returned to
/// the host.
pub struct YieldedVal {
    val: Box<dyn Any + 'static>,
}

impl std::fmt::Debug for YieldedVal {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.is_none() {
            write!(f, "YieldedVal {{ val: None }}")
        } else {
            write!(f, "YieldedVal {{ val: Some }}")
        }
    }
}

impl YieldedVal {
    pub(crate) fn new<A: Any + 'static>(val: A) -> Self {
        YieldedVal { val: Box::new(val) }
    }

    /// Returns `true` if the guest yielded without a value.
    pub fn is_none(&self) -> bool {
        self.val.is::<EmptyYieldVal>()
    }

    /// Returns `true` if the guest yielded with a value.
    pub fn is_some(&self) -> bool {
        !self.is_none()
    }

    /// Attempt to downcast the yielded value to a concrete type, returning the original
    /// `YieldedVal` if unsuccessful.
    pub fn downcast<A: Any + 'static + Send + Sync>(self) -> Result<Box<A>, YieldedVal> {
        match self.val.downcast() {
            Ok(val) => Ok(val),
            Err(val) => Err(YieldedVal { val }),
        }
    }

    /// Returns a reference to the yielded value if it is present and of type `A`, or `None` if it
    /// isn't.
    pub fn downcast_ref<A: Any + 'static + Send + Sync>(&self) -> Option<&A> {
        self.val.downcast_ref()
    }
}

/// A marker value to indicate a yield or resume with no value.
///
/// This exists to unify the implementations of the various operators, and should only ever be
/// created by internal code.
#[derive(Debug)]
pub(crate) struct EmptyYieldVal;

fn default_fatal_handler(inst: &Instance) -> ! {
    panic!("> instance {:p} had fatal error: {}", inst, inst.state);
}
