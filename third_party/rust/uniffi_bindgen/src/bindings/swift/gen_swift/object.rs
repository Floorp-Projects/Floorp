/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeDeclaration, CodeOracle, CodeType, Literal};
use crate::interface::{ComponentInterface, Object};
use askama::Template;

use super::filters;
use super::Config;

pub struct ObjectCodeType {
    id: String,
}

impl ObjectCodeType {
    pub fn new(id: String) -> Self {
        Self { id }
    }
}

impl CodeType for ObjectCodeType {
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
            "// Helper code for {} class is found in ObjectTemplate.swift",
            self.type_label(oracle)
        ))
    }
}

#[derive(Template)]
#[template(syntax = "swift", escape = "none", path = "ObjectTemplate.swift")]
pub struct SwiftObject {
    config: Config,
    inner: Object,
}

impl SwiftObject {
    pub fn new(inner: Object, _ci: &ComponentInterface, config: Config) -> Self {
        Self { inner, config }
    }

    pub fn inner(&self) -> &Object {
        &self.inner
    }
}

impl CodeDeclaration for SwiftObject {
    fn definition_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        Some(self.render().unwrap())
    }

    fn imports(&self, _oracle: &dyn CodeOracle) -> Option<Vec<String>> {
        None
    }
}
