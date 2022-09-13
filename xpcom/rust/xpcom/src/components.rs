/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module contains convenient accessors for static XPCOM components.
//!
//! The contents of this file are generated from
//! `xpcom/components/gen_static_components.py`.

extern "C" {
    fn Gecko_GetServiceByModuleID(
        id: ModuleID,
        iid: &crate::nsIID,
        result: *mut *mut libc::c_void,
    ) -> nserror::nsresult;
    fn Gecko_CreateInstanceByModuleID(
        id: ModuleID,
        iid: &crate::nsIID,
        result: *mut *mut libc::c_void,
    ) -> nserror::nsresult;
}

include!(mozbuild::objdir_path!("xpcom/components/components.rs"));
