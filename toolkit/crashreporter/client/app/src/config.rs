/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Application configuration.

use crate::std::borrow::Cow;
use crate::std::ffi::{OsStr, OsString};
use crate::std::path::{Path, PathBuf};
use crate::{lang, logging::LogTarget, std};
use anyhow::Context;
use once_cell::sync::Lazy;

/// The number of the most recent minidump files to retain when pruning.
const MINIDUMP_PRUNE_SAVE_COUNT: usize = 10;

#[cfg(test)]
pub mod test {
    pub const MINIDUMP_PRUNE_SAVE_COUNT: usize = super::MINIDUMP_PRUNE_SAVE_COUNT;
}

const VENDOR_KEY: &str = "Vendor";
const PRODUCT_KEY: &str = "ProductName";
const DEFAULT_VENDOR: &str = "Mozilla";
const DEFAULT_PRODUCT: &str = "Firefox";

#[derive(Default)]
pub struct Config {
    /// Whether reports should be automatically submitted.
    pub auto_submit: bool,
    /// Whether all threads of the process should be dumped (versus just the crashing thread).
    pub dump_all_threads: bool,
    /// Whether to delete the dump files after submission.
    pub delete_dump: bool,
    /// The data directory.
    pub data_dir: Option<PathBuf>,
    /// The events directory.
    pub events_dir: Option<PathBuf>,
    /// The ping directory.
    pub ping_dir: Option<PathBuf>,
    /// The dump file.
    ///
    /// If missing, an error dialog is displayed.
    pub dump_file: Option<PathBuf>,
    /// The XUL_APP_FILE to define if restarting the application.
    pub app_file: Option<OsString>,
    /// The path to the application to use when restarting the crashed process.
    pub restart_command: Option<OsString>,
    /// The arguments to pass if restarting the application.
    pub restart_args: Vec<OsString>,
    /// The URL to which to send reports.
    pub report_url: Option<OsString>,
    /// The localized strings to use.
    pub strings: Option<lang::LangStrings>,
    /// The log target.
    pub log_target: Option<LogTarget>,
}

pub struct ConfigStringBuilder<'a>(lang::LangStringBuilder<'a>);

impl<'a> ConfigStringBuilder<'a> {
    /// Set an argument for the string.
    pub fn arg<V: Into<Cow<'a, str>>>(self, key: &'a str, value: V) -> Self {
        ConfigStringBuilder(self.0.arg(key, value))
    }

    /// Get the localized string.
    pub fn get(self) -> String {
        self.0
            .get()
            .context("failed to get localized string")
            .unwrap()
    }
}

impl Config {
    /// Return a configuration with no values set, and all bool values false.
    pub fn new() -> Self {
        Self::default()
    }

    /// Load a configuration from the application environment.
    #[cfg_attr(mock, allow(unused))]
    pub fn read_from_environment(&mut self) -> anyhow::Result<()> {
        self.auto_submit = env_bool(ekey!("AUTO_SUBMIT"));
        self.dump_all_threads = env_bool(ekey!("DUMP_ALL_THREADS"));
        self.delete_dump = !env_bool(ekey!("NO_DELETE_DUMP"));
        self.data_dir = env_path(ekey!("DATA_DIRECTORY"));
        self.events_dir = env_path(ekey!("EVENTS_DIRECTORY"));
        self.ping_dir = env_path(ekey!("PING_DIRECTORY"));
        self.app_file = std::env::var_os(ekey!("RESTART_XUL_APP_FILE"));

        // Only support `MOZ_APP_LAUNCHER` on linux and macos.
        if cfg!(not(target_os = "windows")) {
            self.restart_command = std::env::var_os("MOZ_APP_LAUNCHER");
        }

        if self.restart_command.is_none() {
            self.restart_command = Some(
                self.sibling_program_path(mozbuild::config::MOZ_APP_NAME)
                    .into(),
            )
        }

        // We no longer use don't use `MOZ_CRASHREPORTER_RESTART_ARG_0`, see bug 1872920.
        self.restart_args = (1..)
            .into_iter()
            .map_while(|arg_num| std::env::var_os(format!("{}_{}", ekey!("RESTART_ARG"), arg_num)))
            // Sometimes these are empty, in which case they should be ignored.
            .filter(|s| !s.is_empty())
            .collect();

        self.report_url = std::env::var_os(ekey!("URL"));

        let mut args = std::env::args_os()
            // skip program name
            .skip(1);
        self.dump_file = args.next().map(|p| p.into());
        while let Some(arg) = args.next() {
            log::warn!("ignoring extraneous argument: {}", arg.to_string_lossy());
        }

        self.strings = Some(lang::load().context("failed to load localized strings")?);

        Ok(())
    }

    /// Get the localized string for the given index.
    pub fn string(&self, index: &str) -> String {
        self.build_string(index).get()
    }

    /// Build the localized string for the given index.
    pub fn build_string<'a>(&'a self, index: &'a str) -> ConfigStringBuilder<'a> {
        ConfigStringBuilder(
            self.strings
                .as_ref()
                .expect("strings not set")
                .builder(index),
        )
    }

    /// Whether the configured language has right-to-left text flow.
    pub fn is_rtl(&self) -> bool {
        self.strings
            .as_ref()
            .map(|s| s.is_rtl())
            .unwrap_or_default()
    }

    /// Load the extra file, updating configuration.
    pub fn load_extra_file(&mut self) -> anyhow::Result<serde_json::Value> {
        let extra_file = self.extra_file().unwrap();

        // Load the extra file (which minidump-analyzer just updated).
        let extra: serde_json::Value =
            serde_json::from_reader(std::fs::File::open(&extra_file).with_context(|| {
                self.build_string("crashreporter-error-opening-file")
                    .arg("path", extra_file.display().to_string())
                    .get()
            })?)
            .with_context(|| {
                self.build_string("crashreporter-error-loading-file")
                    .arg("path", extra_file.display().to_string())
                    .get()
            })?;

        // Set report url if not already set.
        if self.report_url.is_none() {
            if let Some(url) = extra["ServerURL"].as_str() {
                self.report_url = Some(url.into());
            }
        }

        // Set the data dir if not already set.
        if self.data_dir.is_none() {
            let vendor = extra[VENDOR_KEY].as_str().unwrap_or(DEFAULT_VENDOR);
            let product = extra[PRODUCT_KEY].as_str().unwrap_or(DEFAULT_PRODUCT);
            self.data_dir = Some(self.get_data_dir(vendor, product)?);
        }

        // Clear the restart command if WER handled the crash. This prevents restarting the
        // program. See bug 1872920.
        if extra.get("WindowsErrorReporting").is_some() {
            self.restart_command = None;
        }

        Ok(extra)
    }

    /// Get the path to the extra file.
    ///
    /// Returns None if no dump_file is set.
    pub fn extra_file(&self) -> Option<PathBuf> {
        self.dump_file.clone().map(extra_file_for_dump_file)
    }

    /// Get the path to the memory file.
    ///
    /// Returns None if no dump_file is set or if the memory file does not exist.
    pub fn memory_file(&self) -> Option<PathBuf> {
        self.dump_file.clone().and_then(|p| {
            let p = memory_file_for_dump_file(p);
            p.exists().then_some(p)
        })
    }

    /// The path to the data directory.
    ///
    /// Panics if no data directory is set.
    pub fn data_dir(&self) -> &Path {
        self.data_dir.as_deref().unwrap()
    }

    /// The path to the dump file.
    ///
    /// Panics if no dump file is set.
    pub fn dump_file(&self) -> &Path {
        self.dump_file.as_deref().unwrap()
    }

    /// The id of the local dump file (the base filename without extension).
    ///
    /// Panics if no dump file is set.
    pub fn local_dump_id(&self) -> Cow<str> {
        self.dump_file().file_stem().unwrap().to_string_lossy()
    }

    /// Move crash data to the pending folder.
    pub fn move_crash_data_to_pending(&mut self) -> anyhow::Result<()> {
        let pending_crashes_dir = self.data_dir().join("pending");
        std::fs::create_dir_all(&pending_crashes_dir).with_context(|| {
            self.build_string("crashreporter-error-creating-dir")
                .arg("path", pending_crashes_dir.display().to_string())
                .get()
        })?;

        let move_file = |from: &Path| -> anyhow::Result<PathBuf> {
            let to = pending_crashes_dir.join(from.file_name().unwrap());
            // Try to rename, but copy and remove if it fails. `rename` won't work across
            // mount points. (bug 506009)
            if let Err(e) = std::fs::rename(from, &to) {
                log::warn!("failed to move {} to {}: {e}", from.display(), to.display());
                log::info!("trying to copy and remove instead");

                std::fs::copy(from, &to).with_context(|| {
                    self.build_string("crashreporter-error-moving-path")
                        .arg("from", from.display().to_string())
                        .arg("to", to.display().to_string())
                        .get()
                })?;
                if let Err(e) = std::fs::remove_file(from) {
                    log::warn!("failed to remove {}: {e}", from.display());
                }
            }
            Ok(to)
        };

        let new_dump_file = move_file(self.dump_file())?;
        move_file(self.extra_file().unwrap().as_ref())?;
        // Failing to move the memory file is recoverable.
        if let Some(memory_file) = self.memory_file() {
            if let Err(e) = move_file(memory_file.as_ref()) {
                log::warn!("failed to move memory file: {e}");
                if let Err(e) = std::fs::remove_file(&memory_file) {
                    log::warn!("failed to remove {}: {e}", memory_file.display());
                }
            }
        }

        self.dump_file = Some(new_dump_file);

        Ok(())
    }

    /// Form the path which signals EOL for a particular version.
    pub fn version_eol_file(&self, version: &str) -> PathBuf {
        self.data_dir().join(format!("EndOfLife{version}"))
    }

    /// Return the path used to store submitted crash ids.
    pub fn submitted_crash_dir(&self) -> PathBuf {
        self.data_dir().join("submitted")
    }

    /// Delete files related to the crash report.
    pub fn delete_files(&self) {
        if !self.delete_dump {
            return;
        }

        for file in [&self.dump_file, &self.extra_file(), &self.memory_file()]
            .into_iter()
            .flatten()
        {
            if let Err(e) = std::fs::remove_file(file) {
                log::warn!("failed to remove {}: {e}", file.display());
            }
        }
    }

    /// Prune old minidump files adjacent to the dump file.
    pub fn prune_files(&self) -> anyhow::Result<()> {
        log::info!("pruning minidump files to the {MINIDUMP_PRUNE_SAVE_COUNT} most recent");
        let Some(file) = &self.dump_file else {
            anyhow::bail!("no dump file")
        };
        let Some(dir) = file.parent() else {
            anyhow::bail!("no parent directory for dump file")
        };
        log::debug!("pruning {} directory", dir.display());
        let read_dir = dir.read_dir().with_context(|| {
            format!(
                "failed to read dump file parent directory {}",
                dir.display()
            )
        })?;

        let mut minidump_files = Vec::new();
        for entry in read_dir {
            match entry {
                Err(e) => log::error!(
                    "error while iterating over {} directory entry: {e}",
                    dir.display()
                ),
                Ok(e) if e.path().extension() == Some("dmp".as_ref()) => {
                    // Return if the metadata can't be read, since not being able to get metadata
                    // for any file could make the selection of minidumps to delete incorrect.
                    let meta = e.metadata().with_context(|| {
                        format!("failed to read metadata for {}", e.path().display())
                    })?;
                    if meta.is_file() {
                        let modified_time =
                            meta.modified().expect(
                                "file modification time should be available on all crashreporter platforms",
                            );
                        minidump_files.push((modified_time, e.path()));
                    }
                }
                _ => (),
            }
        }

        // Sort by modification time first, then path (just to have a defined behavior in the case
        // of identical times). The reverse leaves the files in order from newest to oldest.
        minidump_files.sort_unstable_by(|a, b| a.cmp(b).reverse());

        // Delete files, skipping the most recent MINIDUMP_PRUNE_SAVE_COUNT.
        for dump_file in minidump_files
            .into_iter()
            .skip(MINIDUMP_PRUNE_SAVE_COUNT)
            .map(|v| v.1)
        {
            log::debug!("pruning {} and related files", dump_file.display());
            if let Err(e) = std::fs::remove_file(&dump_file) {
                log::warn!("failed to delete {}: {e}", dump_file.display());
            }

            // Ignore errors for the extra file and the memory file: they may not exist.
            let _ = std::fs::remove_file(extra_file_for_dump_file(dump_file.clone()));
            let _ = std::fs::remove_file(memory_file_for_dump_file(dump_file));
        }
        Ok(())
    }

    /// Get the path of a program that is a sibling of the crashreporter.
    ///
    /// On MacOS, this assumes that the crashreporter is its own application bundle within the main
    /// program bundle. On other platforms this assumes siblings reside in the same directory as
    /// the crashreporter.
    ///
    /// The returned path isn't guaranteed to exist.
    // This method could be standalone rather than living in `Config`; it's here because it makes
    // sense that if it were to rely on anything, it would be the `Config` (and that may change in
    // the future).
    pub fn sibling_program_path<N: AsRef<OsStr>>(&self, program: N) -> PathBuf {
        let self_path = self_path();
        let exe_extension = self_path.extension().unwrap_or_default();
        if !exe_extension.is_empty() {
            let mut p = program.as_ref().to_os_string();
            p.push(".");
            p.push(exe_extension);
            sibling_path(p)
        } else {
            sibling_path(program)
        }
    }

    cfg_if::cfg_if! {
        if #[cfg(mock)] {
            fn get_data_dir(&self, vendor: &str, product: &str) -> anyhow::Result<PathBuf> {
                let mut path = PathBuf::from("data_dir");
                path.push(vendor);
                path.push(product);
                path.push("Crash Reports");
                Ok(path)
            }
        } else if #[cfg(target_os = "linux")] {
            fn get_data_dir(&self, vendor: &str, product: &str) -> anyhow::Result<PathBuf> {
                // home_dir is deprecated due to incorrect behavior on windows, but we only use it on linux
                #[allow(deprecated)]
                let mut data_path =
                    std::env::home_dir().with_context(|| self.string("crashreporter-error-no-home-dir"))?;
                data_path.push(format!(".{}", vendor.to_lowercase()));
                data_path.push(product.to_lowercase());
                data_path.push("Crash Reports");
                Ok(data_path)
            }
        } else if #[cfg(target_os = "macos")] {
            fn get_data_dir(&self, _vendor: &str, product: &str) -> anyhow::Result<PathBuf> {
                use objc::{
                    rc::autoreleasepool,
                    runtime::{Object, BOOL, YES},
                    *,
                };
                #[link(name = "Foundation", kind = "framework")]
                extern "system" {
                    fn NSSearchPathForDirectoriesInDomains(
                        directory: usize,
                        domain_mask: usize,
                        expand_tilde: BOOL,
                    ) -> *mut Object /* NSArray<NSString*>* */;
                }
                #[allow(non_upper_case_globals)]
                const NSApplicationSupportDirectory: usize = 14;
                #[allow(non_upper_case_globals)]
                const NSUserDomainMask: usize = 1;

                let mut data_path = autoreleasepool(|| {
                    let paths /* NSArray<NSString*>* */ = unsafe {
                                NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES)
                            };
                    if paths.is_null() {
                        anyhow::bail!("NSSearchPathForDirectoriesInDomains returned nil");
                    }
                    let path: *mut Object /* NSString* */ = unsafe { msg_send![paths, firstObject] };
                    if path.is_null() {
                        anyhow::bail!("NSSearchPathForDirectoriesInDomains returned no paths");
                    }

                    let str_pointer: *const i8 = unsafe { msg_send![path, UTF8String] };
                    // # Safety
                    // The pointer is a readable C string with a null terminator.
                    let Ok(s) = unsafe { std::ffi::CStr::from_ptr(str_pointer) }.to_str() else {
                        anyhow::bail!("NSString wasn't valid UTF8");
                    };
                    Ok(PathBuf::from(s))
                })?;
                data_path.push(product);
                std::fs::create_dir_all(&data_path).with_context(|| {
                    self.build_string("crashreporter-error-creating-dir")
                        .arg("path", data_path.display().to_string())
                        .get()
                })?;
                data_path.push("Crash Reports");
                Ok(data_path)
            }
        } else if #[cfg(target_os = "windows")] {
            fn get_data_dir(&self, vendor: &str, product: &str) -> anyhow::Result<PathBuf> {
                use crate::std::os::windows::ffi::OsStringExt;
                use windows_sys::{
                    core::PWSTR,
                    Win32::{
                        Globalization::lstrlenW,
                        System::Com::CoTaskMemFree,
                        UI::Shell::{FOLDERID_RoamingAppData, SHGetKnownFolderPath},
                    },
                };

                let mut path: PWSTR = std::ptr::null_mut();
                let result = unsafe { SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, 0, &mut path) };
                if result != 0 {
                    unsafe { CoTaskMemFree(path as _) };
                    anyhow::bail!("failed to get known path for roaming appdata");
                }

                let length = unsafe { lstrlenW(path) };
                let slice = unsafe { std::slice::from_raw_parts(path, length as usize) };
                let osstr = OsString::from_wide(slice);
                unsafe { CoTaskMemFree(path as _) };
                let mut path = PathBuf::from(osstr);
                path.push(vendor);
                path.push(product);
                path.push("Crash Reports");
                Ok(path)
            }
        }
    }
}

/// Get the path of a file that is a sibling of the crashreporter.
///
/// On MacOS, this assumes that the crashreporter is its own application bundle within the main
/// program bundle. On other platforms this assumes siblings reside in the same directory as
/// the crashreporter.
///
/// The returned path isn't guaranteed to exist.
pub fn sibling_path<N: AsRef<OsStr>>(file: N) -> PathBuf {
    // Expect shouldn't panic as we don't invoke the program without a parent directory.
    let dir_path = self_path().parent().expect("program invoked based on PATH");

    let mut path = dir_path.join(file.as_ref());

    if !path.exists() && cfg!(all(not(mock), target_os = "macos")) {
        // On macOS the crash reporter client is shipped as an application bundle contained
        // within Firefox's main application bundle. So when it's invoked its current working
        // directory looks like:
        // Firefox.app/Contents/MacOS/crashreporter.app/Contents/MacOS/
        // The other applications we ship with Firefox are stored in the main bundle
        // (Firefox.app/Contents/MacOS/) so we we need to go back three directories
        // to reach them.

        // 3rd ancestor (the 0th element of ancestors has no paths removed) to remove
        // `crashreporter.app/Contents/MacOS`.
        if let Some(ancestor) = dir_path.ancestors().nth(3) {
            path = ancestor.join(file.as_ref());
        }
    }

    path
}

fn self_path() -> &'static Path {
    static PATH: Lazy<PathBuf> = Lazy::new(|| {
        // Expect shouldn't ever panic here because we need more than one argument to run
        // the program in the first place (we've already previously iterated args).
        //
        // We use argv[0] rather than `std::env::current_exe` because `current_exe` doesn't define
        // how symlinks are treated, and we want to support running directly from the local build
        // directory (which uses symlinks on linux and macos).
        PathBuf::from(std::env::args_os().next().expect("failed to get argv[0]"))
    });
    &*PATH
}

fn env_bool<K: AsRef<OsStr>>(name: K) -> bool {
    std::env::var(name).map(|s| !s.is_empty()).unwrap_or(false)
}

fn env_path<K: AsRef<OsStr>>(name: K) -> Option<PathBuf> {
    std::env::var_os(name).map(PathBuf::from)
}

fn extra_file_for_dump_file(mut dump_file: PathBuf) -> PathBuf {
    dump_file.set_extension("extra");
    dump_file
}

fn memory_file_for_dump_file(mut dump_file: PathBuf) -> PathBuf {
    dump_file.set_extension("memory.json.gz");
    dump_file
}
