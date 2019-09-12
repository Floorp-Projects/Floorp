extern crate redox_users;

use std::env;
use std::ffi::OsString;
use std::path::PathBuf;
use self::redox_users::All;
use self::redox_users::AllUsers;
use self::redox_users::Config;

pub fn home_dir() -> Option<PathBuf> {
    let current_uid = redox_users::get_uid().ok()?;
    let users = AllUsers::new(Config::default()).ok()?;
    let user = users.get_by_id(current_uid)?;

    Some(PathBuf::from(user.home.clone()))
}
pub fn cache_dir()      -> Option<PathBuf> { env::var_os("XDG_CACHE_HOME") .and_then(is_absolute_path).or_else(|| home_dir().map(|h| h.join(".cache"))) }
pub fn config_dir()     -> Option<PathBuf> { env::var_os("XDG_CONFIG_HOME").and_then(is_absolute_path).or_else(|| home_dir().map(|h| h.join(".config"))) }
pub fn data_dir()       -> Option<PathBuf> { env::var_os("XDG_DATA_HOME")  .and_then(is_absolute_path).or_else(|| home_dir().map(|h| h.join(".local/share"))) }
pub fn data_local_dir() -> Option<PathBuf> { data_dir().clone() }
pub fn runtime_dir()    -> Option<PathBuf> { env::var_os("XDG_RUNTIME_DIR").and_then(is_absolute_path) }
pub fn executable_dir() -> Option<PathBuf> {
    env::var_os("XDG_BIN_HOME").and_then(is_absolute_path).or_else(|| {
        data_dir().map(|mut e| { e.pop(); e.push("bin"); e })
    })
}
pub fn audio_dir()      -> Option<PathBuf> { home_dir().map(|h| h.join("Music")) }
pub fn desktop_dir()    -> Option<PathBuf> { home_dir().map(|h| h.join("Desktop")) }
pub fn document_dir()   -> Option<PathBuf> { home_dir().map(|h| h.join("Documents")) }
pub fn download_dir()   -> Option<PathBuf> { home_dir().map(|h| h.join("Downloads")) }
pub fn font_dir()       -> Option<PathBuf> { data_dir().map(|d| d.join("fonts")) }
pub fn picture_dir()    -> Option<PathBuf> { home_dir().map(|h| h.join("Pictures")) }
pub fn public_dir()     -> Option<PathBuf> { home_dir().map(|h| h.join("Public")) }
pub fn template_dir()   -> Option<PathBuf> { home_dir().map(|h| h.join("Templates")) }
pub fn video_dir()      -> Option<PathBuf> { home_dir().map(|h| h.join("Videos")) }

// we don't need to explicitly handle empty strings in the code above,
// because an empty string is not considered to be a absolute path here.
fn is_absolute_path(path: OsString) -> Option<PathBuf> {
    let path = PathBuf::from(path);
    if path.is_absolute() {
        Some(path)
    } else {
        None
    }
}
