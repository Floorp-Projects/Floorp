//! # Why?
//!
//! Custom derives commonly use [attributes] to customize the behavior of
//! generated code. For example to control the name of a field when serialized
//! as JSON:
//!
//! [attributes]: https://doc.rust-lang.org/book/attributes.html
//!
//! ```ignore
//! #[derive(Serialize, Deserialize)]
//! struct Person {
//!     #[serde(rename = "firstName")]
//!     first_name: String,
//!     #[serde(rename = "lastName")]
//!     last_name: String,
//! }
//! ```
//!
//! In the old compiler plugin infrastructure, plugins were provided with a
//! mechanism to mark attributes as "used" so the compiler would know to ignore
//! them after running the plugin. The new Macros 1.1 infrastructure is
//! deliberately minimal and does not provide this mechanism. Instead, proc
//! macros are expected to strip away attributes after using them. Any
//! unrecognized attributes that remain after proc macro expansion are turned
//! into errors.
//!
//! This approach causes problems when multiple custom derives want to process
//! the same attributes. For example multiple crates (for JSON, Postgres, and
//! Elasticsearch code generation) may want to standardize on a common rename
//! attribute. If each custom derive is stripping away attributes after using
//! them, subsequent custom derives on the same struct will not see the
//! attributes they should.
//!
//! This crate provides a way to strip attributes (and possibly other cleanup
//! tasks in the future) in a post-expansion pass that happens after other
//! custom derives have been run.
//!
//! # How?
//!
//! Suppose `#[derive(ElasticType)]` wants to piggy-back on Serde's `rename`
//! attributes for types that are serializable by both Serde and Elasticsearch:
//!
//! ```ignore
//! #[derive(Serialize, Deserialize, ElasticType)]
//! struct Point {
//!     #[serde(rename = "xCoord")]
//!     x: f64,
//!     #[serde(rename = "yCoord")]
//!     y: f64,
//! }
//! ```
//!
//! A workable but poor solution would be to have Serde's code generation know
//! that ElasticType expects to read the same attributes, so it should not strip
//! attributes when ElasticType is present in the list of derives. An ideal
//! solution would not require Serde's code generation to know anything about
//! other custom derives.
//!
//! We can handle this by having the Serialize and Deserialize derives register
//! a post-expansion pass to strip the attributes after all other custom derives
//! have been executed. Serde should expand the above code into:
//!
//! ```ignore
//! impl Serialize for Point { /* ... */ }
//! impl Deserialize for Point { /* ... */ }
//!
//! #[derive(ElasticType)]
//! #[derive(PostExpansion)] // insert a post-expansion pass after all other derives
//! #[post_expansion(strip = "serde")] // during post-expansion, strip "serde" attributes
//! struct Point {
//!     #[serde(rename = "xCoord")]
//!     x: f64,
//!     #[serde(rename = "yCoord")]
//!     y: f64,
//! }
//! ```
//!
//! Now the ElasticType custom derive can run and see all the right attributes.
//!
//! ```ignore
//! impl Serialize for Point { /* ... */ }
//! impl Deserialize for Point { /* ... */ }
//! impl ElasticType for Point { /* ... */ }
//!
//! #[derive(PostExpansion)]
//! #[post_expansion(strip = "serde")]
//! struct Point {
//!     #[serde(rename = "xCoord")]
//!     x: f64,
//!     #[serde(rename = "yCoord")]
//!     y: f64,
//! }
//! ```
//!
//! Once all other derives have been expanded the `PostExpansion` pass strips
//! the attributes.
//!
//! ```ignore
//! impl Serialize for Point { /* ... */ }
//! impl Deserialize for Point { /* ... */ }
//! impl ElasticType for Point { /* ... */ }
//!
//! struct Point {
//!     x: f64,
//!     y: f64,
//! }
//! ```
//!
//! There are some complications beyond what is shown in the example. For one,
//! ElasticType needs to register its own post-expansion pass in case somebody
//! does `#[derive(ElasticType, Serialize)]`. The post-expansion passes from
//! Serde and ElasticType cannot both be called "PostExpansion" because that
//! would be a conflict.
//!
//! There are also performance considerations. Stripping attributes in a
//! post-expansion pass requires an extra round trip of syn -> tokenstream ->
//! libsyntax -> tokenstream -> syn, which can be avoided if the current custom
//! derive knows that it is the last custom derive.
//!
//! This crate provides helpers to make the whole process **easy, correct, and
//! performant**.
//!
//! # How Exactly?
//!
//! There are two pieces. Proc macros that process attributes need to register a
//! post-expansion pass using the `register_post_expansion!` macro. During
//! expansion, they need to wire up the custom derive corresponding to the
//! post-expansion pass.
//!
//! ```ignore
//! extern crate syn;
//! #[macro_use]
//! extern crate post_expansion;
//!
//! register_post_expansion!(PostExpansion_my_macro);
//!
//! #[proc_macro_derive(MyMacro)]
//! pub fn my_macro(input: TokenStream) -> TokenStream {
//!     let source = input.to_string();
//!     let ast = syn::parse_macro_input(&source).unwrap();
//!
//!     let derived_impl = expand_my_macro(&ast);
//!
//!     let stripped = post_expansion::strip_attrs_later(ast, &["my_attr"], "my_macro");
//!
//!     let tokens = quote! {
//!         #stripped
//!         #derived_impl
//!     };
//!
//!     tokens.to_string().parse().unwrap()
//! }
//! ```

extern crate syn;
use syn::*;

#[macro_use]
extern crate quote;

/// Register a Macros 1.1 custom derive mode corresponding to a post-expansion
/// pass for the current crate. Must be called at the top level of a proc-macro
/// crate. Must be called with an identifier for the post-expansion pass, which
/// must be "PostExpansion_" + unique identifier. Recommended to use the crate
/// name of the current crate as the unique identifier.
///
/// Only needed if using `strip_attrs_later`. The identifier passed to
/// `strip_attrs_later` must match the unique identifier here.
///
/// ```ignore
/// register_post_expansion!(PostExpansion_elastic_types);
/// ```
#[macro_export]
macro_rules! register_post_expansion {
    ($id:ident) => {
        #[proc_macro_derive($id)]
        pub fn post_expansion(input: ::proc_macro::TokenStream) -> ::proc_macro::TokenStream {
            let source = input.to_string();
            let clean = ::post_expansion::run(source);
            clean.parse().unwrap()
        }
    };
}

/// Run post-expansion pass. This is called from the `register_post_expansion!`
/// macro and should not need to be called directly.
///
/// The post-expansion pass strips any remaining `PostExpansion` derives and
/// strips the attributes that were queued to be stripped "later".
///
/// ```ignore
/// #[proc_macro_derive(PostExpansion_elastic_types)]
/// pub fn post_expansion(input: TokenStream) -> TokenStream {
///     let source = input.to_string();
///     let clean = post_expansion::run(source);
///     clean.parse().unwrap()
/// }
/// ```
pub fn run(input: String) -> String {
    let ast = parse_macro_input(&input).unwrap();
    let clean = post_expansion(ast);
    quote!(#clean).to_string()
}

// Called by `run`, but also by `strip_attrs_later` in the fast path case where
// it determines it is safe to strip attrs now rather than later.
fn post_expansion(mut ast: MacroInput) -> MacroInput {
    // Attributes to be stripped "later"; these come from attributes that look
    // like `#[post_expansion(strip = "serde")]`
    let mut strip = Vec::new();

    // Strip any remaining PostExpansion derives and collect the set of
    // attributes to strip
    ast.attrs = ast.attrs
        .into_iter()
        .filter_map(|attr| {
            if attr.is_sugared_doc || attr.style != AttrStyle::Outer {
                // Neither `#[derive(...)]` nor `#[post_expansion(...)]`
                return Some(attr);
            }
            let (name, nested) = match attr.value {
                MetaItem::List(name, nested) => (name, nested),
                _ => {
                    // Neither `#[derive(...)]` nor `#[post_expansion(...)]`
                    return Some(attr);
                }
            };
            match name.as_ref() {
                "post_expansion" => {
                    for nested in nested {
                        // Look for `#[post_expansion(strip = "...")]`
                        if let MetaItem::NameValue(name, Lit::Str(attr, _)) = nested {
                            if name == "strip" && !strip.contains(&attr) {
                                strip.push(attr);
                            }
                        }
                    }
                    // Drop the `#[post_expansion(...)]` attribute
                    None
                }
                "derive" => {
                    // Drop any other `derive(PostExpansion_xyz)` derives
                    let rest: Vec<_> = nested.into_iter()
                        .filter(|nested| {
                            match *nested {
                                MetaItem::Word(ref word) => {
                                    !word.as_ref().starts_with("PostExpansion_")
                                }
                                _ => true,
                            }
                        })
                        .collect();
                    if rest.is_empty() {
                        // Omit empty `#[derive()]`
                        None
                    } else {
                        // At least one remaining derive
                        Some(Attribute {
                            style: AttrStyle::Outer,
                            value: MetaItem::List(name, rest),
                            is_sugared_doc: false,
                        })
                    }
                }
                _ => {
                    // Neither `#[derive(...)]` nor `#[post_expansion(...)]` but
                    // the original attribute has been destructured so it needs
                    // to be put back together
                    Some(Attribute {
                        style: AttrStyle::Outer,
                        value: MetaItem::List(name, nested),
                        is_sugared_doc: false,
                    })
                }
            }
        })
        .collect();

    strip_attrs_now(ast, &strip.iter().map(AsRef::as_ref).collect::<Vec<_>>())
}

/// Strip the specified attributes from the AST. Note that if it is possible
/// those attributes may be needed by other custom derives, you should use
/// `strip_attrs_later` instead which strips the attributes only after other
/// custom derives have had a chance to see them.
///
/// ```ignore
/// ast = post_expansion::strip_attrs_now(ast, &["serde"]);
/// ```
pub fn strip_attrs_now(ast: MacroInput, strip: &[&str]) -> MacroInput {
    if strip.is_empty() {
        return ast;
    }

    return MacroInput {
        attrs: strip_attrs(ast.attrs, strip),
        body: match ast.body {
            Body::Enum(variants) => {
                Body::Enum(variants.into_iter()
                    .map(|variant| {
                        Variant {
                            attrs: strip_attrs(variant.attrs, strip),
                            data: strip_variant_data(variant.data, strip),
                            ..variant
                        }
                    })
                    .collect())
            }
            Body::Struct(variant_data) => Body::Struct(strip_variant_data(variant_data, strip)),
        },
        ..ast
    };

    fn strip_variant_data(data: VariantData, strip: &[&str]) -> VariantData {
        match data {
            VariantData::Struct(fields) => {
                VariantData::Struct(fields.into_iter()
                    .map(|field| strip_field(field, strip))
                    .collect())
            }
            VariantData::Tuple(fields) => {
                VariantData::Tuple(fields.into_iter()
                    .map(|field| strip_field(field, strip))
                    .collect())
            }
            VariantData::Unit => VariantData::Unit,
        }
    }

    fn strip_field(field: Field, strip: &[&str]) -> Field {
        Field { attrs: strip_attrs(field.attrs, strip), ..field }
    }

    fn strip_attrs(attrs: Vec<Attribute>, strip: &[&str]) -> Vec<Attribute> {
        attrs.into_iter()
            .filter(|attr| {
                match attr.value {
                    MetaItem::List(ref ident, _) => !strip.into_iter().any(|n| ident == n),
                    _ => true,
                }
            })
            .collect()
    }
}

/// Set up a post-expansion pass to strip the given attributes after other
/// custom derives have had a chance to see them. Must be used together with the
/// `register_post_expansion!` macro. The third argument is the unique
/// identifier from the invocation of `register_post_expansion!`. Recommended to
/// use the crate name of the current crate as the unique identifier.
///
/// ```ignore
/// let ast = post_expansion::strip_attrs_later(ast, &["serde", "elastic"], "elastic_types");
/// ```
pub fn strip_attrs_later(mut ast: MacroInput, strip: &[&str], identifier: &str) -> MacroInput {
    if strip.is_empty() {
        return ast;
    }

    let run_now = can_run_post_expansion_now(&ast);

    // #[derive(PostExpansion_$identifier)]
    ast.attrs.push(Attribute {
        style: AttrStyle::Outer,
        value: MetaItem::List("derive".into(),
                              vec![MetaItem::Word(format!("PostExpansion_{}", identifier).into())]),
        is_sugared_doc: false,
    });
    // #[post_expansion(strip = "a", strip = "b")]
    ast.attrs.push(Attribute {
        style: AttrStyle::Outer,
        value: MetaItem::List("post_expansion".into(),
                              strip.into_iter()
                                  .map(|&n| {
                                      MetaItem::NameValue("strip".into(),
                                                          Lit::Str(n.into(), StrStyle::Cooked))
                                  })
                                  .collect()),
        is_sugared_doc: false,
    });

    if run_now { post_expansion(ast) } else { ast }
}

/// Can run now if the only remaining derives are builtin derives which do not
/// look at attributes, or other `PostExpansion` passes.
fn can_run_post_expansion_now(ast: &MacroInput) -> bool {
    for attr in &ast.attrs {
        if attr.is_sugared_doc || attr.style != AttrStyle::Outer {
            // Not a `derive` attribute
            continue;
        }
        let list = match attr.value {
            MetaItem::List(ref name, ref list) if name == "derive" => list,
            _ => {
                // Not a `derive` attribute
                continue;
            }
        };
        for elem in list {
            let word = match *elem {
                MetaItem::Word(ref word) => word,
                _ => {
                    // Unexpected thing inside `derive(...)`, take the safe path
                    // by not running post-expansion now
                    return false;
                }
            };
            match word.as_ref() {
                "Clone" | "Hash" | "RustcEncodable" | "RustcDecodable" | "PartialEq" | "Eq" |
                "PartialOrd" | "Ord" | "Debug" | "Default" | "Send" | "Sync" | "Copy" |
                "Encodable" | "Decodable" => {
                    // The built-in derives do not look at attributes so
                    // shouldn't stop us from stripping attributes now
                    continue;
                }
                custom if custom.starts_with("PostExpansion_") => {
                    // Okay to run now if the only other custom derives are
                    // PostExpansion
                    continue;
                }
                _ => {
                    // Cannot run post-expansion now because there is a custom
                    // derive that may need the same attributes
                    return false;
                }
            }
        }
    }
    // The only remaining derives are builtin derives or other PostExpansion
    // passes; nothing more will need the attributes so strip them now
    true
}
