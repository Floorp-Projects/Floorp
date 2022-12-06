#[derive(Debug)]
pub enum Error {
    /// A problem occurred with the AEAD.
    #[cfg(feature = "rust-hpke")]
    Aead(aead::Error),
    /// A problem occurred during cryptographic processing.
    #[cfg(feature = "nss")]
    Crypto(crate::nss::Error),
    /// An error was found in the format.
    Format,
    /// A problem occurred with HPKE.
    #[cfg(feature = "rust-hpke")]
    Hpke(::hpke::HpkeError),
    /// An internal error occurred.
    Internal,
    /// The wrong type of key was provided for the selected KEM.
    InvalidKeyType,
    /// The wrong KEM was specified.
    InvalidKem,
    /// An IO error.
    Io(std::io::Error),
    /// The key ID was invalid.
    KeyId,
    /// A field was truncated.
    Truncated,
    /// The configuration was not supported.
    Unsupported,
    /// The configuration contained too many symmetric suites.
    TooManySymmetricSuites,
}

macro_rules! forward_errors {
    {$($(#[$m:meta])* $t:path => $v:ident),* $(,)?} => {
        $(
            $(#[$m])*
            impl From<$t> for Error {
                fn from(e: $t) -> Self {
                    Self::$v(e)
                }
            }
        )*

        impl std::error::Error for Error {
            fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
                match self {
                    $( $(#[$m])* Self::$v(e) => Some(e), )*
                    _ => None,
                }
            }
        }
    };
}

forward_errors! {
    #[cfg(feature = "rust-hpke")]
    aead::Error => Aead,
    #[cfg(feature = "nss")]
    crate::nss::Error => Crypto,
    #[cfg(feature = "rust-hpke")]
    ::hpke::HpkeError => Hpke,
    std::io::Error => Io,
}

impl From<std::num::TryFromIntError> for Error {
    fn from(_v: std::num::TryFromIntError) -> Self {
        Self::TooManySymmetricSuites
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "{:?}", self)
    }
}

pub type Res<T> = Result<T, Error>;
