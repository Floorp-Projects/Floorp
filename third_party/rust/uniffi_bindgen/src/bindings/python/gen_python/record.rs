/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeDeclaration, CodeOracle, CodeType, Literal};
use crate::interface::{ComponentInterface, Record};
use askama::Template;

use super::filters;

pub struct RecordCodeType {
    id: String,
}

impl RecordCodeType {
    pub fn new(id: String) -> Self {
        Self { id }
    }
}

impl CodeType for RecordCodeType {
    fn type_label(&self, oracle: &dyn CodeOracle) -> String {
        oracle.class_name(&self.id)
    }

    fn canonical_name(&self, _oracle: &dyn CodeOracle) -> String {
        format!("Type{}", self.id)
    }

    fn literal(&self, _oracle: &dyn CodeOracle, _literal: &Literal) -> String {
        unreachable!();
    }

    fn helper_code(&self, oracle: &dyn CodeOracle) -> Option<String> {
        Some(format!(
            "# Helper code for {} record is found in RecordTemplate.py",
            self.type_label(oracle)
        ))
    }

    fn coerce(&self, _oracle: &dyn CodeOracle, nm: &str) -> String {
        nm.to_string()
    }
}

#[derive(Template)]
#[template(syntax = "py", escape = "none", path = "RecordTemplate.py")]
pub struct PythonRecord {
    inner: Record,
}

impl PythonRecord {
    pub fn new(inner: Record, _ci: &ComponentInterface) -> Self {
        Self { inner }
    }

    pub fn inner(&self) -> &Record {
        &self.inner
    }
}

impl CodeDeclaration for PythonRecord {
    fn definition_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        Some(self.render().unwrap())
    }
}
