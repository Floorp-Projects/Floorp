/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{filters, CustomTypeConfig};
use crate::backend::{CodeDeclaration, CodeOracle, CodeType, Literal};
use crate::interface::{FFIType, Type};
use askama::Template;
use std::borrow::Borrow;

pub struct CustomCodeType {
    name: String,
}

impl CustomCodeType {
    pub fn new(name: String) -> Self {
        CustomCodeType { name }
    }
}

impl CodeType for CustomCodeType {
    fn type_label(&self, _oracle: &dyn CodeOracle) -> String {
        self.name.clone()
    }

    fn canonical_name(&self, _oracle: &dyn CodeOracle) -> String {
        format!("Type{}", self.name)
    }

    fn literal(&self, _oracle: &dyn CodeOracle, _literal: &Literal) -> String {
        // No such thing as a literal custom type
        unreachable!("Can't have a literal of a custom type");
    }

    fn helper_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        Some(format!(
            "// Helper code for {} is found in CustomType.py",
            self.name,
        ))
    }
}

#[derive(Template)]
#[template(syntax = "swift", escape = "none", path = "CustomType.swift")]
pub struct SwiftCustomType {
    name: String,
    builtin: Type,
    config: Option<CustomTypeConfig>,
}

impl SwiftCustomType {
    pub fn new(name: String, builtin: Type, config: Option<CustomTypeConfig>) -> Self {
        SwiftCustomType {
            name,
            builtin,
            config,
        }
    }

    fn builtin_ffi_type(&self) -> FFIType {
        FFIType::from(&self.builtin)
    }
}

impl CodeDeclaration for SwiftCustomType {
    fn definition_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        Some(self.render().unwrap())
    }

    fn imports(&self, _oracle: &dyn CodeOracle) -> Option<Vec<String>> {
        match &self.config {
            None => None,
            Some(custom_type_config) => custom_type_config.imports.clone(),
        }
    }
}
