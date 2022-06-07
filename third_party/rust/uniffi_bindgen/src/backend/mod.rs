/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

mod config;
mod declarations;
mod oracle;
mod types;

pub use config::TemplateExpression;
pub use declarations::CodeDeclaration;
pub use oracle::CodeOracle;
pub use types::CodeType;

pub type TypeIdentifier = crate::interface::Type;
pub type Literal = crate::interface::Literal;
