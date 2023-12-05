/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Manage the universe of ComponentInterfaces / Configs
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
use crate::{Config, ConfigMap};
use anyhow::{bail, Context, Result};
use camino::Utf8PathBuf;
use std::collections::{BTreeSet, HashMap, HashSet};
use uniffi_bindgen::interface::{CallbackInterface, ComponentInterface, FfiFunction, Object};

pub struct ComponentUniverse {
    pub components: Vec<(ComponentInterface, Config)>,
    pub fixture_components: Vec<(ComponentInterface, Config)>,
}

impl ComponentUniverse {
    pub fn new(
        udl_files: Vec<Utf8PathBuf>,
        fixture_udl_files: Vec<Utf8PathBuf>,
        config_map: ConfigMap,
    ) -> Result<Self> {
        let components = udl_files
            .into_iter()
            .map(|udl_file| parse_udl_file(udl_file, &config_map))
            .collect::<Result<Vec<_>>>()?;
        let fixture_components = fixture_udl_files
            .into_iter()
            .map(|udl_file| parse_udl_file(udl_file, &config_map))
            .collect::<Result<Vec<_>>>()?;
        let universe = Self {
            components,
            fixture_components,
        };
        universe.check_udl_namespaces_unique()?;
        universe.check_callback_interfaces()?;
        Ok(universe)
    }

    fn check_udl_namespaces_unique(&self) -> Result<()> {
        let mut set = HashSet::new();
        for ci in self.iter_cis() {
            if !set.insert(ci.namespace()) {
                bail!("UDL files have duplicate namespace: {}", ci.namespace());
            }
        }
        Ok(())
    }

    fn check_callback_interfaces(&self) -> Result<()> {
        // We don't currently support callback interfaces returning values or throwing errors.
        for ci in self.iter_cis() {
            for cbi in ci.callback_interface_definitions() {
                for method in cbi.methods() {
                    if method.return_type().is_some() {
                        bail!("Callback interface method {}.{} throws an error, which is not yet supported", cbi.name(), method.name())
                    }
                    if method.throws_type().is_some() {
                        bail!("Callback interface method {}.{} returns a value, which is not yet supported", cbi.name(), method.name())
                    }
                }
            }
        }
        Ok(())
    }

    pub fn iter_cis(&self) -> impl Iterator<Item = &ComponentInterface> {
        self.components
            .iter()
            .chain(self.fixture_components.iter())
            .map(|(ci, _)| ci)
    }
}

fn parse_udl_file(
    udl_file: Utf8PathBuf,
    config_map: &ConfigMap,
) -> Result<(ComponentInterface, Config)> {
    let udl = std::fs::read_to_string(&udl_file).context("Error reading UDL file")?;
    let ci = ComponentInterface::from_webidl(&udl).context("Failed to parse UDL")?;
    let config = config_map.get(ci.namespace()).cloned().unwrap_or_default();
    Ok((ci, config))
}

pub struct FunctionIds<'a> {
    // Map (CI namespace, func name) -> Ids
    map: HashMap<(&'a str, &'a str), usize>,
}

impl<'a> FunctionIds<'a> {
    pub fn new(cis: &'a ComponentUniverse) -> Self {
        Self {
            map: cis
                .iter_cis()
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

    pub fn get(&self, ci: &ComponentInterface, func: &FfiFunction) -> usize {
        return *self.map.get(&(ci.namespace(), func.name())).unwrap();
    }

    pub fn name(&self, ci: &ComponentInterface, func: &FfiFunction) -> String {
        format!("{}:{}", ci.namespace(), func.name())
    }
}

pub struct ObjectIds<'a> {
    // Map (CI namespace, object name) -> Ids
    map: HashMap<(&'a str, &'a str), usize>,
}

impl<'a> ObjectIds<'a> {
    pub fn new(cis: &'a ComponentUniverse) -> Self {
        Self {
            map: cis
                .iter_cis()
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

pub struct CallbackIds<'a> {
    // Map (CI namespace, callback name) -> Ids
    map: HashMap<(&'a str, &'a str), usize>,
}

impl<'a> CallbackIds<'a> {
    pub fn new(cis: &'a ComponentUniverse) -> Self {
        Self {
            map: cis
                .iter_cis()
                .flat_map(|ci| {
                    ci.callback_interface_definitions()
                        .iter()
                        .map(move |cb| (ci.namespace(), cb.name()))
                })
                .enumerate()
                .map(|(i, (namespace, name))| ((namespace, name), i))
                // Sort using BTreeSet to guarantee the IDs remain stable across runs
                .collect::<BTreeSet<_>>()
                .into_iter()
                .collect(),
        }
    }

    pub fn get(&self, ci: &ComponentInterface, cb: &CallbackInterface) -> usize {
        return *self.map.get(&(ci.namespace(), cb.name())).unwrap();
    }

    pub fn name(&self, ci: &ComponentInterface, cb: &CallbackInterface) -> String {
        format!("{}:{}", ci.namespace(), cb.name())
    }
}
