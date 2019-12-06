use std::fmt;
use std::time::SystemTime;

use humantime::{format_rfc3339_nanos, format_rfc3339_seconds};

use ::fmt::Formatter;

pub(in ::fmt) mod glob {
    pub use super::*;
}

impl Formatter {
    /// Get a [`Timestamp`] for the current date and time in UTC.
    ///
    /// # Examples
    ///
    /// Include the current timestamp with the log record:
    ///
    /// ```
    /// use std::io::Write;
    ///
    /// let mut builder = env_logger::Builder::new();
    ///
    /// builder.format(|buf, record| {
    ///     let ts = buf.timestamp();
    ///
    ///     writeln!(buf, "{}: {}: {}", ts, record.level(), record.args())
    /// });
    /// ```
    ///
    /// [`Timestamp`]: struct.Timestamp.html
    pub fn timestamp(&self) -> Timestamp {
        Timestamp(SystemTime::now())
    }

    /// Get a [`PreciseTimestamp`] for the current date and time in UTC with nanos.
    pub fn precise_timestamp(&self) -> PreciseTimestamp {
        PreciseTimestamp(SystemTime::now())
    }
}

/// An [RFC3339] formatted timestamp.
///
/// The timestamp implements [`Display`] and can be written to a [`Formatter`].
///
/// [RFC3339]: https://www.ietf.org/rfc/rfc3339.txt
/// [`Display`]: https://doc.rust-lang.org/stable/std/fmt/trait.Display.html
/// [`Formatter`]: struct.Formatter.html
pub struct Timestamp(SystemTime);

/// An [RFC3339] formatted timestamp with nanos.
///
/// [RFC3339]: https://www.ietf.org/rfc/rfc3339.txt
#[derive(Debug)]
pub struct PreciseTimestamp(SystemTime);

impl fmt::Debug for Timestamp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        /// A `Debug` wrapper for `Timestamp` that uses the `Display` implementation.
        struct TimestampValue<'a>(&'a Timestamp);

        impl<'a> fmt::Debug for TimestampValue<'a> {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                fmt::Display::fmt(&self.0, f)
            }
        }

        f.debug_tuple("Timestamp")
        .field(&TimestampValue(&self))
        .finish()
    }
}

impl fmt::Display for Timestamp {
    fn fmt(&self, f: &mut fmt::Formatter)->fmt::Result {
        format_rfc3339_seconds(self.0).fmt(f)
    }
}

impl fmt::Display for PreciseTimestamp {
    fn fmt(&self, f: &mut fmt::Formatter)->fmt::Result {
        format_rfc3339_nanos(self.0).fmt(f)
    }
}