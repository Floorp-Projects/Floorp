/// Official capitalized logging (and logging filtering) level names
///
/// In order of `as_usize()`.
pub static LOG_LEVEL_NAMES: [&'static str; 7] = ["OFF", "CRITICAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"];

/// Official capitalized logging (and logging filtering) short level names
///
/// In order of `as_usize()`.
pub static LOG_LEVEL_SHORT_NAMES: [&'static str; 7] = ["OFF", "CRIT", "ERRO", "WARN", "INFO", "DEBG", "TRCE"];


/// Logging level associated with a logging `Record`
#[repr(usize)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub enum Level {
    /// Critical
    Critical,
    /// Error
    Error,
    /// Warning
    Warning,
    /// Info
    Info,
    /// Debug
    Debug,
    /// Trace
    Trace
}

/// Logging filtering level
#[repr(usize)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub enum FilterLevel {
    /// Log nothing
    Off,
    /// Log critical level only
    Critical,
    /// Log only error level and above
    Error,
    /// Log only warning level and above
    Warning,
    /// Log only info level and above
    Info,
    /// Log only debug level and above
    Debug,
    /// Log everything
    Trace,
}

impl Level {
    /// Convert to `str` from `LOG_LEVEL_SHORT_NAMES`
    pub fn as_short_str(&self) -> &'static str {
        LOG_LEVEL_SHORT_NAMES[self.as_usize()]
    }

    /// Convert to `str` from `LOG_LEVEL_NAMES`
    pub fn as_str(&self) -> &'static str {
        LOG_LEVEL_NAMES[self.as_usize()]
    }

    /// Cast `Level` to ordering integer 
    ///
    /// `Critical` is the smallest and `Trace` the biggest value
    #[inline]
    pub fn as_usize(&self) -> usize {
        match *self {
            Level::Critical => 1,
            Level::Error => 2,
            Level::Warning => 3,
            Level::Info => 4,
            Level::Debug => 5,
            Level::Trace => 6,
        }
    }

    /// Get a `Level` from an `usize`
    ///
    /// This complements `as_usize`
    #[inline]
    pub fn from_usize(u: usize) -> Option<Level> {
        match u {
            1 => Some(Level::Critical),
            2 => Some(Level::Error),
            3 => Some(Level::Warning),
            4 => Some(Level::Info),
            5 => Some(Level::Debug),
            6 => Some(Level::Trace),
            _ => None
        }
    }
}

impl FilterLevel {
    /// Convert to `usize` value
    ///
    /// `Off` is 0, and `Trace` 6
    #[inline]
    pub fn as_usize(&self) -> usize {
        match *self {
            FilterLevel::Off => 0,
            FilterLevel::Critical => 1,
            FilterLevel::Error => 2,
            FilterLevel::Warning => 3,
            FilterLevel::Info => 4,
            FilterLevel::Debug => 5,
            FilterLevel::Trace => 6,
        }
    }

    /// Get a `FilterLevel` from an `usize`
    ///
    /// This complements `as_usize`
    #[inline]
    pub fn from_usize(u: usize) -> Option<FilterLevel> {
        match u {
            0 => Some(FilterLevel::Off),
            1 => Some(FilterLevel::Critical),
            2 => Some(FilterLevel::Error),
            3 => Some(FilterLevel::Warning),
            4 => Some(FilterLevel::Info),
            5 => Some(FilterLevel::Debug),
            6 => Some(FilterLevel::Trace),
            _ => None
        }
    }

    /// Maximum logging level (log everything)
    #[inline]
    pub fn max() -> Self {
        FilterLevel::Trace
    }


    /// Minimum logging level (log nothing)
    #[inline]
    pub fn min() -> Self {
        FilterLevel::Off
    }
}

static ASCII_LOWERCASE_MAP: [u8; 256] = [
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    b' ', b'!', b'"', b'#', b'$', b'%', b'&', b'\'',
    b'(', b')', b'*', b'+', b',', b'-', b'.', b'/',
    b'0', b'1', b'2', b'3', b'4', b'5', b'6', b'7',
    b'8', b'9', b':', b';', b'<', b'=', b'>', b'?',
    b'@',

    b'a', b'b', b'c', b'd', b'e', b'f', b'g',
    b'h', b'i', b'j', b'k', b'l', b'm', b'n', b'o',
    b'p', b'q', b'r', b's', b't', b'u', b'v', b'w',
    b'x', b'y', b'z',

    b'[', b'\\', b']', b'^', b'_',
    b'`', b'a', b'b', b'c', b'd', b'e', b'f', b'g',
    b'h', b'i', b'j', b'k', b'l', b'm', b'n', b'o',
    b'p', b'q', b'r', b's', b't', b'u', b'v', b'w',
    b'x', b'y', b'z', b'{', b'|', b'}', b'~', 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
    ];

impl FromStr for FilterLevel {
    type Err = ();
    fn from_str(level: &str) -> core::result::Result<FilterLevel, ()> {
        LOG_LEVEL_NAMES.iter()
            .position(|&name| name.as_bytes().iter().zip(level.as_bytes().iter()).all(|(a, b)| {
                ASCII_LOWERCASE_MAP[*a as usize] == ASCII_LOWERCASE_MAP[*b as usize]
            }))
            .map(|p| FilterLevel::from_usize(p).unwrap()).ok_or(())
    }
}

impl fmt::Display for Level {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.as_short_str())
    }
}

impl Level {
    /// Returns true if `self` is at least `level` logging level
    #[inline]
    pub fn is_at_least(&self, level : Self) -> bool {
        self.as_usize() <= level.as_usize()
    }
}

#[test]
fn level_at_least() {
    assert!(Level::Debug.is_at_least(Level::Debug));
    assert!(Level::Debug.is_at_least(Level::Trace));
    assert!(!Level::Debug.is_at_least(Level::Info));
}

#[test]
fn filterlevel_sanity() {
    assert!(Level::Critical.as_usize() > FilterLevel::Off.as_usize());
    assert!(Level::Critical.as_usize() == FilterLevel::Critical.as_usize());
    assert!(Level::Trace.as_usize() == FilterLevel::Trace.as_usize());
}

#[test]
fn level_from_str() {
    assert_eq!("info".parse::<FilterLevel>().unwrap(), FilterLevel::Info);
}
