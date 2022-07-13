/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeDeclaration, CodeOracle, CodeType, Literal};
use crate::interface::{ComponentInterface, Enum};
use askama::Template;

use super::filters;
pub struct EnumCodeType {
    id: String,
}

impl EnumCodeType {
    pub fn new(id: String) -> Self {
        Self { id }
    }
}

impl CodeType for EnumCodeType {
    fn type_label(&self, oracle: &dyn CodeOracle) -> String {
        oracle.class_name(&self.id)
    }

    fn canonical_name(&self, _oracle: &dyn CodeOracle) -> String {
        format!("Type{}", self.id)
    }

    fn literal(&self, oracle: &dyn CodeOracle, literal: &Literal) -> String {
        if let Literal::Enum(v, _) = literal {
            format!(".{}", oracle.enum_variant_name(v))
        } else {
            unreachable!();
        }
    }

    fn helper_code(&self, oracle: &dyn CodeOracle) -> Option<String> {
        Some(format!(
            "// Helper code for {} enum is found in EnumTemplate.swift",
            self.type_label(oracle)
        ))
    }
}

#[derive(Template)]
#[template(syntax = "swift", escape = "none", path = "EnumTemplate.swift")]
pub struct SwiftEnum {
    inner: Enum,
    contains_object_references: bool,
}

impl SwiftEnum {
    pub fn new(inner: Enum, ci: &ComponentInterface) -> Self {
        Self {
            contains_object_references: ci.item_contains_object_references(&inner),
            inner,
        }
    }
    pub fn inner(&self) -> &Enum {
        &self.inner
    }
    pub fn contains_object_references(&self) -> bool {
        self.contains_object_references
    }
}

impl CodeDeclaration for SwiftEnum {
    fn definition_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        Some(self.render().unwrap())
    }
}
