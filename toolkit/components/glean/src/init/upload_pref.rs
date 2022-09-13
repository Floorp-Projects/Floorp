// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::ffi::CStr;
use std::os::raw::c_char;
use std::sync::atomic::{AtomicBool, Ordering};

use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::{nsACString, nsCStr};
use xpcom::{
    interfaces::{nsIPrefBranch, nsISupports},
    RefPtr,
};

/// Whether the current value of the localhost testing pref is permitting
/// metric recording (even if upload is disabled).
static RECORDING_ENABLED: AtomicBool = AtomicBool::new(false);

// Partially cargo-culted from https://searchfox.org/mozilla-central/rev/598e50d2c3cd81cd616654f16af811adceb08f9f/security/manager/ssl/cert_storage/src/lib.rs#1192
#[xpcom(implement(nsIObserver), atomic)]
pub(crate) struct UploadPrefObserver {}

#[allow(non_snake_case)]
impl UploadPrefObserver {
    pub(crate) fn begin_observing() -> Result<(), nsresult> {
        // Ensure we begin with the correct current value of RECORDING_ENABLED.
        let recording_enabled = static_prefs::pref!("telemetry.fog.test.localhost_port") < 0;
        RECORDING_ENABLED.store(recording_enabled, Ordering::SeqCst);

        // SAFETY: Everything here is self-contained.
        //
        // * We allocate the pref observer, created by the xpcom macro
        // * We query the pref service and bail out if it doesn't exist.
        // * We create a nsCStr from a static string.
        // * We control all input to `AddObserverImpl`
        unsafe {
            let pref_obs = Self::allocate(InitUploadPrefObserver {});
            let pref_branch: RefPtr<nsIPrefBranch> =
                xpcom::components::Preferences::service().map_err(|_| NS_ERROR_FAILURE)?;
            let pref_nscstr =
                &nsCStr::from("datareporting.healthreport.uploadEnabled") as &nsACString;
            (*pref_branch)
                .AddObserverImpl(pref_nscstr, pref_obs.coerce(), false)
                .to_result()?;
            let pref_nscstr = &nsCStr::from("telemetry.fog.test.localhost_port") as &nsACString;
            (*pref_branch)
                .AddObserverImpl(pref_nscstr, pref_obs.coerce(), false)
                .to_result()?;
        }

        Ok(())
    }

    unsafe fn Observe(
        &self,
        _subject: *const nsISupports,
        topic: *const c_char,
        pref_name: *const u16,
    ) -> nserror::nsresult {
        let topic = CStr::from_ptr(topic).to_str().unwrap();
        // Conversion utf16 to utf8 is messy.
        // We should only ever observe changes to one of the two prefs we want,
        // but just to be on the safe side let's assert.

        // cargo-culted from https://searchfox.org/mozilla-central/rev/598e50d2c3cd81cd616654f16af811adceb08f9f/security/manager/ssl/cert_storage/src/lib.rs#1606-1612
        // (with a little transformation)
        let len = (0..).take_while(|&i| *pref_name.offset(i) != 0).count(); // find NUL.
        let slice = std::slice::from_raw_parts(pref_name, len);
        let pref_name = match String::from_utf16(slice) {
            Ok(name) => name,
            Err(_) => return NS_ERROR_FAILURE,
        };
        log::info!("Observed {:?}, {:?}", topic, pref_name);
        debug_assert!(topic == "nsPref:changed");
        debug_assert!(
            pref_name == "datareporting.healthreport.uploadEnabled"
                || pref_name == "telemetry.fog.test.localhost_port"
        );

        let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");
        let recording_enabled = static_prefs::pref!("telemetry.fog.test.localhost_port") < 0;
        log::info!(
            "New upload_enabled {}, recording_enabled {}",
            upload_enabled,
            recording_enabled
        );
        if RECORDING_ENABLED.load(Ordering::SeqCst) && !recording_enabled {
            // Whenever the test pref goes from permitting recording to forbidding it,
            // ensure Glean is told to wipe the stores.
            // This may send a "deletion-request" ping for a client_id that's never sent
            // any other pings.
            glean::set_upload_enabled(false);
        }
        RECORDING_ENABLED.store(recording_enabled, Ordering::SeqCst);
        glean::set_upload_enabled(upload_enabled || recording_enabled);
        NS_OK
    }
}
