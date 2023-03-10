use thiserror::Error;

#[derive(Error, Debug)]
pub enum Error {
    #[error("a request used the CONNECT method")]
    ConnectUnsupported,
    #[error("a field contained invalid Unicode: {0}")]
    CharacterEncoding(#[from] std::string::FromUtf8Error),
    #[error("a field contained an integer value that was out of range: {0}")]
    IntRange(#[from] std::num::TryFromIntError),
    #[error("the mode of the message was invalid")]
    InvalidMode,
    #[error("IO error {0}")]
    Io(#[from] std::io::Error),
    #[error("a field or line was missing a necessary character 0x{0:x}")]
    Missing(u8),
    #[error("a URL was missing a key component")]
    MissingUrlComponent,
    #[error("an obs-fold line was the first line of a field section")]
    ObsFold,
    #[error("a field contained a non-integer value: {0}")]
    ParseInt(#[from] std::num::ParseIntError),
    #[error("a field was truncated")]
    Truncated,
    #[error("a message included the Upgrade field")]
    UpgradeUnsupported,
    #[error("a URL could not be parsed into components: {0}")]
    #[cfg(feature = "read-http")]
    UrlParse(#[from] url::ParseError),
}

#[cfg(any(
    feature = "read-http",
    feature = "write-http",
    feature = "read-bhttp",
    feature = "write-bhttp"
))]
pub type Res<T> = Result<T, Error>;
