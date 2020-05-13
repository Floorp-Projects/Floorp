#[macro_use]
extern crate cfg_if;

use std::ffi::OsString;
use std::path::PathBuf;
use std::env;

// we don't need to explicitly handle empty strings in the code above,
// because an empty string is not considered to be a absolute path here.
pub fn is_absolute_path(path: OsString) -> Option<PathBuf> {
    let path = PathBuf::from(path);
    if path.is_absolute() {
        Some(path)
    } else {
        None
    }
}


cfg_if! { if #[cfg(all(unix, not(target_os = "redox")))] {

extern crate libc;

use std::ffi::CStr;
use std::mem;
use std::os::unix::ffi::OsStringExt;
use std::ptr;

// https://github.com/rust-lang/rust/blob/master/src/libstd/sys/unix/os.rs#L498
pub fn home_dir() -> Option<PathBuf> {
    return env::var_os("HOME")
        .and_then(|h| if h.is_empty() { None } else { Some(h) })
        .or_else(|| unsafe { fallback() })
        .map(PathBuf::from);

    #[cfg(any(target_os = "android", target_os = "ios", target_os = "emscripten"))]
    unsafe fn fallback() -> Option<OsString> {
        None
    }
    #[cfg(not(any(target_os = "android", target_os = "ios", target_os = "emscripten")))]
    unsafe fn fallback() -> Option<OsString> {
        let amt = match libc::sysconf(libc::_SC_GETPW_R_SIZE_MAX) {
            n if n < 0 => 512 as usize,
            n => n as usize,
        };
        let mut buf = Vec::with_capacity(amt);
        let mut passwd: libc::passwd = mem::zeroed();
        let mut result = ptr::null_mut();
        match libc::getpwuid_r(
            libc::getuid(),
            &mut passwd,
            buf.as_mut_ptr(),
            buf.capacity(),
            &mut result,
        ) {
            0 if !result.is_null() => {
                let ptr = passwd.pw_dir as *const _;
                let bytes = CStr::from_ptr(ptr).to_bytes();
                if bytes.is_empty() {
                    None
                } else {
                    Some(OsStringExt::from_vec(bytes.to_vec()))
                }
            }
            _ => None,
        }
    }
}

}}


cfg_if! { if #[cfg(target_os = "redox")] {

extern crate redox_users;

use self::redox_users::All;
use self::redox_users::AllUsers;
use self::redox_users::Config;

pub fn home_dir() -> Option<PathBuf> {
    let current_uid = redox_users::get_uid().ok()?;
    let users = AllUsers::new(Config::default()).ok()?;
    let user = users.get_by_id(current_uid)?;

    Some(PathBuf::from(user.home.clone()))
}

}}

cfg_if! { if #[cfg(all(unix, not(any(target_os = "macos", target_os = "ios"))))] {

mod xdg_user_dirs;

use std::path::Path;

use std::collections::HashMap;

fn user_dir_file(home_dir: &Path) -> PathBuf {
    env::var_os("XDG_CONFIG_HOME").and_then(is_absolute_path).unwrap_or_else(|| home_dir.join(".config")).join("user-dirs.dirs")
}

// this could be optimized further to not create a map and instead retrieve the requested path only
pub fn user_dir(user_dir_name: &str) -> Option<PathBuf> {
    if let Some(home_dir) = home_dir() {
        xdg_user_dirs::single(&home_dir, &user_dir_file(&home_dir), user_dir_name).remove(user_dir_name)
    } else {
        None
    }
}

pub fn user_dirs(home_dir_path: &Path) -> HashMap<String, PathBuf> {
    xdg_user_dirs::all(home_dir_path, &user_dir_file(home_dir_path))
}
}}


cfg_if! { if #[cfg(target_os = "windows")] {

extern crate winapi;
use self::winapi::shared::winerror;
use self::winapi::um::combaseapi;
use self::winapi::um::knownfolders;
use self::winapi::um::shlobj;
use self::winapi::um::shtypes;
use self::winapi::um::winbase;
use self::winapi::um::winnt;

pub fn known_folder(folder_id: shtypes::REFKNOWNFOLDERID) -> Option<PathBuf> {
    unsafe {
        let mut path_ptr: winnt::PWSTR = std::ptr::null_mut();
        let result = shlobj::SHGetKnownFolderPath(folder_id, 0, std::ptr::null_mut(), &mut path_ptr);
        if result == winerror::S_OK {
            let len = winbase::lstrlenW(path_ptr) as usize;
            let path = std::slice::from_raw_parts(path_ptr, len);
            let ostr: std::ffi::OsString = std::os::windows::ffi::OsStringExt::from_wide(path);
            combaseapi::CoTaskMemFree(path_ptr as *mut winapi::ctypes::c_void);
            Some(PathBuf::from(ostr))
        } else {
            None
        }
    }
}

pub fn known_folder_profile() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_Profile)
}

pub fn known_folder_roaming_app_data() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_RoamingAppData)
}

pub fn known_folder_local_app_data() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_LocalAppData)
}

pub fn known_folder_music() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_Music)
}

pub fn known_folder_desktop() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_Desktop)
}

pub fn known_folder_documents() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_Documents)
}

pub fn known_folder_downloads() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_Downloads)
}

pub fn known_folder_pictures() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_Pictures)
}

pub fn known_folder_public() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_Public)
}
pub fn known_folder_templates() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_Templates)
}
pub fn known_folder_videos() -> Option<PathBuf> {
    known_folder(&knownfolders::FOLDERID_Videos)
}

}}
