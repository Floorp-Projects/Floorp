/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeDeclaration, CodeOracle};
use crate::interface::{ComponentInterface, Function};
use askama::Template;
use std::borrow::Borrow;

use super::filters;

#[derive(Template)]
#[template(syntax = "py", escape = "none", path = "TopLevelFunctionTemplate.py")]
pub struct PythonFunction {
    inner: Function,
}

impl PythonFunction {
    pub fn new(inner: Function, _ci: &ComponentInterface) -> Self {
        Self { inner }
    }
    pub fn inner(&self) -> &Function {
        &self.inner
    }
}

impl CodeDeclaration for PythonFunction {
    fn definition_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        Some(self.render().unwrap())
    }
}
