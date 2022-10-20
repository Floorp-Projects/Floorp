/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeOracle, CodeType, Literal, TypeIdentifier};

pub struct OptionalCodeType {
    inner: TypeIdentifier,
}

impl OptionalCodeType {
    pub fn new(inner: TypeIdentifier) -> Self {
        Self { inner }
    }
}

impl CodeType for OptionalCodeType {
    fn type_label(&self, oracle: &dyn CodeOracle) -> String {
        format!("{}?", oracle.find(&self.inner).type_label(oracle))
    }

    fn canonical_name(&self, oracle: &dyn CodeOracle) -> String {
        format!("Option{}", oracle.find(&self.inner).canonical_name(oracle))
    }

    fn literal(&self, oracle: &dyn CodeOracle, literal: &Literal) -> String {
        match literal {
            Literal::Null => "nil".into(),
            _ => oracle.find(&self.inner).literal(oracle, literal),
        }
    }
}

pub struct SequenceCodeType {
    inner: TypeIdentifier,
}

impl SequenceCodeType {
    pub fn new(inner: TypeIdentifier) -> Self {
        Self { inner }
    }
}

impl CodeType for SequenceCodeType {
    fn type_label(&self, oracle: &dyn CodeOracle) -> String {
        format!("[{}]", oracle.find(&self.inner).type_label(oracle))
    }

    fn canonical_name(&self, oracle: &dyn CodeOracle) -> String {
        format!(
            "Sequence{}",
            oracle.find(&self.inner).canonical_name(oracle)
        )
    }

    fn literal(&self, _oracle: &dyn CodeOracle, literal: &Literal) -> String {
        match literal {
            Literal::EmptySequence => "[]".into(),
            _ => unreachable!(),
        }
    }
}

pub struct MapCodeType {
    key: TypeIdentifier,
    value: TypeIdentifier,
}

impl MapCodeType {
    pub fn new(key: TypeIdentifier, value: TypeIdentifier) -> Self {
        Self { key, value }
    }
}

impl CodeType for MapCodeType {
    fn type_label(&self, oracle: &dyn CodeOracle) -> String {
        format!(
            "[{}: {}]",
            oracle.find(&self.key).type_label(oracle),
            oracle.find(&self.value).type_label(oracle)
        )
    }

    fn canonical_name(&self, oracle: &dyn CodeOracle) -> String {
        format!(
            "Dictionary{}{}",
            oracle.find(&self.key).canonical_name(oracle),
            oracle.find(&self.value).canonical_name(oracle)
        )
    }

    fn literal(&self, _oracle: &dyn CodeOracle, literal: &Literal) -> String {
        match literal {
            Literal::EmptyMap => "[:]".into(),
            _ => unreachable!(),
        }
    }
}
