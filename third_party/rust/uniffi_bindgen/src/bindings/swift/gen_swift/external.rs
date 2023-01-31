/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeOracle, CodeType};

pub struct ExternalCodeType {
    name: String,
}

impl ExternalCodeType {
    pub fn new(name: String) -> Self {
        ExternalCodeType { name }
    }
}

impl CodeType for ExternalCodeType {
    fn type_label(&self, _oracle: &dyn CodeOracle) -> String {
        self.name.clone()
    }

    fn canonical_name(&self, _oracle: &dyn CodeOracle) -> String {
        format!("Type{}", self.name)
    }

    // lower and lift need to call public function which were generated for
    // the original types.
    fn lower(&self, oracle: &dyn CodeOracle) -> String {
        format!("{}_lower", self.ffi_converter_name(oracle))
    }

    fn lift(&self, oracle: &dyn CodeOracle) -> String {
        format!("{}_lift", self.ffi_converter_name(oracle))
    }
}
