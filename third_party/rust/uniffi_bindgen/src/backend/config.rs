/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::{Deserialize, Serialize};

/// Config value for template expressions
///
/// These are strings that support simple template substitution.  `{}` gets replaced by a value.
#[derive(Debug, Default, Clone, Serialize, Deserialize)]
pub struct TemplateExpression(String);

impl TemplateExpression {
    pub fn render(&self, var: &str) -> String {
        self.0.replace("{}", var)
    }
}
