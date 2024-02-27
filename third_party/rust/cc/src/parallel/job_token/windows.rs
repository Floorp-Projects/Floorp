use std::{ffi::CString, io, ptr, str};

use crate::windows::windows_sys::{
    OpenSemaphoreA, ReleaseSemaphore, WaitForSingleObject, FALSE, HANDLE, SEMAPHORE_MODIFY_STATE,
    THREAD_SYNCHRONIZE, WAIT_ABANDONED, WAIT_FAILED, WAIT_OBJECT_0, WAIT_TIMEOUT,
};

pub(super) struct JobServerClient {
    sem: HANDLE,
}

unsafe impl Sync for JobServerClient {}
unsafe impl Send for JobServerClient {}

impl JobServerClient {
    pub(super) unsafe fn open(var: &[u8]) -> Option<Self> {
        let var = str::from_utf8(var).ok()?;
        if !var.is_ascii() {
            // `OpenSemaphoreA` only accepts ASCII, not utf-8.
            //
            // Upstream implementation jobserver and jobslot also uses the
            // same function and they works without problem, so there's no
            // motivation to support utf-8 here using `OpenSemaphoreW`
            // which only makes the code harder to maintain by making it more
            // different than upstream.
            return None;
        }

        let name = CString::new(var).ok()?;

        let sem = OpenSemaphoreA(
            THREAD_SYNCHRONIZE | SEMAPHORE_MODIFY_STATE,
            FALSE,
            name.as_bytes().as_ptr(),
        );
        if sem != ptr::null_mut() {
            Some(Self { sem })
        } else {
            None
        }
    }

    pub(super) fn try_acquire(&self) -> io::Result<Option<()>> {
        match unsafe { WaitForSingleObject(self.sem, 0) } {
            WAIT_OBJECT_0 => Ok(Some(())),
            WAIT_TIMEOUT => Ok(None),
            WAIT_FAILED => Err(io::Error::last_os_error()),
            // We believe this should be impossible for a semaphore, but still
            // check the error code just in case it happens.
            WAIT_ABANDONED => Err(io::Error::new(
                io::ErrorKind::Other,
                "Wait on jobserver semaphore returned WAIT_ABANDONED",
            )),
            _ => unreachable!("Unexpected return value from WaitForSingleObject"),
        }
    }

    pub(super) fn release(&self) -> io::Result<()> {
        // SAFETY: ReleaseSemaphore will write to prev_count is it is Some
        // and release semaphore self.sem by 1.
        let r = unsafe { ReleaseSemaphore(self.sem, 1, ptr::null_mut()) };
        if r != 0 {
            Ok(())
        } else {
            Err(io::Error::last_os_error())
        }
    }
}
