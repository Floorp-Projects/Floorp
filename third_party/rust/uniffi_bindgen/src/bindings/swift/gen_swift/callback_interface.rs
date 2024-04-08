/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::CodeType;

#[derive(Debug)]
pub struct CallbackInterfaceCodeType {
    name: String,
}

impl CallbackInterfaceCodeType {
    pub fn new(name: String) -> Self {
        Self { name }
    }
}

impl CodeType for CallbackInterfaceCodeType {
    fn type_label(&self) -> String {
        super::SwiftCodeOracle.class_name(&self.name)
    }

    fn canonical_name(&self) -> String {
        format!("CallbackInterface{}", self.type_label())
    }

    fn initialization_fn(&self) -> Option<String> {
        Some(format!("uniffiCallbackInit{}", self.name))
    }
}
