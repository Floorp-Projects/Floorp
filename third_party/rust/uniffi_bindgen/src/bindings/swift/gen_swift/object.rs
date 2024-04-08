/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::CodeType;
use crate::interface::ObjectImpl;

#[derive(Debug)]
pub struct ObjectCodeType {
    name: String,
    imp: ObjectImpl,
}

impl ObjectCodeType {
    pub fn new(name: String, imp: ObjectImpl) -> Self {
        Self { name, imp }
    }
}

impl CodeType for ObjectCodeType {
    fn type_label(&self) -> String {
        super::SwiftCodeOracle.class_name(&self.name)
    }

    fn canonical_name(&self) -> String {
        format!("Type{}", self.name)
    }

    fn initialization_fn(&self) -> Option<String> {
        self.imp
            .has_callback_interface()
            .then(|| format!("uniffiCallbackInit{}", self.name))
    }
}
