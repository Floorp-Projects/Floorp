/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{AsCodeType, CodeType};
use crate::backend::{Literal, Type};
use crate::ComponentInterface;
use paste::paste;

fn render_literal(literal: &Literal, inner: &Type, ci: &ComponentInterface) -> String {
    match literal {
        Literal::Null => "null".into(),
        Literal::EmptySequence => "listOf()".into(),
        Literal::EmptyMap => "mapOf()".into(),

        // For optionals
        _ => super::KotlinCodeOracle.find(inner).literal(literal, ci),
    }
}

macro_rules! impl_code_type_for_compound {
     ($T:ty, $type_label_pattern:literal, $canonical_name_pattern: literal) => {
        paste! {
            #[derive(Debug)]
            pub struct $T {
                inner: Type,
            }

            impl $T {
                pub fn new(inner: Type) -> Self {
                    Self { inner }
                }
                fn inner(&self) -> &Type {
                    &self.inner
                }
            }

            impl CodeType for $T  {
                fn type_label(&self, ci: &ComponentInterface) -> String {
                    format!($type_label_pattern, super::KotlinCodeOracle.find(self.inner()).type_label(ci))
                }

                fn canonical_name(&self) -> String {
                    format!($canonical_name_pattern, super::KotlinCodeOracle.find(self.inner()).canonical_name())
                }

                fn literal(&self, literal: &Literal, ci: &ComponentInterface) -> String {
                    render_literal(literal, self.inner(), ci)
                }
            }
        }
    }
 }

impl_code_type_for_compound!(OptionalCodeType, "{}?", "Optional{}");
impl_code_type_for_compound!(SequenceCodeType, "List<{}>", "Sequence{}");

#[derive(Debug)]
pub struct MapCodeType {
    key: Type,
    value: Type,
}

impl MapCodeType {
    pub fn new(key: Type, value: Type) -> Self {
        Self { key, value }
    }

    fn key(&self) -> &Type {
        &self.key
    }

    fn value(&self) -> &Type {
        &self.value
    }
}

impl CodeType for MapCodeType {
    fn type_label(&self, ci: &ComponentInterface) -> String {
        format!(
            "Map<{}, {}>",
            super::KotlinCodeOracle.find(self.key()).type_label(ci),
            super::KotlinCodeOracle.find(self.value()).type_label(ci),
        )
    }

    fn canonical_name(&self) -> String {
        format!(
            "Map{}{}",
            self.key().as_codetype().canonical_name(),
            self.value().as_codetype().canonical_name(),
        )
    }

    fn literal(&self, literal: &Literal, ci: &ComponentInterface) -> String {
        render_literal(literal, &self.value, ci)
    }
}
