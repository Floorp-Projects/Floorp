/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{AsCodeType, KotlinCodeOracle};
use crate::backend::CodeType;
use crate::interface::Variant;

#[derive(Debug)]
pub(super) struct VariantCodeType {
    pub v: Variant,
}

impl CodeType for VariantCodeType {
    fn type_label(&self) -> String {
        KotlinCodeOracle.class_name(self.v.name())
    }
}

impl AsCodeType for Variant {
    fn as_codetype(&self) -> Box<dyn CodeType> {
        Box::new(VariantCodeType { v: self.clone() })
    }
}

#[derive(Debug)]
pub(super) struct ErrorVariantCodeType {
    pub v: Variant,
}

impl CodeType for ErrorVariantCodeType {
    fn type_label(&self) -> String {
        KotlinCodeOracle.error_name(self.v.name())
    }
}

pub(super) struct ErrorVariantCodeTypeProvider {
    pub v: Variant,
}

impl AsCodeType for ErrorVariantCodeTypeProvider {
    fn as_codetype(&self) -> Box<dyn CodeType> {
        Box::new(ErrorVariantCodeType { v: self.v.clone() })
    }
}
