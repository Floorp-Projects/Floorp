/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Manage the universe of ComponentInterfaces
//!
//! uniffi-bindgen-gecko-js is unique because it generates bindings over a set of UDL files rather
//! than just one.  This is because we want to generate the WebIDL statically rather than generate
//! it.  To accomplish that, each WebIDL function inputs an opaque integer id that identifies which
//! version of it should run, for example `CallSync` inputs a function id.  Operating on all UDL
//! files at once simplifies the task of ensuring those ids are to be unique and consistent between
//! the JS and c++ code.
//!
//! This module manages the list of ComponentInterface and the object ids.

use crate::render::cpp::ComponentInterfaceCppExt;
use anyhow::{bail, Context, Result};
use camino::Utf8PathBuf;
use std::collections::{BTreeSet, HashMap, HashSet};
use uniffi_bindgen::interface::{ComponentInterface, FFIFunction, Object};

pub struct ComponentInterfaceUniverse {
    ci_list: Vec<ComponentInterface>,
    fixture_ci_list: Vec<ComponentInterface>,
}

impl ComponentInterfaceUniverse {
    pub fn new(udl_files: Vec<Utf8PathBuf>, fixture_udl_files: Vec<Utf8PathBuf>) -> Result<Self> {
        let ci_list = udl_files
            .into_iter()
            .map(parse_udl_file)
            .collect::<Result<Vec<_>>>()?;
        let fixture_ci_list = fixture_udl_files
            .into_iter()
            .map(parse_udl_file)
            .collect::<Result<Vec<_>>>()?;
        Self::check_udl_namespaces_unique(&ci_list, &fixture_ci_list)?;
        Ok(Self {
            ci_list,
            fixture_ci_list,
        })
    }

    fn check_udl_namespaces_unique(
        ci_list: &Vec<ComponentInterface>,
        fixture_ci_list: &Vec<ComponentInterface>,
    ) -> Result<()> {
        let mut set = HashSet::new();
        for ci in ci_list.iter().chain(fixture_ci_list.iter()) {
            if !set.insert(ci.namespace()) {
                bail!("UDL files have duplicate namespace: {}", ci.namespace());
            }
        }
        Ok(())
    }

    pub fn ci_list(&self) -> &Vec<ComponentInterface> {
        &self.ci_list
    }

    pub fn fixture_ci_list(&self) -> &Vec<ComponentInterface> {
        &self.fixture_ci_list
    }

    pub fn iter_all(&self) -> impl Iterator<Item = &ComponentInterface> {
        self.ci_list.iter().chain(self.fixture_ci_list.iter())
    }
}

fn parse_udl_file(udl_file: Utf8PathBuf) -> Result<ComponentInterface> {
    let udl = std::fs::read_to_string(&udl_file).context("Error reading UDL file")?;
    ComponentInterface::from_webidl(&udl).context("Failed to parse UDL")
}

pub struct FunctionIds<'a> {
    // Map (CI namespace, func name) -> Ids
    map: HashMap<(&'a str, &'a str), usize>,
}

impl<'a> FunctionIds<'a> {
    pub fn new(cis: &'a ComponentInterfaceUniverse) -> Self {
        Self {
            map: cis
                .iter_all()
                .flat_map(|ci| {
                    ci.exposed_functions()
                        .into_iter()
                        .map(move |f| (ci.namespace(), f.name()))
                })
                .enumerate()
                .map(|(i, (namespace, name))| ((namespace, name), i))
                // Sort using BTreeSet to guarantee the IDs remain stable across runs
                .collect::<BTreeSet<_>>()
                .into_iter()
                .collect(),
        }
    }

    pub fn get(&self, ci: &ComponentInterface, func: &FFIFunction) -> usize {
        return *self.map.get(&(ci.namespace(), func.name())).unwrap();
    }

    pub fn name(&self, ci: &ComponentInterface, func: &FFIFunction) -> String {
        format!("{}:{}", ci.namespace(), func.name())
    }
}

pub struct ObjectIds<'a> {
    // Map (CI namespace, object name) -> Ids
    map: HashMap<(&'a str, &'a str), usize>,
}

impl<'a> ObjectIds<'a> {
    pub fn new(cis: &'a ComponentInterfaceUniverse) -> Self {
        Self {
            map: cis
                .iter_all()
                .flat_map(|ci| {
                    ci.object_definitions()
                        .iter()
                        .map(move |o| (ci.namespace(), o.name()))
                })
                .enumerate()
                .map(|(i, (namespace, name))| ((namespace, name), i))
                // Sort using BTreeSet to guarantee the IDs remain stable across runs
                .collect::<BTreeSet<_>>()
                .into_iter()
                .collect(),
        }
    }

    pub fn get(&self, ci: &ComponentInterface, obj: &Object) -> usize {
        return *self.map.get(&(ci.namespace(), obj.name())).unwrap();
    }

    pub fn name(&self, ci: &ComponentInterface, obj: &Object) -> String {
        format!("{}:{}", ci.namespace(), obj.name())
    }
}
