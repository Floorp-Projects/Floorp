//! #[diplomat::attr] and other attributes

use crate::ast;
use crate::ast::attrs::DiplomatBackendAttrCfg;
use crate::hir::LoweringError;

use quote::ToTokens;
use syn::{LitStr, Meta};

#[non_exhaustive]
#[derive(Clone, Default, Debug)]
pub struct Attrs {
    pub disable: bool,
    pub rename: Option<String>,
    // more to be added: rename, namespace, etc
}

/// Where the attribute was found. Some attributes are only allowed in some contexts
/// (e.g. namespaces cannot be specified on methods)
#[non_exhaustive] // might add module attrs in the future
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum AttributeContext {
    Struct { out: bool },
    Enum,
    EnumVariant,
    Opaque,
    Method,
}

impl Attrs {
    pub fn from_ast(
        ast: &ast::Attrs,
        validator: &(impl AttributeValidator + ?Sized),
        context: AttributeContext,
        errors: &mut Vec<LoweringError>,
    ) -> Self {
        let mut this = Attrs::default();
        let support = validator.attrs_supported();
        for attr in &ast.attrs {
            if validator.satisfies_cfg(&attr.cfg) {
                match &attr.meta {
                    Meta::Path(p) => {
                        if p.is_ident("disable") {
                            if this.disable {
                                errors.push(LoweringError::Other(
                                    "Duplicate `disable` attribute".into(),
                                ));
                            } else if !support.disabling {
                                errors.push(LoweringError::Other(format!(
                                    "`disable` not supported in backend {}",
                                    validator.primary_name()
                                )))
                            } else if context == AttributeContext::EnumVariant {
                                errors.push(LoweringError::Other(
                                    "`disable` cannot be used on enum variants".into(),
                                ))
                            } else {
                                this.disable = true;
                            }
                        } else {
                            errors.push(LoweringError::Other(format!(
                                "Unknown diplomat attribute {p:?}: expected one of: `disable, rename`"
                            )));
                        }
                    }
                    Meta::NameValue(nv) => {
                        let p = &nv.path;
                        if p.is_ident("rename") {
                            if this.rename.is_some() {
                                errors.push(LoweringError::Other(
                                    "Duplicate `rename` attribute".into(),
                                ));
                            } else if !support.renaming {
                                errors.push(LoweringError::Other(format!(
                                    "`rename` not supported in backend {}",
                                    validator.primary_name()
                                )))
                            } else {
                                let v = nv.value.to_token_stream();
                                let l = syn::parse2::<LitStr>(v);
                                if let Ok(ref l) = l {
                                    this.rename = Some(l.value())
                                } else {
                                    errors.push(LoweringError::Other(format!(
                                        "Found diplomat attribute {p:?}: expected string as `rename` argument"
                                    )));
                                }
                            }
                        } else {
                            errors.push(LoweringError::Other(format!(
                                "Unknown diplomat attribute {p:?}: expected one of: `disable, rename`"
                            )));
                        }
                    }
                    other => {
                        errors.push(LoweringError::Other(format!(
                            "Unknown diplomat attribute {other:?}: expected one of: `disable, rename`"
                        )));
                    }
                }
            }
        }

        this
    }
}

#[non_exhaustive]
#[derive(Copy, Clone, Debug, Default)]
pub struct BackendAttrSupport {
    pub disabling: bool,
    pub renaming: bool,
    // more to be added: rename, namespace, etc
}

/// Defined by backends when validating attributes
pub trait AttributeValidator {
    /// The primary name of the backend, for use in diagnostics
    fn primary_name(&self) -> &str;
    /// Does this backend satisfy `cfg(backend_name)`?
    /// (Backends are allowed to satisfy multiple backend names, useful when there
    /// are multiple backends for a language)
    fn is_backend(&self, backend_name: &str) -> bool;
    /// does this backend satisfy cfg(name = value)?
    fn is_name_value(&self, name: &str, value: &str) -> bool;
    /// What backedn attrs does this support?
    fn attrs_supported(&self) -> BackendAttrSupport;

    /// Provided, checks if type satisfies a `DiplomatBackendAttrCfg`
    fn satisfies_cfg(&self, cfg: &DiplomatBackendAttrCfg) -> bool {
        match *cfg {
            DiplomatBackendAttrCfg::Not(ref c) => !self.satisfies_cfg(c),
            DiplomatBackendAttrCfg::Any(ref cs) => cs.iter().any(|c| self.satisfies_cfg(c)),
            DiplomatBackendAttrCfg::All(ref cs) => cs.iter().all(|c| self.satisfies_cfg(c)),
            DiplomatBackendAttrCfg::Star => true,
            DiplomatBackendAttrCfg::BackendName(ref n) => self.is_backend(n),
            DiplomatBackendAttrCfg::NameValue(ref n, ref v) => self.is_name_value(n, v),
        }
    }

    // Provided, constructs an attribute
    fn attr_from_ast(
        &self,
        ast: &ast::Attrs,
        context: AttributeContext,
        errors: &mut Vec<LoweringError>,
    ) -> Attrs {
        Attrs::from_ast(ast, self, context, errors)
    }
}

/// A basic attribute validator
#[non_exhaustive]
#[derive(Default)]
pub struct BasicAttributeValidator {
    /// The primary name of this backend (should be unique, ideally)
    pub backend_name: String,
    /// The attributes supported
    pub support: BackendAttrSupport,
    /// Additional names for this backend
    pub other_backend_names: Vec<String>,
    /// override is_name_value()
    #[allow(clippy::type_complexity)] // dyn fn is not that complex
    pub is_name_value: Option<Box<dyn Fn(&str, &str) -> bool>>,
}

impl BasicAttributeValidator {
    pub fn new(backend_name: &str) -> Self {
        BasicAttributeValidator {
            backend_name: backend_name.into(),
            ..Self::default()
        }
    }
}

impl AttributeValidator for BasicAttributeValidator {
    fn primary_name(&self) -> &str {
        &self.backend_name
    }
    fn is_backend(&self, backend_name: &str) -> bool {
        self.backend_name == backend_name
            || self.other_backend_names.iter().any(|n| n == backend_name)
    }
    fn is_name_value(&self, name: &str, value: &str) -> bool {
        if let Some(ref nv) = self.is_name_value {
            nv(name, value)
        } else {
            false
        }
    }
    fn attrs_supported(&self) -> BackendAttrSupport {
        self.support
    }
}
