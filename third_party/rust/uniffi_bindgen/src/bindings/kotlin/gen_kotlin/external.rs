/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::CodeType;
use crate::ComponentInterface;

#[derive(Debug)]
pub struct ExternalCodeType {
    name: String,
}

impl ExternalCodeType {
    pub fn new(name: String) -> Self {
        Self { name }
    }
}

impl CodeType for ExternalCodeType {
    fn type_label(&self, ci: &ComponentInterface) -> String {
        super::KotlinCodeOracle.class_name(ci, &self.name)
    }

    fn canonical_name(&self) -> String {
        format!("Type{}", self.name)
    }
}
