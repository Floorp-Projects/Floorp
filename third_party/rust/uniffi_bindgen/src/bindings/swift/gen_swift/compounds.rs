/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeOracle, CodeType, Literal, TypeIdentifier};
use askama::Template;
use paste::paste;

// Used in template files.
use super::filters;

fn render_literal(oracle: &dyn CodeOracle, literal: &Literal, inner: &TypeIdentifier) -> String {
    match literal {
        Literal::Null => "nil".into(),
        Literal::EmptySequence => "[]".into(),
        Literal::EmptyMap => "[:]".into(),

        // For optionals
        _ => oracle.find(inner).literal(oracle, literal),
    }
}

macro_rules! impl_code_type_for_compound {
     ($T:ty, $type_label_pattern:literal, $canonical_name_pattern:literal, $template_file:literal) => {
        paste! {
            #[derive(Template)]
            #[template(syntax = "swift", escape = "none", path = $template_file)]
            pub struct $T {
                inner: TypeIdentifier,
                outer: TypeIdentifier,
            }

            impl $T {
                pub fn new(inner: TypeIdentifier, outer: TypeIdentifier) -> Self {
                    Self { inner, outer }
                }
                fn inner(&self) -> &TypeIdentifier {
                    &self.inner
                }
                fn outer(&self) -> &TypeIdentifier {
                    &self.outer
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

                fn helper_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
                    Some(self.render().unwrap())
                }
            }
        }
    }
 }

impl_code_type_for_compound!(
    OptionalCodeType,
    "{}?",
    "Option{}",
    "OptionalTemplate.swift"
);

impl_code_type_for_compound!(
    SequenceCodeType,
    "[{}]",
    "Sequence{}",
    "SequenceTemplate.swift"
);

#[derive(Template)]
#[template(syntax = "swift", escape = "none", path = "MapTemplate.swift")]
pub struct MapCodeType {
    key: TypeIdentifier,
    value: TypeIdentifier,
    outer: TypeIdentifier,
}

impl MapCodeType {
    pub fn new(key: TypeIdentifier, value: TypeIdentifier, outer: TypeIdentifier) -> Self {
        Self { key, value, outer }
    }

    fn key(&self) -> &TypeIdentifier {
        &self.key
    }

    fn value(&self) -> &TypeIdentifier {
        &self.value
    }

    fn outer(&self) -> &TypeIdentifier {
        &self.outer
    }
}

impl CodeType for MapCodeType {
    fn type_label(&self, oracle: &dyn CodeOracle) -> String {
        format!(
            "[{}: {}]",
            oracle.find(self.key()).type_label(oracle),
            oracle.find(self.value()).type_label(oracle)
        )
    }

    fn canonical_name(&self, oracle: &dyn CodeOracle) -> String {
        format!(
            "Dictionary{}{}",
            oracle.find(self.key()).type_label(oracle),
            oracle.find(self.value()).type_label(oracle)
        )
    }

    fn literal(&self, oracle: &dyn CodeOracle, literal: &Literal) -> String {
        render_literal(oracle, literal, self.value())
    }

    fn helper_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        Some(self.render().unwrap())
    }
}
