/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{ Literal};
use super::CodeType;

#[derive(Debug)]
pub struct ErrorCodeType {
    id: String,
}

impl ErrorCodeType {
    pub fn new(id: String) -> Self {
        Self { id }
    }
}

impl CodeType for ErrorCodeType {
    fn type_label(&self) -> String {
        super::PythonCodeOracle.class_name(&self.id)
    }

    fn canonical_name(&self) -> String {
        format!("Type{}", self.id)
    }

    fn literal(&self, _literal: &Literal) -> String {
        unreachable!();
    }
}
