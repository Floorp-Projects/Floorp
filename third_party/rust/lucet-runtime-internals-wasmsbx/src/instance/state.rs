use crate::instance::siginfo_ext::SiginfoExt;
use crate::instance::{FaultDetails, TerminationDetails, YieldedVal};
use crate::sysdeps::UContext;
use libc::{SIGBUS, SIGSEGV};
use std::any::Any;
use std::ffi::{CStr, CString};

/// The representation of a Lucet instance's state machine.
pub enum State {
    /// The instance is ready to run.
    ///
    /// Transitions to `Running` when the instance is run, or to `Ready` when it's reset.
    Ready,

    /// The instance is running.
    ///
    /// Transitions to `Ready` when the guest function returns normally, or to `Faulted`,
    /// `Terminating`, or `Yielding` if the instance faults, terminates, or yields.
    Running,

    /// The instance has faulted, potentially fatally.
    ///
    /// Transitions to `Faulted` when filling in additional fault details, to `Running` if
    /// re-running a non-fatally faulted instance, or to `Ready` when the instance is reset.
    Faulted {
        details: FaultDetails,
        siginfo: libc::siginfo_t,
        context: UContext,
    },

    /// The instance is in the process of terminating.
    ///
    /// Transitions only to `Terminated`; the `TerminationDetails` are always extracted into a
    /// `RunResult` before anything else happens to the instance.
    Terminating { details: TerminationDetails },

    /// The instance has terminated, and must be reset before running again.
    ///
    /// Transitions to `Ready` if the instance is reset.
    Terminated,

    /// The instance is in the process of yielding.
    ///
    /// Transitions only to `Yielded`; the `YieldedVal` is always extracted into a
    /// `RunResult` before anything else happens to the instance.
    Yielding {
        val: YieldedVal,
        /// A phantom value carrying the type of the expected resumption value.
        ///
        /// Concretely, this should only ever be `Box<PhantomData<R>>` where `R` is the type
        /// the guest expects upon resumption.
        expecting: Box<dyn Any>,
    },

    /// The instance has yielded.
    ///
    /// Transitions to `Running` if the instance is resumed, or to `Ready` if the instance is reset.
    Yielded {
        /// A phantom value carrying the type of the expected resumption value.
        ///
        /// Concretely, this should only ever be `Box<PhantomData<R>>` where `R` is the type
        /// the guest expects upon resumption.
        expecting: Box<dyn Any>,
    },

    /// A placeholder state used with `std::mem::replace()` when a new state must be constructed by
    /// moving values out of an old state.
    ///
    /// This is used so that we do not need a `Clone` impl for this type, which would add
    /// unnecessary constraints to the types of values instances could yield or terminate with.
    ///
    /// It is an error for this state to appear outside of a transition between other states.
    Transitioning,
}

impl std::fmt::Display for State {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            State::Ready => write!(f, "ready"),
            State::Running => write!(f, "running"),
            State::Faulted {
                details, siginfo, ..
            } => {
                write!(f, "{}", details)?;
                write!(
                    f,
                    " triggered by {}: ",
                    strsignal_wrapper(siginfo.si_signo)
                        .into_string()
                        .expect("strsignal returns valid UTF-8")
                )?;

                if siginfo.si_signo == SIGSEGV || siginfo.si_signo == SIGBUS {
                    // We know this is inside the heap guard, because by the time we get here,
                    // `lucet_error_verify_trap_safety` will have run and validated it.
                    write!(
                        f,
                        " accessed memory at {:p} (inside heap guard)",
                        siginfo.si_addr_ext()
                    )?;
                }
                Ok(())
            }
            State::Terminated { .. } => write!(f, "terminated"),
            State::Terminating { .. } => write!(f, "terminating"),
            State::Yielding { .. } => write!(f, "yielding"),
            State::Yielded { .. } => write!(f, "yielded"),
            State::Transitioning { .. } => {
                write!(f, "transitioning (IF YOU SEE THIS, THERE'S PROBABLY A BUG)")
            }
        }
    }
}

impl State {
    pub fn is_ready(&self) -> bool {
        if let State::Ready { .. } = self {
            true
        } else {
            false
        }
    }

    pub fn is_running(&self) -> bool {
        if let State::Running = self {
            true
        } else {
            false
        }
    }

    pub fn is_fault(&self) -> bool {
        if let State::Faulted { .. } = self {
            true
        } else {
            false
        }
    }

    pub fn is_fatal(&self) -> bool {
        if let State::Faulted {
            details: FaultDetails { fatal, .. },
            ..
        } = self
        {
            *fatal
        } else {
            false
        }
    }

    pub fn is_terminated(&self) -> bool {
        if let State::Terminated { .. } = self {
            true
        } else {
            false
        }
    }

    pub fn is_yielded(&self) -> bool {
        if let State::Yielded { .. } = self {
            true
        } else {
            false
        }
    }
}

// TODO: PR into `libc`
extern "C" {
    #[no_mangle]
    fn strsignal(sig: libc::c_int) -> *mut libc::c_char;
}

// TODO: PR into `nix`
fn strsignal_wrapper(sig: libc::c_int) -> CString {
    unsafe { CStr::from_ptr(strsignal(sig)).to_owned() }
}
