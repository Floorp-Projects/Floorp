use Error;
use header::{HeaderName, HeaderValue};
use method::Method;
use sealed::Sealed;
use status::StatusCode;
use uri::{Scheme, Authority, PathAndQuery, Uri};

/// Private trait for the `http` crate to have generic methods with fallible
/// conversions.
///
/// This trait is similar to the `TryFrom` trait proposed in the standard
/// library, except this is specialized for the `http` crate and isn't intended
/// for general consumption.
///
/// This trait cannot be implemented types outside of the `http` crate, and is
/// only intended for use as a generic bound on methods in the `http` crate.
pub trait HttpTryFrom<T>: Sized + Sealed {
    /// Associated error with the conversion this implementation represents.
    type Error: Into<Error>;

    #[doc(hidden)]
    fn try_from(t: T) -> Result<Self, Self::Error>;
}

pub(crate) trait HttpTryInto<T>: Sized {
    fn http_try_into(self) -> Result<T, Error>;
}

#[doc(hidden)]
impl<T, U> HttpTryInto<U> for T
where
    U: HttpTryFrom<T>,
    T: Sized,
{
    fn http_try_into(self) -> Result<U, Error> {
        HttpTryFrom::try_from(self)
            .map_err(|e: U::Error| e.into())
    }
}

macro_rules! reflexive {
    ($($t:ty,)*) => ($(
        impl HttpTryFrom<$t> for $t {
            type Error = Error;

            fn try_from(t: Self) -> Result<Self, Self::Error> {
                Ok(t)
            }
        }

        impl Sealed for $t {}
    )*)
}

reflexive! {
    Uri,
    Method,
    StatusCode,
    HeaderName,
    HeaderValue,
    Scheme,
    Authority,
    PathAndQuery,
}
