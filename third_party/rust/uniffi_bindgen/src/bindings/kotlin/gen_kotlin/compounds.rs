/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeOracle, CodeType, Literal, TypeIdentifier};
use paste::paste;

fn render_literal(oracle: &dyn CodeOracle, literal: &Literal, inner: &TypeIdentifier) -> String {
    match literal {
        Literal::Null => "null".into(),
        Literal::EmptySequence => "listOf()".into(),
        Literal::EmptyMap => "mapOf()".into(),

        // For optionals
        _ => oracle.find(inner).literal(oracle, literal),
    }
}

macro_rules! impl_code_type_for_compound {
     ($T:ty, $type_label_pattern:literal, $canonical_name_pattern: literal) => {
        paste! {
            pub struct $T {
                inner: TypeIdentifier,
            }

            impl $T {
                pub fn new(inner: TypeIdentifier) -> Self {
                    Self { inner }
                }
                fn inner(&self) -> &TypeIdentifier {
                    &self.inner
                }
            }

            impl CodeType for $T  {
                fn type_label(&self, oracle: &dyn CodeOracle) -> String {
                    format!($type_label_pattern, oracle.find(self.inner()).type_label(oracle))
                }

                fn canonical_name(&self, oracle: &dyn CodeOracle) -> String {
                    format!($canonical_name_pattern, oracle.find(self.inner()).canonical_name(oracle))
                }

                fn literal(&self, oracle: &dyn CodeOracle, literal: &Literal) -> String {
                    render_literal(oracle, literal, self.inner())
                }
            }
        }
    }
 }

impl_code_type_for_compound!(OptionalCodeType, "{}?", "Optional{}");
impl_code_type_for_compound!(SequenceCodeType, "List<{}>", "Sequence{}");

pub struct MapCodeType {
    key: TypeIdentifier,
    value: TypeIdentifier,
}

impl MapCodeType {
    pub fn new(key: TypeIdentifier, value: TypeIdentifier) -> Self {
        Self { key, value }
    }

    fn key(&self) -> &TypeIdentifier {
        &self.key
    }

    fn value(&self) -> &TypeIdentifier {
        &self.value
    }
}

impl CodeType for MapCodeType {
    fn type_label(&self, oracle: &dyn CodeOracle) -> String {
        format!(
            "Map<{}, {}>",
            self.key().type_label(oracle),
            self.value().type_label(oracle),
        )
    }

    fn canonical_name(&self, oracle: &dyn CodeOracle) -> String {
        format!(
            "Map{}{}",
            self.key().type_label(oracle),
            self.value().type_label(oracle),
        )
    }

    fn literal(&self, oracle: &dyn CodeOracle, literal: &Literal) -> String {
        render_literal(oracle, literal, &self.value)
    }
}
