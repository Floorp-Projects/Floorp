use core::mem;
use core::ptr;

use Fail;
use backtrace::Backtrace;

pub(crate) struct ErrorImpl {
    inner: Box<Inner<Fail>>,
}

struct Inner<F: ?Sized + Fail> {
    backtrace: Backtrace,
    pub(crate) failure: F,
}

impl<F: Fail> From<F> for ErrorImpl {
    fn from(failure: F) -> ErrorImpl {
        let inner: Inner<F> = {
            let backtrace = if failure.backtrace().is_none() {
                Backtrace::new()
            } else { Backtrace::none() };
            Inner { failure, backtrace }
        };
        ErrorImpl { inner: Box::new(inner) }
    }
}

impl ErrorImpl {
    pub(crate) fn failure(&self) -> &Fail {
        &self.inner.failure
    }

    pub(crate) fn failure_mut(&mut self) -> &mut Fail {
        &mut self.inner.failure
    }

    pub(crate) fn backtrace(&self) -> &Backtrace {
        &self.inner.backtrace
    }

    pub(crate) fn downcast<T: Fail>(self) -> Result<T, ErrorImpl> {
        let ret: Option<T> = self.failure().downcast_ref().map(|fail| {
            unsafe {
                // drop the backtrace
                let _ = ptr::read(&self.inner.backtrace as *const Backtrace);
                // read out the fail type
                ptr::read(fail as *const T)
            }
        });
        match ret {
            Some(ret) => {
                // forget self (backtrace is dropped, failure is moved
                mem::forget(self);
                Ok(ret)
            }
            _ => Err(self)
        }
    }
}
