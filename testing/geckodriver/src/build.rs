use std::fmt;

use rustc_serialize::json::{Json};

include!(concat!(env!("OUT_DIR"), "/build-info.rs"));

pub struct BuildInfo;

impl BuildInfo {
    pub fn version() -> &'static str {
        crate_version!()
    }

    pub fn hash() -> Option<&'static str> {
        COMMIT_HASH
    }

    pub fn date() -> Option<&'static str> {
        COMMIT_DATE
    }
}

impl fmt::Display for BuildInfo {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", BuildInfo::version())?;
        match (BuildInfo::hash(), BuildInfo::date()) {
            (Some(hash), Some(date)) => write!(f, " ({} {})", hash, date)?,
            (Some(hash), None) => write!(f, " ({})", hash)?,
            _ => {}
        }
        Ok(())
    }
}

//std::convert::From<&str>` is not implemented for `rustc_serialize::json::Json

impl Into<Json> for BuildInfo {
    fn into(self) -> Json {
        Json::String(BuildInfo::version().to_string())
    }
}
