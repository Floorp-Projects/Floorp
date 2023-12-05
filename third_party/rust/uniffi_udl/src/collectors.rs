/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Collects metadata from UDL.

use crate::attributes;
use crate::converters::APIConverter;
use crate::finder;
use crate::resolver::TypeResolver;
use anyhow::{bail, Result};
use std::collections::{hash_map, BTreeSet, HashMap};
use uniffi_meta::Type;

/// The implementation of this crate - we collect weedle definitions from UDL and convert
/// them into `uniffi_meta` metadata.
/// We don't really check the sanity of the output in terms of type correctness/duplications/etc
/// etc, that's the job of the consumer.
#[derive(Debug, Default)]
pub(crate) struct InterfaceCollector {
    /// All of the types used in the interface.
    pub types: TypeCollector,
    /// The output we collect and supply to our consumer.
    pub items: BTreeSet<uniffi_meta::Metadata>,
}

impl InterfaceCollector {
    /// Parse an `InterfaceCollector` from a string containing a WebIDL definition.
    pub fn from_webidl(idl: &str, crate_name: &str) -> Result<Self> {
        let mut ci = Self::default();
        // There's some lifetime thing with the errors returned from weedle::Definitions::parse
        // that my own lifetime is too short to worry about figuring out; unwrap and move on.

        // Note we use `weedle::Definitions::parse` instead of `weedle::parse` so
        // on parse errors we can see how far weedle got, which helps locate the problem.
        use weedle::Parse; // this trait must be in scope for parse to work.
        let (remaining, defns) = weedle::Definitions::parse(idl.trim()).unwrap();
        if !remaining.is_empty() {
            println!("Error parsing the IDL. Text remaining to be parsed is:");
            println!("{remaining}");
            bail!("parse error");
        }
        // We process the WebIDL definitions in 3 passes.
        // First, find the namespace.
        // XXX - TODO: it's no longer necessary to do this pass.
        ci.types.namespace = ci.find_namespace(&defns)?;
        ci.types.crate_name = crate_name.to_string();
        // Next, go through and look for all the named types.
        ci.types.add_type_definitions_from(defns.as_slice())?;

        // With those names resolved, we can build a complete representation of the API.
        APIBuilder::process(&defns, &mut ci)?;
        // Any misc items we need to add to the set.
        for t in ci.types.type_definitions.values() {
            if let Type::Custom {
                module_path,
                name,
                builtin,
            } = t
            {
                ci.items.insert(
                    uniffi_meta::CustomTypeMetadata {
                        module_path: module_path.clone(),
                        name: name.clone(),
                        builtin: (**builtin).clone(),
                    }
                    .into(),
                );
            }
        }
        Ok(ci)
    }

    fn find_namespace(&mut self, defns: &Vec<weedle::Definition<'_>>) -> Result<String> {
        for defn in defns {
            if let weedle::Definition::Namespace(n) = defn {
                return Ok(n.identifier.0.to_string());
            }
        }
        bail!("Failed to find the namespace");
    }

    /// The module path which should be used by all items in this namespace.
    pub fn module_path(&self) -> String {
        self.types.module_path()
    }

    /// Get a specific type
    pub fn get_type(&self, name: &str) -> Option<Type> {
        self.types.get_type_definition(name)
    }

    /// Resolve a weedle type expression into a `Type`.
    ///
    /// This method uses the current state of our `TypeCollector` to turn a weedle type expression
    /// into a concrete `Type` (or error if the type expression is not well defined). It abstracts
    /// away the complexity of walking weedle's type struct hierarchy by dispatching to the `TypeResolver`
    /// trait.
    pub fn resolve_type_expression<T: TypeResolver>(&mut self, expr: T) -> Result<Type> {
        self.types.resolve_type_expression(expr)
    }

    /// Resolve a weedle `ReturnType` expression into an optional `Type`.
    ///
    /// This method is similar to `resolve_type_expression`, but tailored specifically for return types.
    /// It can return `None` to represent a non-existent return value.
    pub fn resolve_return_type_expression(
        &mut self,
        expr: &weedle::types::ReturnType<'_>,
    ) -> Result<Option<Type>> {
        Ok(match expr {
            weedle::types::ReturnType::Undefined(_) => None,
            weedle::types::ReturnType::Type(t) => {
                // Older versions of WebIDL used `void` for functions that don't return a value,
                // while newer versions have replaced it with `undefined`. Special-case this for
                // backwards compatibility for our consumers.
                use weedle::types::{NonAnyType::Identifier, SingleType::NonAny, Type::Single};
                match t {
                    Single(NonAny(Identifier(id))) if id.type_.0 == "void" => None,
                    _ => Some(self.resolve_type_expression(t)?),
                }
            }
        })
    }

    /// Called by `APIBuilder` impls to add a newly-parsed definition to the `InterfaceCollector`.
    fn add_definition(&mut self, defn: uniffi_meta::Metadata) -> Result<()> {
        self.items.insert(defn);
        Ok(())
    }
}

/// Turn our internal object into an outgoing public `MetadataGroup`.
impl From<InterfaceCollector> for uniffi_meta::MetadataGroup {
    fn from(value: InterfaceCollector) -> Self {
        Self {
            namespace: uniffi_meta::NamespaceMetadata {
                crate_name: value.types.module_path(),
                name: value.types.namespace,
            },
            items: value.items,
        }
    }
}

/// Trait to help build an `InterfaceCollector` from WedIDL syntax nodes.
///
/// This trait does structural matching on the various weedle AST nodes and
/// uses them to build up the records, enums, objects etc in the provided
/// `InterfaceCollector`.
trait APIBuilder {
    fn process(&self, ci: &mut InterfaceCollector) -> Result<()>;
}

/// Add to an `InterfaceCollector` from a list of weedle definitions,
/// by processing each in turn.
impl<T: APIBuilder> APIBuilder for Vec<T> {
    fn process(&self, ci: &mut InterfaceCollector) -> Result<()> {
        for item in self {
            item.process(ci)?;
        }
        Ok(())
    }
}

/// Add to an `InterfaceCollector` from a weedle definition.
/// This is conceptually the root of the parser, and dispatches to implementations
/// for the various specific WebIDL types that we support.
impl APIBuilder for weedle::Definition<'_> {
    fn process(&self, ci: &mut InterfaceCollector) -> Result<()> {
        match self {
            weedle::Definition::Namespace(d) => d.process(ci)?,
            weedle::Definition::Enum(d) => {
                // We check if the enum represents an error...
                let attrs = attributes::EnumAttributes::try_from(d.attributes.as_ref())?;
                if attrs.contains_error_attr() {
                    let e: uniffi_meta::ErrorMetadata = d.convert(ci)?;
                    ci.add_definition(e.into())?;
                } else {
                    let e: uniffi_meta::EnumMetadata = d.convert(ci)?;
                    ci.add_definition(e.into())?;
                }
            }
            weedle::Definition::Dictionary(d) => {
                let rec = d.convert(ci)?;
                ci.add_definition(rec.into())?;
            }
            weedle::Definition::Interface(d) => {
                let attrs = attributes::InterfaceAttributes::try_from(d.attributes.as_ref())?;
                if attrs.contains_enum_attr() {
                    let e: uniffi_meta::EnumMetadata = d.convert(ci)?;
                    ci.add_definition(e.into())?;
                } else if attrs.contains_error_attr() {
                    let e: uniffi_meta::ErrorMetadata = d.convert(ci)?;
                    ci.add_definition(e.into())?;
                } else {
                    let obj: uniffi_meta::ObjectMetadata = d.convert(ci)?;
                    ci.add_definition(obj.into())?;
                }
            }
            weedle::Definition::CallbackInterface(d) => {
                let obj = d.convert(ci)?;
                ci.add_definition(obj.into())?;
            }
            // everything needed for typedefs is done in finder.rs.
            weedle::Definition::Typedef(_) => {}
            _ => bail!("don't know how to deal with {:?}", self),
        }
        Ok(())
    }
}

impl APIBuilder for weedle::NamespaceDefinition<'_> {
    fn process(&self, ci: &mut InterfaceCollector) -> Result<()> {
        if self.attributes.is_some() {
            bail!("namespace attributes are not supported yet");
        }
        if self.identifier.0 != ci.types.namespace {
            bail!("duplicate namespace definition");
        }
        for func in self.members.body.convert(ci)? {
            ci.add_definition(func.into())?;
        }
        Ok(())
    }
}

#[derive(Debug, Default)]
pub(crate) struct TypeCollector {
    /// The unique prefix that we'll use for namespacing when exposing this component's API.
    pub namespace: String,

    pub crate_name: String,

    // Named type definitions (including aliases).
    pub type_definitions: HashMap<String, Type>,
}

impl TypeCollector {
    /// The module path which should be used by all items in this namespace.
    pub fn module_path(&self) -> String {
        self.crate_name.clone()
    }

    /// Add the definitions of all named [Type]s from a given WebIDL definition.
    ///
    /// This will fail if you try to add a name for which an existing type definition exists.
    pub fn add_type_definitions_from<T: finder::TypeFinder>(&mut self, defn: T) -> Result<()> {
        defn.add_type_definitions_to(self)
    }

    /// Add the definition of a named [Type].
    ///
    /// This will fail if you try to add a name for which an existing type definition exists.
    pub fn add_type_definition(&mut self, name: &str, type_: Type) -> Result<()> {
        match self.type_definitions.entry(name.to_string()) {
            hash_map::Entry::Occupied(o) => {
                let existing_def = o.get();
                if type_ == *existing_def
                    && matches!(type_, Type::Record { .. } | Type::Enum { .. })
                {
                    // UDL and proc-macro metadata are allowed to define the same record, enum and
                    // error types, if the definitions match (fields and variants are checked in
                    // add_{record,enum,error}_definition)
                    Ok(())
                } else {
                    bail!(
                        "Conflicting type definition for `{name}`! \
                         existing definition: {existing_def:?}, \
                         new definition: {type_:?}"
                    );
                }
            }
            hash_map::Entry::Vacant(e) => {
                e.insert(type_);
                Ok(())
            }
        }
    }

    /// Get the [Type] corresponding to a given name, if any.
    pub fn get_type_definition(&self, name: &str) -> Option<Type> {
        self.type_definitions.get(name).cloned()
    }

    /// Get the [Type] corresponding to a given WebIDL type node.
    ///
    /// If the node is a structural type (e.g. a sequence) then this will also add
    /// it to the set of all types seen in the component interface.
    pub fn resolve_type_expression<T: TypeResolver>(&mut self, expr: T) -> Result<Type> {
        expr.resolve_type_expression(self)
    }
}
