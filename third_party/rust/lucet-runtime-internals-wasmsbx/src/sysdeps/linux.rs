use libc::{c_void, ucontext_t, REG_RIP};

#[derive(Clone, Copy, Debug)]
pub struct UContextPtr(*const ucontext_t);

impl UContextPtr {
    #[inline]
    pub fn new(ptr: *const c_void) -> Self {
        assert!(!ptr.is_null(), "non-null context");
        UContextPtr(ptr as *const ucontext_t)
    }

    #[inline]
    pub fn get_ip(self) -> *const c_void {
        let mcontext = &unsafe { *(self.0) }.uc_mcontext;
        mcontext.gregs[REG_RIP as usize] as *const _
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct UContext {
    context: ucontext_t,
}

impl UContext {
    #[inline]
    pub fn new(ptr: *const c_void) -> Self {
        UContext {
            context: *unsafe {
                (ptr as *const ucontext_t)
                    .as_ref()
                    .expect("non-null context")
            },
        }
    }

    pub fn as_ptr(&mut self) -> UContextPtr {
        UContextPtr::new(&self.context as *const _ as *const _)
    }
}

impl Into<UContext> for UContextPtr {
    #[inline]
    fn into(self) -> UContext {
        UContext {
            context: unsafe { *(self.0) },
        }
    }
}
