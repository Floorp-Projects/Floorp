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
            format!(
                "{}.{}",
                self.type_label(oracle),
                oracle.enum_variant_name(v)
            )
        } else {
            unreachable!();
        }
    }

    fn helper_code(&self, oracle: &dyn CodeOracle) -> Option<String> {
        Some(format!(
            "# Helper code for {} enum is found in EnumTemplate.py",
            self.type_label(oracle)
        ))
    }

    fn coerce(&self, _oracle: &dyn CodeOracle, nm: &str) -> String {
        nm.to_string()
    }
}

#[derive(Template)]
#[template(syntax = "py", escape = "none", path = "EnumTemplate.py")]
pub struct PythonEnum {
    inner: Enum,
}

impl PythonEnum {
    pub fn new(inner: Enum, _ci: &ComponentInterface) -> Self {
        Self { inner }
    }

    pub fn inner(&self) -> &Enum {
        &self.inner
    }
}

impl CodeDeclaration for PythonEnum {
    fn definition_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        Some(self.render().unwrap())
    }
}
