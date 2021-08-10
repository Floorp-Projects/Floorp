// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::ffi::CStr;
use std::os::raw::c_char;

use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::{nsACString, nsCStr};
use xpcom::{
    interfaces::{nsIPrefBranch, nsISupports},
    RefPtr, XpCom,
};

// Partially cargo-culted from https://searchfox.org/mozilla-central/rev/598e50d2c3cd81cd616654f16af811adceb08f9f/security/manager/ssl/cert_storage/src/lib.rs#1192
#[derive(xpcom)]
#[xpimplements(nsIObserver)]
#[refcnt = "atomic"]
pub(crate) struct InitUploadPrefObserver {}

#[allow(non_snake_case)]
impl UploadPrefObserver {
    pub(crate) unsafe fn begin_observing() -> Result<(), nsresult> {
        let pref_obs = Self::allocate(InitUploadPrefObserver {});
        let pref_service = xpcom::services::get_PrefService().ok_or(NS_ERROR_FAILURE)?;
        let pref_branch: RefPtr<nsIPrefBranch> =
            (*pref_service).query_interface().ok_or(NS_ERROR_FAILURE)?;
        let pref_nscstr = &nsCStr::from("datareporting.healthreport.uploadEnabled") as &nsACString;
        (*pref_branch)
            .AddObserverImpl(pref_nscstr, pref_obs.coerce(), false)
            .to_result()?;
        Ok(())
    }

    unsafe fn Observe(
        &self,
        _subject: *const nsISupports,
        topic: *const c_char,
        pref_name: *const i16,
    ) -> nserror::nsresult {
        let topic = CStr::from_ptr(topic).to_str().unwrap();
        // Conversion utf16 to utf8 is messy.
        // We should only ever observe changes to the one pref we want,
        // but just to be on the safe side let's assert.

        // cargo-culted from https://searchfox.org/mozilla-central/rev/598e50d2c3cd81cd616654f16af811adceb08f9f/security/manager/ssl/cert_storage/src/lib.rs#1606-1612
        // (with a little transformation)
        let len = (0..).take_while(|&i| *pref_name.offset(i) != 0).count(); // find NUL.
        let slice = std::slice::from_raw_parts(pref_name as *const u16, len);
        let pref_name = match String::from_utf16(slice) {
            Ok(name) => name,
            Err(_) => return NS_ERROR_FAILURE,
        };
        log::info!("Observed {:?}, {:?}", topic, pref_name);
        debug_assert!(
            topic == "nsPref:changed" && pref_name == "datareporting.healthreport.uploadEnabled"
        );

        let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");
        glean::set_upload_enabled(upload_enabled);
        NS_OK
    }
}
