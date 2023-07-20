/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::CodeType;

#[derive(Debug)]
pub struct ForeignExecutorCodeType;

impl CodeType for ForeignExecutorCodeType {
    fn type_label(&self) -> String {
        // On Swift, we define a struct to represent a ForeignExecutor
        "UniFfiForeignExecutor".into()
    }

    fn canonical_name(&self) -> String {
        "ForeignExecutor".into()
    }

    fn initialization_fn(&self) -> Option<String> {
        Some("uniffiInitForeignExecutor".into())
    }
}
