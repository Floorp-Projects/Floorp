use super::{Attribute, AttrStyle, Body, Crate, Ident, Item, ItemKind, MacroInput, MetaItem,
            NestedMetaItem};
use quote::Tokens;

use std::collections::BTreeMap as Map;
use std::fs::File;
use std::io::{Read, Write};
use std::path::Path;

/// Implementation of a custom derive. Custom derives take a struct or enum and
/// expand it into zero or more items, typically `impl` items.
pub trait CustomDerive {
    /// Expand the given struct or enum. If this custom derive modifies the
    /// input item or preserves it unmodified, it must be returned back in the
    /// `original` field of Expanded. The custom derive may discard the input
    /// item by setting `original` to None.
    fn expand(&self, input: MacroInput) -> Result<Expanded, String>;
}

/// Produced by expanding a custom derive.
pub struct Expanded {
    /// The items (typically `impl` items) constructed by the custom derive.
    pub new_items: Vec<Item>,
    /// The input to the custom derive, whether modified or unmodified. If the
    /// custom derive discards the input item it may do so by setting `original`
    /// to None.
    pub original: Option<MacroInput>,
}

/// Registry of custom derives. Callers add custom derives to a registry, then
/// use the registry to expand those derives in a source file.
#[derive(Default)]
pub struct Registry<'a> {
    derives: Map<String, Box<CustomDerive + 'a>>,
}

impl<T> CustomDerive for T
    where T: Fn(MacroInput) -> Result<Expanded, String>
{
    fn expand(&self, input: MacroInput) -> Result<Expanded, String> {
        self(input)
    }
}

impl<'a> Registry<'a> {
    pub fn new() -> Self {
        Default::default()
    }

    /// Register a custom derive. A `fn(MacroInput) -> Result<Expanded, String>`
    /// may be used as a custom derive.
    ///
    /// ```ignore
    /// registry.add_derive("Serialize", expand_serialize);
    /// ```
    pub fn add_derive<T>(&mut self, name: &str, derive: T)
        where T: CustomDerive + 'a
    {
        self.derives.insert(name.into(), Box::new(derive));
    }

    /// Read Rust source code from the `src` file, expand the custom derives
    /// that have been registered, and write the result to the `dst` file.
    pub fn expand_file<S, D>(&self, src: S, dst: D) -> Result<(), String>
        where S: AsRef<Path>,
              D: AsRef<Path>
    {
        // Open the src file
        let mut src = match File::open(src) {
            Ok(open) => open,
            Err(err) => return Err(err.to_string()),
        };

        // Read the contents of the src file to a String
        let mut content = String::new();
        if let Err(err) = src.read_to_string(&mut content) {
            return Err(err.to_string());
        }

        // Parse the contents
        let krate = try!(super::parse_crate(&content));

        // Expand
        let expanded = try!(expand_crate(self, krate));

        // Print the expanded code to a String
        let out = try!(pretty(quote!(#expanded)));

        // Create or truncate the dst file, opening in write-only mode
        let mut dst = match File::create(dst) {
            Ok(create) => create,
            Err(err) => return Err(err.to_string()),
        };

        // Write expanded code to the dst file
        if let Err(err) = dst.write_all(out.as_bytes()) {
            return Err(err.to_string());
        }

        Ok(())
    }
}

fn expand_crate(reg: &Registry, krate: Crate) -> Result<Crate, String> {
    let mut items = Vec::new();
    for item in krate.items {
        try!(expand_item(reg, item, Vec::new(), &mut items));
    }
    Ok(Crate { items: items, ..krate })
}

fn expand_item(reg: &Registry,
               mut item: Item,
               cfg: Vec<NestedMetaItem>,
               out: &mut Vec<Item>)
               -> Result<(), String> {
    let (body, generics) = match item.node {
        ItemKind::Enum(variants, generics) => (Body::Enum(variants), generics),
        ItemKind::Struct(variant_data, generics) => (Body::Struct(variant_data), generics),
        _ => {
            // Custom derives cannot apply to this item, preserve it unmodified
            item.attrs.extend(combine_cfgs(cfg));
            out.push(item);
            return Ok(());
        }
    };
    let macro_input = MacroInput {
        ident: item.ident,
        vis: item.vis,
        attrs: item.attrs,
        generics: generics,
        body: body,
    };
    expand_macro_input(reg, macro_input, cfg, out)
}

fn expand_macro_input(reg: &Registry,
                      mut input: MacroInput,
                      inherited_cfg: Vec<NestedMetaItem>,
                      out: &mut Vec<Item>)
                      -> Result<(), String> {
    let mut derives = Vec::new();
    let mut all_cfg = inherited_cfg;

    // Find custom derives on this item, removing them from the input
    input.attrs = input.attrs
        .into_iter()
        .flat_map(|attr| {
            let (new_derives, cfg, attr) = parse_attr(reg, attr);
            derives.extend(new_derives);
            all_cfg.extend(cfg);
            attr
        })
        .collect();

    // Expand each custom derive
    for derive in derives {
        let expanded = try!(reg.derives[derive.name.as_ref()].expand(input));

        for new_item in expanded.new_items {
            let mut extended_cfg = all_cfg.clone();
            extended_cfg.extend(derive.cfg.clone());
            try!(expand_item(reg, new_item, extended_cfg, out));
        }

        input = match expanded.original {
            Some(input) => input,
            None => return Ok(()),
        };
    }

    input.attrs.extend(combine_cfgs(all_cfg));
    out.push(input.into());
    Ok(())
}

struct Derive {
    name: Ident,
    /// If the custom derive was behind a cfg_attr
    cfg: Option<NestedMetaItem>,
}

/// Pull custom derives and cfgs out of the given Attribute.
fn parse_attr(reg: &Registry,
              attr: Attribute)
              -> (Vec<Derive>, Vec<NestedMetaItem>, Option<Attribute>) {
    if attr.style != AttrStyle::Outer || attr.is_sugared_doc {
        return (Vec::new(), Vec::new(), Some(attr));
    }

    let (name, nested) = match attr.value {
        MetaItem::List(name, nested) => (name, nested),
        _ => return (Vec::new(), Vec::new(), Some(attr)),
    };

    match name.as_ref() {
        "derive" => {
            let (derives, attr) = parse_derive_attr(reg, nested);
            let derives = derives.into_iter()
                .map(|d| {
                    Derive {
                        name: d,
                        cfg: None,
                    }
                })
                .collect();
            (derives, Vec::new(), attr)
        }
        "cfg_attr" => {
            let (derives, attr) = parse_cfg_attr(reg, nested);
            (derives, Vec::new(), attr)
        }
        "cfg" => (Vec::new(), nested, None),
        _ => {
            // Rebuild the original attribute because it was destructured above
            let attr = Attribute {
                style: AttrStyle::Outer,
                value: MetaItem::List(name, nested),
                is_sugared_doc: false,
            };
            (Vec::new(), Vec::new(), Some(attr))
        }
    }
}

/// Assuming the given nested meta-items came from a #[derive(...)] attribute,
/// pull out the ones that are custom derives.
fn parse_derive_attr(reg: &Registry,
                     nested: Vec<NestedMetaItem>)
                     -> (Vec<Ident>, Option<Attribute>) {
    let mut derives = Vec::new();

    let remaining: Vec<_> = nested.into_iter()
        .flat_map(|meta| {
            let word = match meta {
                NestedMetaItem::MetaItem(MetaItem::Word(word)) => word,
                _ => return Some(meta),
            };
            if reg.derives.contains_key(word.as_ref()) {
                derives.push(word);
                None
            } else {
                Some(NestedMetaItem::MetaItem(MetaItem::Word(word)))
            }
        })
        .collect();

    let attr = if remaining.is_empty() {
        // Elide an empty #[derive()]
        None
    } else {
        Some(Attribute {
            style: AttrStyle::Outer,
            value: MetaItem::List("derive".into(), remaining),
            is_sugared_doc: false,
        })
    };

    (derives, attr)
}

/// Assuming the given nested meta-items came from a #[cfg_attr(...)] attribute,
/// pull out any custom derives contained within.
fn parse_cfg_attr(reg: &Registry, nested: Vec<NestedMetaItem>) -> (Vec<Derive>, Option<Attribute>) {
    if nested.len() != 2 {
        let attr = Attribute {
            style: AttrStyle::Outer,
            value: MetaItem::List("cfg_attr".into(), nested),
            is_sugared_doc: false,
        };
        return (Vec::new(), Some(attr));
    }

    let mut iter = nested.into_iter();
    let cfg = iter.next().unwrap();
    let arg = iter.next().unwrap();

    let (name, nested) = match arg {
        NestedMetaItem::MetaItem(MetaItem::List(name, nested)) => (name, nested),
        _ => {
            let attr = Attribute {
                style: AttrStyle::Outer,
                value: MetaItem::List("cfg_attr".into(), vec![cfg, arg]),
                is_sugared_doc: false,
            };
            return (Vec::new(), Some(attr));
        }
    };

    if name == "derive" {
        let (derives, attr) = parse_derive_attr(reg, nested);
        let derives = derives.into_iter()
            .map(|d| {
                Derive {
                    name: d,
                    cfg: Some(cfg.clone()),
                }
            })
            .collect();
        let attr = attr.map(|attr| {
            Attribute {
                style: AttrStyle::Outer,
                value: MetaItem::List("cfg_attr".into(),
                                      vec![cfg, NestedMetaItem::MetaItem(attr.value)]),
                is_sugared_doc: false,
            }
        });
        (derives, attr)
    } else {
        let attr = Attribute {
            style: AttrStyle::Outer,
            value:
                MetaItem::List("cfg_attr".into(),
                               vec![cfg, NestedMetaItem::MetaItem(MetaItem::List(name, nested))]),
            is_sugared_doc: false,
        };
        (Vec::new(), Some(attr))
    }
}

/// Combine a list of cfg expressions into an attribute like `#[cfg(a)]` or
/// `#[cfg(all(a, b, c))]`, or nothing if there are no cfg expressions.
fn combine_cfgs(cfg: Vec<NestedMetaItem>) -> Option<Attribute> {
    // Flatten `all` cfgs so we don't nest `all` inside of `all`.
    let cfg: Vec<_> = cfg.into_iter()
        .flat_map(|cfg| {
            let (name, nested) = match cfg {
                NestedMetaItem::MetaItem(MetaItem::List(name, nested)) => (name, nested),
                _ => return vec![cfg],
            };
            if name == "all" {
                nested
            } else {
                vec![NestedMetaItem::MetaItem(MetaItem::List(name, nested))]
            }
        })
        .collect();

    let value = match cfg.len() {
        0 => return None,
        1 => cfg,
        _ => vec![NestedMetaItem::MetaItem(MetaItem::List("all".into(), cfg))],
    };

    Some(Attribute {
        style: AttrStyle::Outer,
        value: MetaItem::List("cfg".into(), value),
        is_sugared_doc: false,
    })
}

#[cfg(not(feature = "pretty"))]
fn pretty(tokens: Tokens) -> Result<String, String> {
    Ok(tokens.to_string())
}

#[cfg(feature = "pretty")]
fn pretty(tokens: Tokens) -> Result<String, String> {
    use syntax::parse::{self, ParseSess};
    use syntax::print::pprust;

    let name = "syn".to_string();
    let source = tokens.to_string();
    let sess = ParseSess::new();
    let krate = match parse::parse_crate_from_source_str(name, source, &sess) {
        Ok(krate) => krate,
        Err(mut err) => {
            err.emit();
            return Err("pretty printer failed to parse expanded code".into());
        }
    };

    if sess.span_diagnostic.has_errors() {
        return Err("pretty printer failed to parse expanded code".into());
    }

    let mut reader = &tokens.to_string().into_bytes()[..];
    let mut writer = Vec::new();
    let ann = pprust::NoAnn;

    try!(pprust::print_crate(
        sess.codemap(),
        &sess.span_diagnostic,
        &krate,
        "".to_string(),
        &mut reader,
        Box::new(&mut writer),
        &ann,
        false).map_err(|err| err.to_string()));

    String::from_utf8(writer).map_err(|err| err.to_string())
}
