#![allow(improper_ctypes)]

#[cfg(test)]
mod tests;

use crate::val::{val_to_reg, val_to_stack, RegVal, UntypedRetVal, Val};
use failure::Fail;
use nix;
use nix::sys::signal;
use std::arch::x86_64::{__m128, _mm_setzero_ps};
use std::mem;
use std::ptr::NonNull;
use xfailure::xbail;

/// Callee-saved general-purpose registers in the AMD64 ABI.
///
/// # Layout
///
/// `repr(C)` is required to preserve the ordering of members, which are read by the assembly at
/// hard-coded offsets.
///
/// # TODOs
///
/// - Unlike the C code, this doesn't use the `packed` repr due to warnings in the Nomicon:
/// <https://doc.rust-lang.org/nomicon/other-reprs.html#reprpacked>. Since the members are all
/// `u64`, this should be fine?
#[repr(C)]
#[derive(Clone)]
struct GpRegs {
    rbx: u64,
    rsp: u64,
    rbp: u64,
    rdi: u64,
    r12: u64,
    r13: u64,
    r14: u64,
    r15: u64,
}

impl GpRegs {
    fn new() -> Self {
        GpRegs {
            rbx: 0,
            rsp: 0,
            rbp: 0,
            rdi: 0,
            r12: 0,
            r13: 0,
            r14: 0,
            r15: 0,
        }
    }
}

/// Floating-point argument registers in the AMD64 ABI.
///
/// # Layout
///
/// `repr(C)` is required to preserve the ordering of members, which are read by the assembly at
/// hard-coded offsets.
///
/// # TODOs
///
/// - Unlike the C code, this doesn't use the `packed` repr due to warnings in the Nomicon:
/// <https://doc.rust-lang.org/nomicon/other-reprs.html#reprpacked>. Since the members are all
/// `__m128`, this should be fine?
#[repr(C)]
#[derive(Clone)]
struct FpRegs {
    xmm0: __m128,
    xmm1: __m128,
    xmm2: __m128,
    xmm3: __m128,
    xmm4: __m128,
    xmm5: __m128,
    xmm6: __m128,
    xmm7: __m128,
}

impl FpRegs {
    fn new() -> Self {
        let zero = unsafe { _mm_setzero_ps() };
        FpRegs {
            xmm0: zero,
            xmm1: zero,
            xmm2: zero,
            xmm3: zero,
            xmm4: zero,
            xmm5: zero,
            xmm6: zero,
            xmm7: zero,
        }
    }
}

/// Everything we need to make a context switch: a signal mask, and the registers and return values
/// that are manipulated directly by assembly code.
///
/// # Layout
///
/// The `repr(C)` and order of fields in this struct are very important, as the assembly code reads
/// and writes hard-coded offsets from the base of the struct. Without `repr(C)`, Rust is free to
/// reorder the fields.
///
/// Contexts are also `repr(align(64))` in order to align to cache lines and minimize contention
/// when running multiple threads.
///
/// # Movement
///
/// `Context` values must not be moved once they've been initialized. Contexts contain a pointer to
/// their stack, which in turn contains a pointer back to the context. If the context gets moved,
/// that pointer becomes invalid, and the behavior of returning from that context becomes undefined.
#[repr(C, align(64))]
#[derive(Clone)]
pub struct Context {
    gpr: GpRegs,
    fpr: FpRegs,
    retvals_gp: [u64; 2],
    retval_fp: __m128,
    sigset: signal::SigSet,
}

impl Context {
    /// Create an all-zeroed `Context`.
    pub fn new() -> Self {
        Context {
            gpr: GpRegs::new(),
            fpr: FpRegs::new(),
            retvals_gp: [0; 2],
            retval_fp: unsafe { _mm_setzero_ps() },
            sigset: signal::SigSet::empty(),
        }
    }
}

/// A wrapper around a `Context`, primarily meant for use in test code.
///
/// Users of this library interact with contexts implicitly via `Instance` values, but for testing
/// the context code independently, it is helpful to use contexts directly.
///
/// # Movement of `ContextHandle`
///
/// `ContextHandle` keeps a pointer to a `Context` rather than keeping all of the data directly as
/// fields in order to have better control over where that data lives in memory. We always want that
/// data to be heap-allocated, and to never move once it has been initialized. The `ContextHandle`,
/// by contrast, should be treated like a normal Rust value with no such restrictions.
///
/// Until the `Unpin` marker trait arrives in stable Rust, it is difficult to enforce this with the
/// type system alone, so we use a bit of unsafety and (hopefully) clever API design to ensure that
/// the data cannot be moved.
///
/// We create the `Context` within a box to allocate it on the heap, then convert it into a raw
/// pointer to relinquish ownership. When accessing the internal structure via the `DerefMut` trait,
/// data must not be moved out of the `Context` with functions like `mem::replace`.
///
/// # Layout
///
/// Foreign code accesses the `internal` pointer in tests, so it is important that it is the first
/// member, and that the struct is `repr(C)`.
#[repr(C)]
pub struct ContextHandle {
    internal: NonNull<Context>,
}

impl Drop for ContextHandle {
    fn drop(&mut self) {
        unsafe {
            // create a box from the pointer so that it'll get dropped
            // and we won't leak `Context`s
            Box::from_raw(self.internal.as_ptr());
        }
    }
}

impl std::ops::Deref for ContextHandle {
    type Target = Context;
    fn deref(&self) -> &Self::Target {
        unsafe { self.internal.as_ref() }
    }
}

impl std::ops::DerefMut for ContextHandle {
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { self.internal.as_mut() }
    }
}

impl ContextHandle {
    /// Create an all-zeroed `ContextHandle`.
    pub fn new() -> Self {
        let internal = NonNull::new(Box::into_raw(Box::new(Context::new())))
            .expect("Box::into_raw should never return NULL");
        ContextHandle { internal }
    }

    pub fn create_and_init(
        stack: &mut [u64],
        parent: &mut ContextHandle,
        fptr: usize,
        args: &[Val],
    ) -> Result<ContextHandle, Error> {
        let mut child = ContextHandle::new();
        Context::init(stack, parent, &mut child, fptr, args)?;
        Ok(child)
    }
}

impl Context {
    /// Initialize a new child context.
    ///
    /// - `stack`: The stack for the child; *must be 16-byte aligned*.
    ///
    /// - `parent`: The context that the child will return to. Since `swap` initializes the fields
    /// in its `from` argument, this will typically be an empty context from `ContextHandle::zero()`
    /// that will later be passed to `swap`.
    ///
    /// - `child`: The context for the child. The fields of this structure will be overwritten by
    /// `init`.
    ///
    /// - `fptr`: A pointer to the entrypoint for the child. Note that while the type signature here
    /// is for a void function of no arguments (equivalent to `void (*fptr)(void)` in C), the
    /// entrypoint actually can be a function of any argument or return type that corresponds to a
    /// `val::Val` variant.
    ///
    /// - `args`: A slice of arguments for the `fptr` entrypoint. These must match the number and
    /// types of `fptr`'s actual arguments exactly, otherwise swapping to this context will cause
    /// undefined behavior.
    ///
    /// # Errors
    ///
    /// - `Error::UnalignedStack` if the _end_ of `stack` is not 16-byte aligned.
    ///
    /// # Examples
    ///
    /// ## C entrypoint
    ///
    /// This example initializes a context that will start in a C function `entrypoint` when first
    /// swapped to.
    ///
    /// ```c
    /// void entrypoint(uint64_t x, float y);
    /// ```
    ///
    /// ```no_run
    /// # use lucet_runtime_internals::context::Context;
    /// # use lucet_runtime_internals::val::Val;
    /// extern "C" { fn entrypoint(x: u64, y: f32); }
    /// // allocating an even number of `u64`s seems to reliably yield
    /// // properly aligned stacks, but TODO do better
    /// let mut stack = vec![0u64; 1024].into_boxed_slice();
    /// let mut parent = Context::new();
    /// let mut child = Context::new();
    /// let res = Context::init(
    ///     &mut *stack,
    ///     &mut parent,
    ///     &mut child,
    ///     entrypoint as usize,
    ///     &[Val::U64(120), Val::F32(3.14)],
    /// );
    /// assert!(res.is_ok());
    /// ```
    ///
    /// ## Rust entrypoint
    ///
    /// This example initializes a context that will start in a Rust function `entrypoint` when
    /// first swapped to. Note that we mark `entrypoint` as `extern "C"` to make sure it is compiled
    /// with C calling conventions.
    ///
    /// ```no_run
    /// # use lucet_runtime_internals::context::{Context, ContextHandle};
    /// # use lucet_runtime_internals::val::Val;
    /// extern "C" fn entrypoint(x: u64, y: f32) { }
    /// // allocating an even number of `u64`s seems to reliably yield
    /// // properly aligned stacks, but TODO do better
    /// let mut stack = vec![0u64; 1024].into_boxed_slice();
    /// let mut parent = ContextHandle::new();
    /// let mut child = Context::new();
    /// let res = Context::init(
    ///     &mut *stack,
    ///     &mut parent,
    ///     &mut child,
    ///     entrypoint as usize,
    ///     &[Val::U64(120), Val::F32(3.14)],
    /// );
    /// assert!(res.is_ok());
    /// ```
    pub fn init(
        stack: &mut [u64],
        parent: &mut Context,
        child: &mut Context,
        fptr: usize,
        args: &[Val],
    ) -> Result<(), Error> {
        if !stack_is_aligned(stack) {
            xbail!(Error::UnalignedStack);
        }

        let mut gp_args_ix = 0;
        let mut fp_args_ix = 0;

        let mut spilled_args = vec![];

        for arg in args {
            match val_to_reg(arg) {
                RegVal::GpReg(v) => {
                    if gp_args_ix >= 6 {
                        spilled_args.push(arg);
                    } else {
                        child.bootstrap_gp_ix_arg(gp_args_ix, v);
                        gp_args_ix += 1;
                    }
                }
                RegVal::FpReg(v) => {
                    if fp_args_ix >= 8 {
                        spilled_args.push(arg);
                    } else {
                        child.bootstrap_fp_ix_arg(fp_args_ix, v);
                        fp_args_ix += 1;
                    }
                }
            }
        }

        // the top of the stack; should not be used as an index, always subtracted from
        let sp = stack.len();

        let stack_start = 3 // the bootstrap ret addr, then guest func ret addr, then the backstop ret addr
        + spilled_args.len() // then any args to guest func that don't fit in registers
        + spilled_args.len() % 2 // padding to keep the stack 16-byte aligned when we spill an odd number of spilled arguments
        + 4; // then the backstop args and terminator

        // stack-saved arguments start 3 below the top of the stack
        // (TODO: a diagram would be great here)
        let mut stack_args_ix = 3;

        // If there are more additional args to the guest function than available registers, they
        // have to be pushed on the stack underneath the return address.
        for arg in spilled_args {
            let v = val_to_stack(arg);
            stack[sp + stack_args_ix - stack_start] = v;
            stack_args_ix += 1;
        }

        // Prepare the stack for a swap context that lands in the bootstrap function swap will ret
        // into the bootstrap function
        stack[sp + 0 - stack_start] = lucet_context_bootstrap as u64;

        // The bootstrap function returns into the guest function, fptr
        stack[sp + 1 - stack_start] = fptr as u64;

        // the guest function returns into lucet_context_backstop.
        stack[sp + 2 - stack_start] = lucet_context_backstop as u64;

        // if fptr ever returns, it returns to the backstop func. backstop needs two arguments in
        // its frame - first the context we are switching *out of* (which is also the one we are
        // creating right now) and the ctx we switch back into. Note *parent might not be a valid
        // ctx now, but it should be when this ctx is started.
        stack[sp - 4] = child as *mut Context as u64;
        stack[sp - 3] = parent as *mut Context as u64;
        // Terminate the call chain.
        stack[sp - 2] = 0;
        stack[sp - 1] = 0;

        // RSP, RBP, and sigset still remain to be initialized.
        // Stack pointer: this has the return address of the first function to be run on the swap.
        child.gpr.rsp = &mut stack[sp - stack_start] as *mut u64 as u64;
        // Frame pointer: this is only used by the backstop code. It uses it to locate the ctx and
        // parent arguments set above.
        child.gpr.rbp = &mut stack[sp - 2] as *mut u64 as u64;

        // Read the sigprocmask to be restored if we ever need to jump out of a signal handler. If
        // this isn't possible, die.
        signal::sigprocmask(
            signal::SigmaskHow::SIG_SETMASK,
            None,
            Some(&mut child.sigset),
        )
        .expect("sigprocmask could not be retrieved");

        Ok(())
    }

    /// Save the current context, and swap to another context.
    ///
    /// - `from`: the current context is written here
    /// - `to`: the context to read from and swap to
    ///
    /// The current registers, including the stack pointer, are saved to `from`. The current stack
    /// pointer is then replaced by the value saved in `to.gpr.rsp`, so when `swap` returns, it will
    /// return to the pointer saved in `to`'s stack.
    ///
    /// If `to` was freshly initialized by passing it as the child to `init`, `swap` will return to
    /// the function that bootstraps arguments and then calls the entrypoint that was passed to
    /// `init`.
    ///
    /// If `to` was previously passed as the `from` argument to another call to `swap`, the program
    /// will return as if from that _first_ call to `swap`.
    ///
    /// # Safety
    ///
    /// The value in `to.gpr.rsp` must be a valid pointer into the stack that was originally passed
    /// to `init` when the `to` context was initialized, or to the original stack created implicitly
    /// by Rust.
    ///
    /// The registers saved in the `to` context must match the arguments expected by the entrypoint
    /// of the function passed to `init`, or be unaltered from when they were previously written by
    /// `swap`.
    ///
    /// If `from` is never returned to, `swap`ped to, or `set` to, resources could leak due to
    /// implicit `drop`s never being called:
    ///
    /// ```no_run
    /// # use lucet_runtime_internals::context::Context;
    /// fn f(x: Box<u64>, child: &Context) {
    ///     let mut xs = vec![187; 410757864530];
    ///     xs[0] += *x;
    ///
    ///     // manually drop here to avoid leaks
    ///     drop(x);
    ///     drop(xs);
    ///
    ///     let mut parent = Context::new();
    ///     unsafe { Context::swap(&mut parent, child); }
    ///     // implicit `drop(x)` and `drop(xs)` here never get called unless we swap back
    /// }
    /// ```
    ///
    /// # Examples
    ///
    /// The typical case is to initialize a new child context, and then swap to it from a zeroed
    /// parent context.
    ///
    /// ```no_run
    /// # use lucet_runtime_internals::context::Context;
    /// # extern "C" fn entrypoint() {}
    /// # let mut stack = vec![0u64; 1024].into_boxed_slice();
    /// let mut parent = Context::new();
    /// let mut child = Context::new();
    /// Context::init(
    ///     &mut stack,
    ///     &mut parent,
    ///     &mut child,
    ///     entrypoint as usize,
    ///     &[],
    /// ).unwrap();
    ///
    /// unsafe { Context::swap(&mut parent, &child); }
    /// ```
    #[inline]
    pub unsafe fn swap(from: &mut Context, to: &Context) {
        lucet_context_swap(from as *mut Context, to as *const Context);
    }

    /// Swap to another context without saving the current context.
    ///
    /// - `to`: the context to read from and swap to
    ///
    /// The current registers, including the stack pointer, are discarded. The current stack pointer
    /// is then replaced by the value saved in `to.gpr.rsp`, so when `swap` returns, it will return
    /// to the pointer saved in `to`'s stack.
    ///
    /// If `to` was freshly initialized by passing it as the child to `init`, `swap` will return to
    /// the function that bootstraps arguments and then calls the entrypoint that was passed to
    /// `init`.
    ///
    /// If `to` was previously passed as the `from` argument to another call to `swap`, the program
    /// will return as if from the call to `swap`.
    ///
    /// # Safety
    ///
    /// ## Stack and registers
    ///
    /// The value in `to.gpr.rsp` must be a valid pointer into the stack that was originally passed
    /// to `init` when the context was initialized, or to the original stack created implicitly by
    /// Rust.
    ///
    /// The registers saved in `to` must match the arguments expected by the entrypoint of the
    /// function passed to `init`, or be unaltered from when they were previously written by `swap`.
    ///
    /// ## Returning
    ///
    /// If `to` is a context freshly initialized by `init`, at least one of the following must be
    /// true, otherwise the program will return to a context with uninitialized registers:
    ///
    /// - The `fptr` argument to `init` is a function that never returns
    ///
    /// - The `parent` argument to `init` was passed as the `from` argument to `swap` before this
    /// call to `set`
    ///
    /// ## Resource leaks
    ///
    /// Since control flow will not return to the calling context, care must be taken to ensure that
    /// any resources owned by the calling context are manually dropped. The implicit `drop`s
    /// inserted by Rust at the end of the calling scope will not be reached:
    ///
    /// ```no_run
    /// # use lucet_runtime_internals::context::Context;
    /// fn f(x: Box<u64>, child: &Context) {
    ///     let mut xs = vec![187; 410757864530];
    ///     xs[0] += *x;
    ///
    ///     // manually drop here to avoid leaks
    ///     drop(x);
    ///     drop(xs);
    ///
    ///     unsafe { Context::set(child); }
    ///     // implicit `drop(x)` and `drop(xs)` here never get called
    /// }
    /// ```
    #[inline]
    pub unsafe fn set(to: &Context) -> ! {
        lucet_context_set(to as *const Context);
    }

    #[inline]
    pub unsafe fn get_current_stack_pointer() -> u64 {
        return lucet_get_current_stack_pointer();
    }

    /// Like `set`, but also manages the return from a signal handler.
    ///
    /// TODO: the return type of this function should really be `Result<!, nix::Error>`, but using
    /// `!` as a type like that is currently experimental.
    #[inline]
    pub unsafe fn set_from_signal(to: &Context) -> Result<(), nix::Error> {
        signal::sigprocmask(signal::SigmaskHow::SIG_SETMASK, Some(&to.sigset), None)?;
        Context::set(to)
    }

    /// Clear (zero) return values.
    pub fn clear_retvals(&mut self) {
        self.retvals_gp = [0; 2];
        let zero = unsafe { _mm_setzero_ps() };
        self.retval_fp = zero;
    }

    /// Get the general-purpose return value at index `idx`.
    ///
    /// If this method is called before the context has returned from its original entrypoint, the
    /// result will be `0`.
    pub fn get_retval_gp(&self, idx: usize) -> u64 {
        self.retvals_gp[idx]
    }

    /// Get the floating point return value.
    ///
    /// If this method is called before the context has returned from its original entrypoint, the
    /// result will be `0.0`.
    pub fn get_retval_fp(&self) -> __m128 {
        self.retval_fp
    }

    /// Get the return value as an `UntypedRetVal`.
    ///
    /// This combines the 0th general-purpose return value, and the single floating-point return value.
    pub fn get_untyped_retval(&self) -> UntypedRetVal {
        let gp = self.get_retval_gp(0);
        let fp = self.get_retval_fp();
        UntypedRetVal::new(gp, fp)
    }

    /// Put one of the first 6 general-purpose arguments into a `Context` register.
    ///
    /// Although these registers are callee-saved registers rather than argument registers, they get
    /// moved into argument registers by `lucet_context_bootstrap`.
    ///
    /// - `ix`: ABI general-purpose argument number
    /// - `arg`: argument value
    fn bootstrap_gp_ix_arg(&mut self, ix: usize, arg: u64) {
        match ix {
            // rdi lives across bootstrap
            0 => self.gpr.rdi = arg,
            // bootstraps into rsi
            1 => self.gpr.r12 = arg,
            // bootstraps into rdx
            2 => self.gpr.r13 = arg,
            // bootstraps into rcx
            3 => self.gpr.r14 = arg,
            // bootstraps into r8
            4 => self.gpr.r15 = arg,
            // bootstraps into r9
            5 => self.gpr.rbx = arg,
            _ => panic!("unexpected gp register index {}", ix),
        }
    }

    /// Put one of the first 8 floating-point arguments into a `Context` register.
    ///
    /// - `ix`: ABI floating-point argument number
    /// - `arg`: argument value
    fn bootstrap_fp_ix_arg(&mut self, ix: usize, arg: __m128) {
        match ix {
            0 => self.fpr.xmm0 = arg,
            1 => self.fpr.xmm1 = arg,
            2 => self.fpr.xmm2 = arg,
            3 => self.fpr.xmm3 = arg,
            4 => self.fpr.xmm4 = arg,
            5 => self.fpr.xmm5 = arg,
            6 => self.fpr.xmm6 = arg,
            7 => self.fpr.xmm7 = arg,
            _ => panic!("unexpected fp register index {}", ix),
        }
    }
}

/// Errors that may arise when working with contexts.
#[derive(Debug, Fail)]
pub enum Error {
    /// Raised when the bottom of the stack provided to `Context::init` is not 16-byte aligned
    #[fail(display = "context initialized with unaligned stack")]
    UnalignedStack,
}

/// Check whether the bottom (highest address) of the stack is 16-byte aligned, as required by the
/// ABI.
fn stack_is_aligned(stack: &[u64]) -> bool {
    let size = stack.len();
    let last_elt_addr = &stack[size - 1] as *const u64 as usize;
    let bottom_addr = last_elt_addr + mem::size_of::<u64>();
    bottom_addr % 16 == 0
}

extern "C" {
    /// Bootstraps arguments and calls the entrypoint via returning; implemented in assembly.
    ///
    /// Loads general-purpose arguments from the callee-saved registers in a `Context` to the
    /// appropriate argument registers for the AMD64 ABI, and then returns to the entrypoint.
    fn lucet_context_bootstrap();

    /// Stores return values into the parent context, and then swaps to it; implemented in assembly.
    ///
    /// This is where the entrypoint function returns to, so that we swap back to the parent on
    /// return.
    fn lucet_context_backstop();

    /// Saves the current context and performs the context switch. Implemented in assembly.
    fn lucet_context_swap(from: *mut Context, to: *const Context);

    /// Performs the context switch; implemented in assembly.
    ///
    /// Never returns because the current context is discarded.
    fn lucet_context_set(to: *const Context) -> !;

    fn lucet_get_current_stack_pointer() -> u64;
}
