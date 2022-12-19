#[derive(Debug)]
pub enum Error {
    /// A request used the CONNECT method.
    ConnectUnsupported,
    /// A field contained invalid Unicode.
    CharacterEncoding(std::string::FromUtf8Error),
    /// A field contained an integer value that was out of range.
    IntRange(std::num::TryFromIntError),
    /// The mode of the message was invalid.
    InvalidMode,
    /// An IO error.
    Io(std::io::Error),
    /// A field or line was missing a necessary character.
    Missing(u8),
    /// A URL was missing a key component.
    MissingUrlComponent,
    /// An obs-fold line was the first line of a field section.
    ObsFold,
    /// A field contained a non-integer value.
    ParseInt(std::num::ParseIntError),
    /// A field was truncated.
    Truncated,
    /// A message included the Upgrade field.
    UpgradeUnsupported,
    /// A URL could not be parsed into components.
    UrlParse(url::ParseError),
}

macro_rules! forward_errors {
    {$($(#[$a:meta])* $t:path => $v:ident),* $(,)?} => {
        $(
            impl From<$t> for Error {
                fn from(e: $t) -> Self {
                    Self::$v(e)
                }
            }
        )*

        impl std::error::Error for Error {
            fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
                match self {
                    $( $(#[$a])* Self::$v(e) => Some(e), )*
                    _ => None,
                }
            }
        }
    };
}

forward_errors! {
    std::io::Error => Io,
    std::string::FromUtf8Error => CharacterEncoding,
    std::num::ParseIntError => ParseInt,
    std::num::TryFromIntError => IntRange,
    url::ParseError => UrlParse,
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "{:?}", self)
    }
}

#[cfg(any(
    feature = "read-http",
    feature = "write-http",
    feature = "read-bhttp",
    feature = "write-bhttp"
))]
pub type Res<T> = Result<T, Error>;
