/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::filters;
use crate::backend::{CodeOracle, CodeType};
use askama::Template;

#[derive(Template)]
#[template(syntax = "py", escape = "none", path = "ExternalTemplate.py")]
pub struct ExternalCodeType {
    name: String,
    crate_name: String,
}

impl ExternalCodeType {
    pub fn new(name: String, crate_name: String) -> Self {
        Self { name, crate_name }
    }
}

impl CodeType for ExternalCodeType {
    fn type_label(&self, _oracle: &dyn CodeOracle) -> String {
        self.name.clone()
    }

    fn canonical_name(&self, _oracle: &dyn CodeOracle) -> String {
        format!("Type{}", self.name)
    }

    fn helper_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        Some(self.render().unwrap())
    }

    fn coerce(&self, _oracle: &dyn CodeOracle, _nm: &str) -> String {
        panic!("should not be necessary to coerce External types");
    }
}
