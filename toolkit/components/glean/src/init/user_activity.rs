// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::ffi::CStr;
use std::os::raw::c_char;
use std::sync::{
    atomic::{AtomicBool, Ordering},
    RwLock,
};
use std::time::{Duration, Instant};

use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use xpcom::{
    interfaces::{nsIObserverService, nsISupports},
    RefPtr,
};

// Partially cargo-culted from UploadPrefObserver.
#[xpcom(implement(nsIObserver), atomic)]
pub(crate) struct UserActivityObserver {
    last_edge: RwLock<Instant>,
    was_active: AtomicBool,
}

/// Listens to Firefox Desktop's `user-interaction-(in)active` topics,
/// debouncing them before calling into the Glean SDK Client Activity API.
/// See
/// [the docs](https://firefox-source-docs.mozilla.org/toolkit/components/glean/builtin_pings.html)
/// for more info.
#[allow(non_snake_case)]
impl UserActivityObserver {
    pub(crate) fn begin_observing() -> Result<(), nsresult> {
        // First and foremost, even if we can't get the ObserverService,
        // init always means client activity.
        glean::handle_client_active();

        // SAFETY: Everything here is self-contained.
        //
        // * We allocate the activity observer, created by the xpcom macro
        // * We create cstr from a static string.
        // * We control all input to `AddObserver`
        unsafe {
            let activity_obs = Self::allocate(InitUserActivityObserver {
                last_edge: RwLock::new(Instant::now()),
                was_active: AtomicBool::new(false),
            });
            let obs_service: RefPtr<nsIObserverService> =
                xpcom::components::Observer::service().map_err(|_| NS_ERROR_FAILURE)?;
            let rv = obs_service.AddObserver(
                activity_obs.coerce(),
                cstr!("user-interaction-active").as_ptr(),
                false,
            );
            if !rv.succeeded() {
                return Err(rv);
            }
            let rv = obs_service.AddObserver(
                activity_obs.coerce(),
                cstr!("user-interaction-inactive").as_ptr(),
                false,
            );
            if !rv.succeeded() {
                return Err(rv);
            }
        }
        Ok(())
    }

    unsafe fn Observe(
        &self,
        _subject: *const nsISupports,
        topic: *const c_char,
        _data: *const u16,
    ) -> nserror::nsresult {
        match CStr::from_ptr(topic).to_str() {
            Ok("user-interaction-active") => self.handle_active(),
            Ok("user-interaction-inactive") => self.handle_inactive(),
            _ => NS_OK,
        }
    }

    fn handle_active(&self) -> nserror::nsresult {
        let was_active = self.was_active.swap(true, Ordering::SeqCst);
        if !was_active {
            let inactivity = self
                .last_edge
                .read()
                .expect("Edge lock poisoned.")
                .elapsed();
            // We only care after a certain period of inactivity (default 20min).
            let limit = static_prefs::pref!("telemetry.fog.test.inactivity_limit");
            if inactivity >= Duration::from_secs(limit.into()) {
                log::info!(
                    "User triggers core activity after {}s!",
                    inactivity.as_secs()
                );
                glean::handle_client_active();
            }
            let mut edge = self.last_edge.write().expect("Edge lock poisoned.");
            *edge = Instant::now();
        }
        NS_OK
    }

    fn handle_inactive(&self) -> nserror::nsresult {
        let was_active = self.was_active.swap(false, Ordering::SeqCst);
        // This is actually always so. Inactivity is only notified once.
        if was_active {
            let activity = self
                .last_edge
                .read()
                .expect("Edge lock poisoned.")
                .elapsed();
            // We only care after a certain period of activity (default 2min).
            let limit = static_prefs::pref!("telemetry.fog.test.activity_limit");
            if activity >= Duration::from_secs(limit.into()) {
                log::info!(
                    "User triggers core inactivity after {}s!",
                    activity.as_secs()
                );
                glean::handle_client_inactive();
            }
            let mut edge = self.last_edge.write().expect("Edge lock poisoned.");
            *edge = Instant::now();
        }
        NS_OK
    }
}
