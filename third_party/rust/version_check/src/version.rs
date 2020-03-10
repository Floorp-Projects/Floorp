use std::fmt;

/// Version number: `major.minor.patch`, ignoring release channel.
#[derive(Debug, PartialEq, Eq, Copy, Clone, PartialOrd, Ord)]
pub struct Version(u64);

impl Version {
    fn to_mmp(&self) -> (u16, u16, u16) {
        let major = self.0 >> 32;
        let minor = (self.0 << 32) >> 48;
        let patch = (self.0 << 48) >> 48;
        (major as u16, minor as u16, patch as u16)
    }

    /// Reads the version of the running compiler. If it cannot be determined
    /// (see the [top-level documentation](crate)), returns `None`.
    ///
    /// # Example
    ///
    /// ```rust
    /// use version_check::Version;
    ///
    /// match Version::read() {
    ///     Some(d) => format!("Version is: {}", d),
    ///     None => format!("Failed to read the version.")
    /// };
    /// ```
    pub fn read() -> Option<Version> {
        ::get_version_and_date()
            .and_then(|(version, _)| version)
            .and_then(|version| Version::parse(&version))
    }


    /// Parse a Rust release version (of the form
    /// `major[.minor[.patch[-channel]]]`), ignoring the release channel, if
    /// any. Returns `None` if `version` is not a valid Rust version string.
    ///
    /// # Example
    ///
    /// ```rust
    /// use version_check::Version;
    ///
    /// let version = Version::parse("1.18.0").unwrap();
    /// assert!(version.exactly("1.18.0"));
    ///
    /// let version = Version::parse("1.20.0-nightly").unwrap();
    /// assert!(version.exactly("1.20.0"));
    /// assert!(version.exactly("1.20.0-beta"));
    ///
    /// let version = Version::parse("1.3").unwrap();
    /// assert!(version.exactly("1.3.0"));
    ///
    /// let version = Version::parse("1").unwrap();
    /// assert!(version.exactly("1.0.0"));
    ///
    /// assert!(Version::parse("one.two.three").is_none());
    /// ```
    pub fn parse(version: &str) -> Option<Version> {
        let mut mmp: Vec<u16> = version.split('-')
            .nth(0)
            .unwrap_or("")
            .split('.')
            .filter_map(|s| s.parse::<u16>().ok())
            .collect();

        if mmp.is_empty() {
            return None
        }

        while mmp.len() < 3 {
            mmp.push(0);
        }

        let (maj, min, patch) = (mmp[0] as u64, mmp[1] as u64, mmp[2] as u64);
        Some(Version((maj << 32) | (min << 16) | patch))
    }

    /// Returns `true` if `self` is greater than or equal to `version`.
    ///
    /// If `version` is greater than `self`, or if `version` is not a valid Rust
    /// version string, returns `false`.
    ///
    /// # Example
    ///
    /// ```rust
    /// use version_check::Version;
    ///
    /// let version = Version::parse("1.35.0").unwrap();
    ///
    /// assert!(version.at_least("1.33.0"));
    /// assert!(version.at_least("1.35.0"));
    /// assert!(version.at_least("1.13.2"));
    ///
    /// assert!(!version.at_least("1.35.1"));
    /// assert!(!version.at_least("1.55.0"));
    /// ```
    pub fn at_least(&self, version: &str) -> bool {
        Version::parse(version)
            .map(|version| self >= &version)
            .unwrap_or(false)
    }

    /// Returns `true` if `self` is less than or equal to `version`.
    ///
    /// If `version` is less than `self`, or if `version` is not a valid Rust
    /// version string, returns `false`.
    ///
    /// # Example
    ///
    /// ```rust
    /// use version_check::Version;
    ///
    /// let version = Version::parse("1.35.0").unwrap();
    ///
    /// assert!(version.at_most("1.35.1"));
    /// assert!(version.at_most("1.55.0"));
    /// assert!(version.at_most("1.35.0"));
    ///
    /// assert!(!version.at_most("1.33.0"));
    /// assert!(!version.at_most("1.13.2"));
    /// ```
    pub fn at_most(&self, version: &str) -> bool {
        Version::parse(version)
            .map(|version| self <= &version)
            .unwrap_or(false)
    }

    /// Returns `true` if `self` is exactly equal to `version`.
    ///
    /// If `version` is not equal to `self`, or if `version` is not a valid Rust
    /// version string, returns `false`.
    ///
    /// # Example
    ///
    /// ```rust
    /// use version_check::Version;
    ///
    /// let version = Version::parse("1.35.0").unwrap();
    ///
    /// assert!(version.exactly("1.35.0"));
    ///
    /// assert!(!version.exactly("1.33.0"));
    /// assert!(!version.exactly("1.35.1"));
    /// assert!(!version.exactly("1.13.2"));
    /// ```
    pub fn exactly(&self, version: &str) -> bool {
        Version::parse(version)
            .map(|version| self == &version)
            .unwrap_or(false)
    }
}

impl fmt::Display for Version {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let (major, minor, patch) = self.to_mmp();
        write!(f, "{}.{}.{}", major, minor, patch)
    }
}

#[cfg(test)]
mod tests {
    use super::Version;

    macro_rules! check_mmp {
        ($s:expr => ($x:expr, $y:expr, $z:expr)) => (
            if let Some(v) = Version::parse($s) {
                if v.to_mmp() != ($x, $y, $z) {
                    panic!("{:?} ({}) didn't parse as {}.{}.{}.", $s, v, $x, $y, $z);
                }
            } else {
                panic!("{:?} didn't parse for mmp testing.", $s);
            }
        )
    }

    #[test]
    fn test_str_to_mmp() {
        check_mmp!("1.18.0" => (1, 18, 0));
        check_mmp!("3.19.0" => (3, 19, 0));
        check_mmp!("1.19.0-nightly" => (1, 19, 0));
        check_mmp!("1.12.2349" => (1, 12, 2349));
        check_mmp!("0.12" => (0, 12, 0));
        check_mmp!("1.12.5" => (1, 12, 5));
        check_mmp!("1.12" => (1, 12, 0));
        check_mmp!("1" => (1, 0, 0));
        check_mmp!("1.4.4-nightly (d84693b93 2017-07-09)" => (1, 4, 4));
        check_mmp!("1.58879.4478-dev" => (1, 58879, 4478));
        check_mmp!("1.58879.4478-dev (d84693b93 2017-07-09)" => (1, 58879, 4478));
    }

    #[test]
    fn test_comparisons() {
        let version = Version::parse("1.18.0").unwrap();
        assert!(version.exactly("1.18.0"));
        assert!(version.at_least("1.12.0"));
        assert!(version.at_least("1.12"));
        assert!(version.at_least("1"));
        assert!(version.at_most("1.18.1"));
        assert!(!version.exactly("1.19.0"));
        assert!(!version.exactly("1.18.1"));

        let version = Version::parse("1.20.0-nightly").unwrap();
        assert!(version.exactly("1.20.0-beta"));
        assert!(version.exactly("1.20.0-nightly"));
        assert!(version.exactly("1.20.0"));
        assert!(!version.exactly("1.19"));

        let version = Version::parse("1.3").unwrap();
        assert!(version.exactly("1.3.0"));
        assert!(version.exactly("1.3.0-stable"));
        assert!(version.exactly("1.3"));
        assert!(!version.exactly("1.5.0-stable"));

        let version = Version::parse("1").unwrap();
        assert!(version.exactly("1.0.0"));
        assert!(version.exactly("1.0"));
        assert!(version.exactly("1"));

        assert!(Version::parse("one.two.three").is_none());
    }

    macro_rules! reflexive_display {
        ($s:expr) => (
            assert_eq!(Version::parse($s).unwrap().to_string(), $s);
        )
    }

    #[test]
    fn display() {
        reflexive_display!("1.0.0");
        reflexive_display!("1.2.3");
        reflexive_display!("1.12.1438");
        reflexive_display!("1.44.0");
        reflexive_display!("2.44.0");
        reflexive_display!("23459.28923.3483");
    }
}
