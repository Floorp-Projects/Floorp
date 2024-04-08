/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::CodeType;
use crate::{
    backend::{Literal, Type},
    bindings::python::gen_python::AsCodeType,
};

#[derive(Debug)]
pub struct OptionalCodeType {
    inner: Type,
}

impl OptionalCodeType {
    pub fn new(inner: Type) -> Self {
        Self { inner }
    }
}

impl CodeType for OptionalCodeType {
    fn type_label(&self) -> String {
        format!(
            "typing.Optional[{}]",
            super::PythonCodeOracle.find(&self.inner).type_label()
        )
    }

    fn canonical_name(&self) -> String {
        format!(
            "Optional{}",
            super::PythonCodeOracle.find(&self.inner).canonical_name(),
        )
    }

    fn literal(&self, literal: &Literal) -> String {
        match literal {
            Literal::None => "None".into(),
            Literal::Some { inner } => super::PythonCodeOracle.find(&self.inner).literal(inner),
            _ => panic!("Invalid literal for Optional type: {literal:?}"),
        }
    }
}

#[derive(Debug)]
pub struct SequenceCodeType {
    inner: Type,
}

impl SequenceCodeType {
    pub fn new(inner: Type) -> Self {
        Self { inner }
    }
}

impl CodeType for SequenceCodeType {
    fn type_label(&self) -> String {
        // Python 3.8 and below do not support `list[T]`
        format!(
            "typing.List[{}]",
            super::PythonCodeOracle.find(&self.inner).type_label()
        )
    }

    fn canonical_name(&self) -> String {
        format!(
            "Sequence{}",
            super::PythonCodeOracle.find(&self.inner).canonical_name(),
        )
    }

    fn literal(&self, literal: &Literal) -> String {
        match literal {
            Literal::EmptySequence => "[]".into(),
            _ => unimplemented!(),
        }
    }
}

#[derive(Debug)]
pub struct MapCodeType {
    key: Type,
    value: Type,
}

impl MapCodeType {
    pub fn new(key: Type, value: Type) -> Self {
        Self { key, value }
    }
}

impl CodeType for MapCodeType {
    fn type_label(&self) -> String {
        format!(
            "dict[{}, {}]",
            self.key.as_codetype().type_label(),
            self.value.as_codetype().type_label()
        )
    }

    fn canonical_name(&self) -> String {
        format!(
            "Map{}{}",
            super::PythonCodeOracle.find(&self.key).canonical_name(),
            super::PythonCodeOracle.find(&self.value).canonical_name(),
        )
    }

    fn literal(&self, literal: &Literal) -> String {
        match literal {
            Literal::EmptyMap => "{}".into(),
            _ => unimplemented!(),
        }
    }
}
