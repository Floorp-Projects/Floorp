/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{Metadata, NamespaceMetadata};
use anyhow::{bail, Result};
use std::collections::{BTreeSet, HashMap};

/// Group metadata by namespace
pub fn group_metadata(items: Vec<Metadata>) -> Result<Vec<MetadataGroup>> {
    // Map crate names to MetadataGroup instances
    let mut group_map = items
        .iter()
        .filter_map(|i| match i {
            Metadata::Namespace(namespace) => {
                let group = MetadataGroup {
                    namespace: namespace.clone(),
                    items: BTreeSet::new(),
                };
                Some((namespace.crate_name.clone(), group))
            }
            _ => None,
        })
        .collect::<HashMap<_, _>>();

    for item in items {
        let (item_desc, module_path) = match &item {
            Metadata::Namespace(_) => continue,
            Metadata::UdlFile(meta) => (format!("udl_file `{}`", meta.name), &meta.module_path),
            Metadata::Func(meta) => (format!("function `{}`", meta.name), &meta.module_path),
            Metadata::Constructor(meta) => (
                format!("constructor `{}.{}`", meta.self_name, meta.name),
                &meta.module_path,
            ),
            Metadata::Method(meta) => (
                format!("method `{}.{}`", meta.self_name, meta.name),
                &meta.module_path,
            ),
            Metadata::Record(meta) => (format!("record `{}`", meta.name), &meta.module_path),
            Metadata::Enum(meta) => (format!("enum `{}`", meta.name), &meta.module_path),
            Metadata::Object(meta) => (format!("object `{}`", meta.name), &meta.module_path),
            Metadata::CallbackInterface(meta) => (
                format!("callback interface `{}`", meta.name),
                &meta.module_path,
            ),
            Metadata::TraitMethod(meta) => {
                (format!("trait method`{}`", meta.name), &meta.module_path)
            }
            Metadata::Error(meta) => (format!("error `{}`", meta.name()), meta.module_path()),
        };

        let crate_name = module_path.split("::").next().unwrap();
        let group = match group_map.get_mut(crate_name) {
            Some(ns) => ns,
            None => bail!("Unknown namespace for {item_desc} ({crate_name})"),
        };
        if group.items.contains(&item) {
            bail!("Duplicate metadata item: {item:?}");
        }
        group.items.insert(item);
    }
    Ok(group_map.into_values().collect())
}

#[derive(Debug)]
pub struct MetadataGroup {
    pub namespace: NamespaceMetadata,
    pub items: BTreeSet<Metadata>,
}
