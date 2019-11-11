use crate::instance::{FaultDetails, TerminationDetails};
use failure::Fail;

/// Lucet runtime errors.
#[derive(Debug, Fail)]
pub enum Error {
    #[fail(display = "Invalid argument: {}", _0)]
    InvalidArgument(&'static str),

    /// A [`Region`](trait.Region.html) cannot currently accommodate additional instances.
    #[fail(display = "Region capacity reached: {} instances", _0)]
    RegionFull(usize),

    /// A module error occurred.
    #[fail(display = "Module error: {}", _0)]
    ModuleError(ModuleError),

    /// A method call or module specification would exceed an instance's
    /// [`Limit`s](struct.Limits.html).
    #[fail(display = "Instance limits exceeded: {}", _0)]
    LimitsExceeded(String),

    /// A method call attempted to modify linear memory for an instance that
    /// does not have linear memory
    #[fail(display = "No linear memory available: {}", _0)]
    NoLinearMemory(String),

    /// An attempt to look up a WebAssembly function by its symbol name failed.
    #[fail(display = "Symbol not found: {}", _0)]
    SymbolNotFound(String),

    /// An attempt to look up a WebAssembly function by its table index failed.
    #[fail(display = "Function not found: (table {}, func {})", _0, _1)]
    FuncNotFound(u32, u32),

    /// An instance aborted due to a runtime fault.
    #[fail(display = "Runtime fault: {:?}", _0)]
    RuntimeFault(FaultDetails),

    /// An instance terminated, potentially with extra information about the termination.
    ///
    /// This condition can arise from a hostcall explicitly calling
    /// [`Vmctx::terminate()`](vmctx/struct.Vmctx.html#method.terminate), or via a custom signal handler
    /// that returns [`SignalBehavior::Terminate`](enum.SignalBehavior.html#variant.Terminate).
    #[fail(display = "Runtime terminated")]
    RuntimeTerminated(TerminationDetails),

    /// IO errors arising during dynamic loading with [`DlModule`](struct.DlModule.html).
    #[fail(display = "Dynamic loading error: {}", _0)]
    DlError(#[cause] std::io::Error),

    #[fail(display = "Instance not returned")]
    InstanceNotReturned,

    #[fail(display = "Instance not yielded")]
    InstanceNotYielded,

    #[fail(display = "Start function yielded")]
    StartYielded,

    /// A catch-all for internal errors that are likely unrecoverable by the runtime user.
    ///
    /// As the API matures, these will likely become rarer, replaced by new variants of this enum,
    /// or by panics for truly unrecoverable situations.
    #[fail(display = "Internal error: {}", _0)]
    InternalError(#[cause] failure::Error),

    /// An unsupported feature was used.
    #[fail(display = "Unsupported feature: {}", _0)]
    Unsupported(String),
}

impl From<failure::Error> for Error {
    fn from(e: failure::Error) -> Error {
        Error::InternalError(e)
    }
}

impl From<crate::context::Error> for Error {
    fn from(e: crate::context::Error) -> Error {
        Error::InternalError(e.into())
    }
}

impl From<nix::Error> for Error {
    fn from(e: nix::Error) -> Error {
        Error::InternalError(e.into())
    }
}

impl From<std::ffi::IntoStringError> for Error {
    fn from(e: std::ffi::IntoStringError) -> Error {
        Error::InternalError(e.into())
    }
}

impl From<lucet_module::Error> for Error {
    fn from(e: lucet_module::Error) -> Error {
        Error::ModuleError(ModuleError::ModuleDataError(e))
    }
}

/// Lucet module errors.
#[derive(Debug, Fail)]
pub enum ModuleError {
    /// An error was found in the definition of a Lucet module.
    #[fail(display = "Incorrect module definition: {}", _0)]
    IncorrectModule(String),

    /// An error occurred with the module data section, likely during deserialization.
    #[fail(display = "Module data error: {}", _0)]
    ModuleDataError(#[cause] lucet_module::Error),
}

#[macro_export]
macro_rules! lucet_bail {
    ($e:expr) => {
        return Err(lucet_format_err!($e));
    };
    ($fmt:expr, $($arg:tt)*) => {
        return Err(lucet_format_err!($fmt, $($arg)*));
    };
}

#[macro_export(local_inner_macros)]
macro_rules! lucet_ensure {
    ($cond:expr, $e:expr) => {
        if !($cond) {
            lucet_bail!($e);
        }
    };
    ($cond:expr, $fmt:expr, $($arg:tt)*) => {
        if !($cond) {
            lucet_bail!($fmt, $($arg)*);
        }
    };
}

#[macro_export]
macro_rules! lucet_format_err {
    ($($arg:tt)*) => { $crate::error::Error::InternalError(failure::format_err!($($arg)*)) }
}

#[macro_export]
macro_rules! lucet_incorrect_module {
    ($($arg:tt)*) => {
        $crate::error::Error::ModuleError(
            $crate::error::ModuleError::IncorrectModule(format!($($arg)*))
        )
    }
}

#[macro_export]
macro_rules! bail_limits_exceeded {
    ($($arg:tt)*) => { return Err($crate::error::Error::LimitsExceeded(format!($($arg)*))); }
}
