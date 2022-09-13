/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! This provides a XPCOM service to send app services logs to the desktop

#[macro_use]
extern crate cstr;

#[macro_use]
extern crate xpcom;

use golden_gate::log::LogSink;
use log;
use nserror::{nsresult, NS_OK};
use nsstring::nsAString;
use once_cell::sync::Lazy;
use std::os::raw::c_char;
use std::{
    cmp,
    collections::HashMap,
    sync::{
        atomic::{AtomicBool, Ordering},
        RwLock,
    },
};
use xpcom::{
    interfaces::{mozIAppServicesLogger, mozIServicesLogSink, nsIObserverService, nsISupports},
    RefPtr,
};

/// A flag that's set after we register our observer to clear the map of loggers
/// on shutdown.
static SHUTDOWN_OBSERVED: AtomicBool = AtomicBool::new(false);

#[xpcom(implement(mozIAppServicesLogger), nonatomic)]
pub struct AppServicesLogger {}

pub static LOGGERS_BY_TARGET: Lazy<RwLock<HashMap<String, LogSink>>> = Lazy::new(|| {
    let h: HashMap<String, LogSink> = HashMap::new();
    let m = RwLock::new(h);
    m
});

impl AppServicesLogger {
    xpcom_method!(register => Register(target: *const nsAString, logger: *const mozIServicesLogSink));
    fn register(&self, target: &nsAString, logger: &mozIServicesLogSink) -> Result<(), nsresult> {
        let log_sink_logger = LogSink::with_logger(Some(logger))?;
        let max_level = cmp::max(log::max_level(), log_sink_logger.max_level);

        // Note: This will only work if the max_level is lower than the compile-time
        // max_level_* filter.
        log::set_max_level(max_level);

        ensure_observing_shutdown();

        LOGGERS_BY_TARGET
            .write()
            .unwrap()
            .insert(target.to_string(), log_sink_logger);
        Ok(())
    }

    pub fn is_app_services_logger_registered(target: String) -> bool {
        match LOGGERS_BY_TARGET.read() {
            Ok(loggers_by_target) => loggers_by_target.contains_key(&target),
            Err(_e) => false,
        }
    }
}

// Import the `NS_IsMainThread` symbol from Gecko...
extern "C" {
    fn NS_IsMainThread() -> bool;
}

/// Registers an observer to clear the loggers map on `xpcom-shutdown`. This
/// function must be called from the main thread, because the observer service
//// is main thread-only.
fn ensure_observing_shutdown() {
    assert!(unsafe { NS_IsMainThread() });
    // If we've already registered our observer, bail. Relaxed ordering is safe
    // here and below, because we've asserted we're only called from the main
    // thread, and only check the flag here.
    if SHUTDOWN_OBSERVED.load(Ordering::Relaxed) {
        return;
    }
    if let Ok(service) = xpcom::components::Observer::service::<nsIObserverService>() {
        let observer = ShutdownObserver::allocate(InitShutdownObserver {});
        let rv = unsafe {
            service.AddObserver(observer.coerce(), cstr!("xpcom-shutdown").as_ptr(), false)
        };
        // If we fail to register the observer now, or fail to get the observer
        // service, the flag will remain `false`, and we'll try again on the
        // next call to `ensure_observing_shutdown`.
        SHUTDOWN_OBSERVED.store(rv.succeeded(), Ordering::Relaxed);
    }
}

#[xpcom(implement(nsIObserver), nonatomic)]
struct ShutdownObserver {}

impl ShutdownObserver {
    xpcom_method!(observe => Observe(_subject: *const nsISupports, topic: *const c_char, _data: *const u16));
    /// Remove our shutdown observer and clear the map.
    fn observe(
        &self,
        _subject: &nsISupports,
        topic: *const c_char,
        _data: *const u16,
    ) -> Result<(), nsresult> {
        LOGGERS_BY_TARGET.write().unwrap().clear();
        if let Ok(service) = xpcom::components::Observer::service::<nsIObserverService>() {
            // Ignore errors, since we're already shutting down.
            let _ = unsafe { service.RemoveObserver(self.coerce(), topic) };
        }
        Ok(())
    }
}

/// The constructor for an `AppServicesLogger` service. This uses C linkage so that it
/// can be called from C++. See `AppServicesLoggerComponents.h` for the C++
/// constructor that's passed to the component manager.
///
/// # Safety
///
/// This function is unsafe because it dereferences `result`.
#[no_mangle]
pub unsafe extern "C" fn NS_NewAppServicesLogger(
    result: *mut *const mozIAppServicesLogger,
) -> nsresult {
    let logger = AppServicesLogger::allocate(InitAppServicesLogger {});
    RefPtr::new(logger.coerce::<mozIAppServicesLogger>()).forget(&mut *result);
    NS_OK
}
