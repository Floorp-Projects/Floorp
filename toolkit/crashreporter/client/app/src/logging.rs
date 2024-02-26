/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Application logging facilities.

use crate::std::{
    self,
    path::Path,
    sync::{Arc, Mutex},
};

/// Initialize logging and return a log target which can be used to change the destination of log
/// statements.
pub fn init() -> LogTarget {
    let log_target_inner = LogTargetInner::default();

    env_logger::builder()
        .parse_env(
            env_logger::Env::new()
                .filter("MOZ_CRASHEREPORTER")
                .write_style("MOZ_CRASHREPORTER_STYLE"),
        )
        .target(env_logger::fmt::Target::Pipe(Box::new(
            log_target_inner.clone(),
        )))
        .init();

    LogTarget {
        inner: log_target_inner,
    }
}

/// Controls the target of logging.
#[derive(Clone)]
pub struct LogTarget {
    inner: LogTargetInner,
}

impl LogTarget {
    /// Set the file to which log statements will be written.
    pub fn set_file(&self, path: &Path) {
        match std::fs::File::create(path) {
            Ok(file) => {
                if let Ok(mut guard) = self.inner.target.lock() {
                    *guard = Box::new(file);
                }
            }
            Err(e) => log::error!("failed to retarget log to {}: {e}", path.display()),
        }
    }
}

/// A private inner class implements Write, allows creation, etc. Externally the `LogTarget` only
/// supports changing the target and nothing else.
#[derive(Clone)]
struct LogTargetInner {
    target: Arc<Mutex<Box<dyn std::io::Write + Send + 'static>>>,
}

impl Default for LogTargetInner {
    fn default() -> Self {
        LogTargetInner {
            target: Arc::new(Mutex::new(Box::new(std::io::stderr()))),
        }
    }
}

impl std::io::Write for LogTargetInner {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        let Ok(mut guard) = self.target.lock() else {
            // Pretend we wrote successfully.
            return Ok(buf.len());
        };
        guard.write(buf)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        let Ok(mut guard) = self.target.lock() else {
            // Pretend we flushed successfully.
            return Ok(());
        };
        guard.flush()
    }
}
