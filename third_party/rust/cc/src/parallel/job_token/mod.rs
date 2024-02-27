use std::{mem::MaybeUninit, sync::Once};

use crate::Error;

#[cfg(unix)]
#[path = "unix.rs"]
mod sys;

#[cfg(windows)]
#[path = "windows.rs"]
mod sys;

pub(crate) struct JobToken();

impl Drop for JobToken {
    fn drop(&mut self) {
        match JobTokenServer::new() {
            JobTokenServer::Inherited(jobserver) => jobserver.release_token_raw(),
            JobTokenServer::InProcess(jobserver) => jobserver.release_token_raw(),
        }
    }
}

enum JobTokenServer {
    Inherited(inherited_jobserver::JobServer),
    InProcess(inprocess_jobserver::JobServer),
}

impl JobTokenServer {
    /// This function returns a static reference to the jobserver because
    ///  - creating a jobserver from env is a bit fd-unsafe (e.g. the fd might
    ///    be closed by other jobserver users in the process) and better do it
    ///    at the start of the program.
    ///  - in case a jobserver cannot be created from env (e.g. it's not
    ///    present), we will create a global in-process only jobserver
    ///    that has to be static so that it will be shared by all cc
    ///    compilation.
    fn new() -> &'static Self {
        static INIT: Once = Once::new();
        static mut JOBSERVER: MaybeUninit<JobTokenServer> = MaybeUninit::uninit();

        unsafe {
            INIT.call_once(|| {
                let server = inherited_jobserver::JobServer::from_env()
                    .map(Self::Inherited)
                    .unwrap_or_else(|| Self::InProcess(inprocess_jobserver::JobServer::new()));
                JOBSERVER = MaybeUninit::new(server);
            });
            // TODO: Poor man's assume_init_ref, as that'd require a MSRV of 1.55.
            &*JOBSERVER.as_ptr()
        }
    }
}

pub(crate) struct ActiveJobTokenServer(&'static JobTokenServer);

impl ActiveJobTokenServer {
    pub(crate) fn new() -> Result<Self, Error> {
        let jobserver = JobTokenServer::new();

        #[cfg(unix)]
        if let JobTokenServer::Inherited(inherited_jobserver) = &jobserver {
            inherited_jobserver.enter_active()?;
        }

        Ok(Self(jobserver))
    }

    pub(crate) fn try_acquire(&self) -> Result<Option<JobToken>, Error> {
        match &self.0 {
            JobTokenServer::Inherited(jobserver) => jobserver.try_acquire(),
            JobTokenServer::InProcess(jobserver) => Ok(jobserver.try_acquire()),
        }
    }
}

impl Drop for ActiveJobTokenServer {
    fn drop(&mut self) {
        #[cfg(unix)]
        if let JobTokenServer::Inherited(inherited_jobserver) = &self.0 {
            inherited_jobserver.exit_active();
        }
    }
}

mod inherited_jobserver {
    use super::{sys, Error, JobToken};

    use std::{
        env::var_os,
        sync::atomic::{
            AtomicBool,
            Ordering::{AcqRel, Acquire},
        },
    };

    #[cfg(unix)]
    use std::sync::{Mutex, MutexGuard, PoisonError};

    pub(crate) struct JobServer {
        /// Implicit token for this process which is obtained and will be
        /// released in parent. Since JobTokens only give back what they got,
        /// there should be at most one global implicit token in the wild.
        ///
        /// Since Rust does not execute any `Drop` for global variables,
        /// we can't just put it back to jobserver and then re-acquire it at
        /// the end of the process.
        global_implicit_token: AtomicBool,
        inner: sys::JobServerClient,
        /// number of active clients is required to know when it is safe to clear non-blocking
        /// flags
        #[cfg(unix)]
        active_clients_cnt: Mutex<usize>,
    }

    impl JobServer {
        pub(super) unsafe fn from_env() -> Option<Self> {
            let var = var_os("CARGO_MAKEFLAGS")
                .or_else(|| var_os("MAKEFLAGS"))
                .or_else(|| var_os("MFLAGS"))?;

            #[cfg(unix)]
            let var = std::os::unix::ffi::OsStrExt::as_bytes(var.as_os_str());
            #[cfg(not(unix))]
            let var = var.to_str()?.as_bytes();

            let makeflags = var.split(u8::is_ascii_whitespace);

            // `--jobserver-auth=` is the only documented makeflags.
            // `--jobserver-fds=` is actually an internal only makeflags, so we should
            // always prefer `--jobserver-auth=`.
            //
            // Also, according to doc of makeflags, if there are multiple `--jobserver-auth=`
            // the last one is used
            if let Some(flag) = makeflags
                .clone()
                .filter_map(|s| s.strip_prefix(b"--jobserver-auth="))
                .last()
            {
                sys::JobServerClient::open(flag)
            } else {
                sys::JobServerClient::open(
                    makeflags
                        .filter_map(|s| s.strip_prefix(b"--jobserver-fds="))
                        .last()?,
                )
            }
            .map(|inner| Self {
                inner,
                global_implicit_token: AtomicBool::new(true),
                #[cfg(unix)]
                active_clients_cnt: Mutex::new(0),
            })
        }

        #[cfg(unix)]
        fn get_locked_active_cnt(&self) -> MutexGuard<'_, usize> {
            self.active_clients_cnt
                .lock()
                .unwrap_or_else(PoisonError::into_inner)
        }

        #[cfg(unix)]
        pub(super) fn enter_active(&self) -> Result<(), Error> {
            let mut active_cnt = self.get_locked_active_cnt();
            if *active_cnt == 0 {
                self.inner.prepare_for_acquires()?;
            }

            *active_cnt += 1;

            Ok(())
        }

        #[cfg(unix)]
        pub(super) fn exit_active(&self) {
            let mut active_cnt = self.get_locked_active_cnt();
            *active_cnt -= 1;

            if *active_cnt == 0 {
                self.inner.done_acquires();
            }
        }

        pub(super) fn try_acquire(&self) -> Result<Option<JobToken>, Error> {
            if !self.global_implicit_token.swap(false, AcqRel) {
                // Cold path, no global implicit token, obtain one
                if self.inner.try_acquire()?.is_none() {
                    return Ok(None);
                }
            }
            Ok(Some(JobToken()))
        }

        pub(super) fn release_token_raw(&self) {
            // All tokens will be put back into the jobserver immediately
            // and they cannot be cached, since Rust does not call `Drop::drop`
            // on global variables.
            if self
                .global_implicit_token
                .compare_exchange(false, true, AcqRel, Acquire)
                .is_err()
            {
                // There's already a global implicit token, so this token must
                // be released back into jobserver
                let _ = self.inner.release();
            }
        }
    }
}

mod inprocess_jobserver {
    use super::JobToken;

    use std::{
        env::var,
        sync::atomic::{
            AtomicU32,
            Ordering::{AcqRel, Acquire},
        },
    };

    pub(crate) struct JobServer(AtomicU32);

    impl JobServer {
        pub(super) fn new() -> Self {
            // Use `NUM_JOBS` if set (it's configured by Cargo) and otherwise
            // just fall back to a semi-reasonable number.
            //
            // Note that we could use `num_cpus` here but it's an extra
            // dependency that will almost never be used, so
            // it's generally not too worth it.
            let mut parallelism = 4;
            // TODO: Use std::thread::available_parallelism as an upper bound
            // when MSRV is bumped.
            if let Ok(amt) = var("NUM_JOBS") {
                if let Ok(amt) = amt.parse() {
                    parallelism = amt;
                }
            }

            Self(AtomicU32::new(parallelism))
        }

        pub(super) fn try_acquire(&self) -> Option<JobToken> {
            let res = self
                .0
                .fetch_update(AcqRel, Acquire, |tokens| tokens.checked_sub(1));

            res.ok().map(|_| JobToken())
        }

        pub(super) fn release_token_raw(&self) {
            self.0.fetch_add(1, AcqRel);
        }
    }
}
