//! Possible ZIP compression methods.

use std::fmt;

/// Compression methods for the contents of a ZIP file.
#[derive(Copy, Clone, PartialEq, Debug)]
pub enum CompressionMethod
{
    /// The file is stored (no compression)
    Stored,
    /// The file is Deflated
    Deflated,
    /// File is compressed using BZIP2 algorithm
    #[cfg(feature = "bzip2")]
    Bzip2,
    /// Unsupported compression method
    Unsupported(u16),
}

impl CompressionMethod {
    /// Converts an u16 to its corresponding CompressionMethod
    pub fn from_u16(val: u16) -> CompressionMethod {
        match val {
            0 => CompressionMethod::Stored,
            8 => CompressionMethod::Deflated,
            #[cfg(feature = "bzip2")]
            12 => CompressionMethod::Bzip2,
            v => CompressionMethod::Unsupported(v),
        }
    }

    /// Converts a CompressionMethod to a u16
    pub fn to_u16(self) -> u16 {
        match self {
            CompressionMethod::Stored => 0,
            CompressionMethod::Deflated => 8,
            #[cfg(feature = "bzip2")]
            CompressionMethod::Bzip2 => 12,
            CompressionMethod::Unsupported(v) => v,
        }
    }
}

impl fmt::Display for CompressionMethod {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // Just duplicate what the Debug format looks like, i.e, the enum key:
        write!(f, "{:?}", self)
    }
}

#[cfg(test)]
mod test {
    use super::CompressionMethod;

    #[test]
    fn from_eq_to() {
        for v in 0..(::std::u16::MAX as u32 + 1)
        {
            let from = CompressionMethod::from_u16(v as u16);
            let to = from.to_u16() as u32;
            assert_eq!(v, to);
        }
    }

    #[cfg(not(feature = "bzip2"))]
    fn methods() -> Vec<CompressionMethod> {
        vec![CompressionMethod::Stored, CompressionMethod::Deflated]
    }

    #[cfg(feature = "bzip2")]
    fn methods() -> Vec<CompressionMethod> {
        vec![CompressionMethod::Stored, CompressionMethod::Deflated, CompressionMethod::Bzip2]
    }

    #[test]
    fn to_eq_from() {
        fn check_match(method: CompressionMethod) {
            let to = method.to_u16();
            let from = CompressionMethod::from_u16(to);
            let back = from.to_u16();
            assert_eq!(to, back);
        }

        for method in methods() {
            check_match(method);
        }
    }

    #[test]
    fn to_display_fmt() {
        fn check_match(method: CompressionMethod) {
            let debug_str = format!("{:?}", method);
            let display_str = format!("{}", method);
            assert_eq!(debug_str, display_str);
        }

        for method in methods() {
            check_match(method);
        }
    }
}
