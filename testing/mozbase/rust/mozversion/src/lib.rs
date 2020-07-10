#![forbid(unsafe_code)]

extern crate ini;
extern crate regex;
extern crate semver;

use crate::platform::ini_path;
use ini::Ini;
use regex::Regex;
use std::default::Default;
use std::error;
use std::fmt::{self, Display, Formatter};
use std::path::Path;
use std::str::FromStr;

/// Details about the version of a Firefox build.
#[derive(Clone, Default)]
pub struct AppVersion {
    /// Unique date-based id for a build
    pub build_id: Option<String>,
    /// Channel name
    pub code_name: Option<String>,
    /// Version number e.g. 55.0a1
    pub version_string: Option<String>,
    /// Url of the respoistory from which the build was made
    pub source_repository: Option<String>,
    /// Commit ID of the build
    pub source_stamp: Option<String>,
}

impl AppVersion {
    pub fn new() -> AppVersion {
        Default::default()
    }

    fn update_from_application_ini(&mut self, ini_file: &Ini) {
        if let Some(section) = ini_file.section(Some("App")) {
            if let Some(build_id) = section.get("BuildID") {
                self.build_id = Some(build_id.clone());
            }
            if let Some(code_name) = section.get("CodeName") {
                self.code_name = Some(code_name.clone());
            }
            if let Some(version) = section.get("Version") {
                self.version_string = Some(version.clone());
            }
            if let Some(source_repository) = section.get("SourceRepository") {
                self.source_repository = Some(source_repository.clone());
            }
            if let Some(source_stamp) = section.get("SourceStamp") {
                self.source_stamp = Some(source_stamp.clone());
            }
        }
    }

    fn update_from_platform_ini(&mut self, ini_file: &Ini) {
        if let Some(section) = ini_file.section(Some("Build")) {
            if let Some(build_id) = section.get("BuildID") {
                self.build_id = Some(build_id.clone());
            }
            if let Some(version) = section.get("Milestone") {
                self.version_string = Some(version.clone());
            }
            if let Some(source_repository) = section.get("SourceRepository") {
                self.source_repository = Some(source_repository.clone());
            }
            if let Some(source_stamp) = section.get("SourceStamp") {
                self.source_stamp = Some(source_stamp.clone());
            }
        }
    }

    pub fn version(&self) -> Option<Version> {
        self.version_string
            .as_ref()
            .and_then(|x| Version::from_str(&*x).ok())
    }
}

#[derive(Default, Clone)]
/// Version number information
pub struct Version {
    /// Major version number (e.g. 55 in 55.0)
    pub major: u64,
    /// Minor version number (e.g. 1 in 55.1)
    pub minor: u64,
    /// Patch version number (e.g. 2 in 55.1.2)
    pub patch: u64,
    /// Prerelase information (e.g. Some(("a", 1)) in 55.0a1)
    pub pre: Option<(String, u64)>,
}

impl Version {
    fn to_semver(&self) -> semver::Version {
        // The way the semver crate handles prereleases isn't what we want here
        // This should be fixed in the long term by implementing our own comparison
        // operators, but for now just act as if prerelease metadata was missing,
        // otherwise it is almost impossible to use this with nightly
        semver::Version {
            major: self.major,
            minor: self.minor,
            patch: self.patch,
            pre: vec![],
            build: vec![],
        }
    }

    pub fn matches(&self, version_req: &str) -> Result<bool, Error> {
        let req = semver::VersionReq::parse(version_req)?;
        Ok(req.matches(&self.to_semver()))
    }
}

impl FromStr for Version {
    type Err = Error;

    fn from_str(version_string: &str) -> Result<Version, Error> {
        let mut version: Version = Default::default();
        let version_re = Regex::new(r"^(?P<major>[[:digit:]]+)\.(?P<minor>[[:digit:]]+)(?:\.(?P<patch>[[:digit:]]+))?(?:(?P<pre0>[a-z]+)(?P<pre1>[[:digit:]]*))?$").unwrap();
        if let Some(captures) = version_re.captures(version_string) {
            match captures
                .name("major")
                .and_then(|x| u64::from_str(x.as_str()).ok())
            {
                Some(x) => version.major = x,
                None => return Err(Error::VersionError("No major version number found".into())),
            }
            match captures
                .name("minor")
                .and_then(|x| u64::from_str(x.as_str()).ok())
            {
                Some(x) => version.minor = x,
                None => return Err(Error::VersionError("No minor version number found".into())),
            }
            if let Some(x) = captures
                .name("patch")
                .and_then(|x| u64::from_str(x.as_str()).ok())
            {
                version.patch = x
            }
            if let Some(pre_0) = captures.name("pre0").map(|x| x.as_str().to_string()) {
                if captures.name("pre1").is_some() {
                    if let Some(pre_1) = captures
                        .name("pre1")
                        .and_then(|x| u64::from_str(x.as_str()).ok())
                    {
                        version.pre = Some((pre_0, pre_1))
                    } else {
                        return Err(Error::VersionError(
                            "Failed to convert prelease number to u64".into(),
                        ));
                    }
                } else {
                    return Err(Error::VersionError(
                        "Failed to convert prelease number to u64".into(),
                    ));
                }
            }
        } else {
            return Err(Error::VersionError(
                "Failed to parse input as version string".into(),
            ));
        }
        Ok(version)
    }
}

impl Display for Version {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        match self.patch {
            0 => write!(f, "{}.{}", self.major, self.minor)?,
            _ => write!(f, "{}.{}.{}", self.major, self.minor, self.patch)?,
        }
        if let Some(ref pre) = self.pre {
            write!(f, "{}{}", pre.0, pre.1)?;
        };
        Ok(())
    }
}

/// Determine the version of Firefox given the path to a binary.
///
/// Given the path to a Firefox binary, read the associated application.ini
/// and platform.ini files to extract information about the version of Firefox
/// at that path.
pub fn firefox_version(binary: &Path) -> Result<AppVersion, Error> {
    let mut version = AppVersion::new();
    let mut updated = false;

    if let Some(dir) = ini_path(binary) {
        let mut application_ini = dir.clone();
        application_ini.push("application.ini");

        if Path::exists(&application_ini) {
            let ini_file = Ini::load_from_file(application_ini).ok();
            if let Some(ini) = ini_file {
                updated = true;
                version.update_from_application_ini(&ini);
            }
        }

        let mut platform_ini = dir;
        platform_ini.push("platform.ini");

        if Path::exists(&platform_ini) {
            let ini_file = Ini::load_from_file(platform_ini).ok();
            if let Some(ini) = ini_file {
                updated = true;
                version.update_from_platform_ini(&ini);
            }
        }

        if !updated {
            return Err(Error::MetadataError(
                "Neither platform.ini nor application.ini found".into(),
            ));
        }
    } else {
        return Err(Error::MetadataError("Invalid binary path".into()));
    }
    Ok(version)
}

#[derive(Debug)]
pub enum Error {
    /// Error parsing a version string
    VersionError(String),
    /// Error reading application metadata
    MetadataError(String),
    /// Error processing a string as a semver comparator
    SemVerError(semver::ReqParseError),
}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Error::VersionError(ref x) => {
                "VersionError: ".fmt(f)?;
                x.fmt(f)
            }
            Error::MetadataError(ref x) => {
                "MetadataError: ".fmt(f)?;
                x.fmt(f)
            }
            Error::SemVerError(ref e) => {
                "SemVerError: ".fmt(f)?;
                e.fmt(f)
            }
        }
    }
}

impl From<semver::ReqParseError> for Error {
    fn from(err: semver::ReqParseError) -> Error {
        Error::SemVerError(err)
    }
}

impl error::Error for Error {
    fn cause(&self) -> Option<&dyn error::Error> {
        match *self {
            Error::SemVerError(ref e) => Some(e),
            Error::VersionError(_) | Error::MetadataError(_) => None,
        }
    }
}

#[cfg(target_os = "macos")]
mod platform {
    use std::path::{Path, PathBuf};

    pub fn ini_path(binary: &Path) -> Option<PathBuf> {
        binary
            .canonicalize()
            .ok()
            .as_ref()
            .and_then(|dir| dir.parent())
            .and_then(|dir| dir.parent())
            .map(|dir| dir.join("Resources"))
    }
}

#[cfg(not(target_os = "macos"))]
mod platform {
    use std::path::{Path, PathBuf};

    pub fn ini_path(binary: &Path) -> Option<PathBuf> {
        binary
            .canonicalize()
            .ok()
            .as_ref()
            .and_then(|dir| dir.parent())
            .map(|dir| dir.to_path_buf())
    }
}

#[cfg(test)]
mod test {
    use super::Version;
    use std::str::FromStr;

    fn parse_version(input: &str) -> String {
        Version::from_str(input).unwrap().to_string()
    }

    fn compare(version: &str, comparison: &str) -> bool {
        let v = Version::from_str(version).unwrap();
        v.matches(comparison).unwrap()
    }

    #[test]
    fn test_parser() {
        assert!(parse_version("50.0a1") == "50.0a1");
        assert!(parse_version("50.0.1a1") == "50.0.1a1");
        assert!(parse_version("50.0.0") == "50.0");
    }

    #[test]
    fn test_matches() {
        assert!(compare("50.0", "=50"));
        assert!(compare("50.1", "=50"));
        assert!(compare("50.1", "=50.1"));
        assert!(compare("50.1.1", "=50.1"));
        assert!(compare("50.0.0", "=50.0.0"));
        assert!(compare("51.0.0", ">50"));
        assert!(compare("49.0", "<50"));
        assert!(compare("50.0", "<50.1"));
        assert!(compare("50.0.0", "<50.0.1"));
        assert!(!compare("50.1.0", ">50"));
        assert!(!compare("50.1.0", "<50"));
        assert!(compare("50.1.0", ">=50,<51"));
        assert!(compare("50.0a1", ">49.0"));
        assert!(compare("50.0a2", "=50"));
        // This is the weird one
        assert!(!compare("50.0a2", ">50.0"));
    }
}
