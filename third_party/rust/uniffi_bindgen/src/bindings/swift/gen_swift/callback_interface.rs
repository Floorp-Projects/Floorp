/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeDeclaration, CodeOracle, CodeType, Literal};
use crate::interface::{CallbackInterface, ComponentInterface};
use askama::Template;

use super::filters;
use super::Config;

pub struct CallbackInterfaceCodeType {
    id: String,
}

impl CallbackInterfaceCodeType {
    pub fn new(id: String) -> Self {
        Self { id }
    }
}

impl CodeType for CallbackInterfaceCodeType {
    fn type_label(&self, oracle: &dyn CodeOracle) -> String {
        oracle.class_name(&self.id)
    }

    fn canonical_name(&self, oracle: &dyn CodeOracle) -> String {
        format!("CallbackInterface{}", self.type_label(oracle))
    }

    fn literal(&self, _oracle: &dyn CodeOracle, _literal: &Literal) -> String {
        unreachable!();
    }

    fn helper_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        None
    }
}

#[derive(Template)]
#[template(
    syntax = "swift",
    escape = "none",
    path = "CallbackInterfaceTemplate.swift"
)]
pub struct SwiftCallbackInterface {
    inner: CallbackInterface,
    config: Config,
}

impl SwiftCallbackInterface {
    pub fn new(inner: CallbackInterface, _ci: &ComponentInterface, config: Config) -> Self {
        Self { inner, config }
    }
    pub fn inner(&self) -> &CallbackInterface {
        &self.inner
    }
}

impl CodeDeclaration for SwiftCallbackInterface {
    fn initialization_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        None
    }

    fn definition_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        Some(self.render().unwrap())
    }

    fn imports(&self, _oracle: &dyn CodeOracle) -> Option<Vec<String>> {
        None
    }
}

#[derive(Template)]
#[template(
    syntax = "swift",
    escape = "none",
    path = "CallbackInterfaceRuntime.swift"
)]
pub struct SwiftCallbackInterfaceRuntime {
    is_needed: bool,
}

impl SwiftCallbackInterfaceRuntime {
    pub fn new(ci: &ComponentInterface) -> Self {
        Self {
            is_needed: !ci.iter_callback_interface_definitions().is_empty(),
        }
    }
}

impl CodeDeclaration for SwiftCallbackInterfaceRuntime {
    fn definition_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        if !self.is_needed {
            None
        } else {
            Some(self.render().unwrap())
        }
    }
}
