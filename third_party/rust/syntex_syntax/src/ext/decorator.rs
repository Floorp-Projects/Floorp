use ast::{self, Name};
use attr::{self, HasAttrs};
use fold::*;
use codemap::{ExpnInfo, MacroAttribute, NameAndSpan, respan};
use ext::base::*;
use ext::build::AstBuilder;
use parse::token::intern;
use ptr::P;
use util::small_vector::SmallVector;

pub fn expand_attributes(cx: &mut ExtCtxt, krate: ast::Crate) -> ast::Crate {
    MacroExpander { cx: cx }.fold_crate(krate)
}

struct MacroExpander<'a, 'b: 'a> {
    cx: &'a mut ExtCtxt<'b>,
}

impl<'a, 'b: 'a> Folder for MacroExpander<'a, 'b> {
    fn fold_item(&mut self, item: P<ast::Item>) -> SmallVector<P<ast::Item>> {
        let annotatable = Annotatable::Item(item);
        expand_annotatable(annotatable, self).into_iter().flat_map(|annotatable| {
            match annotatable {
                Annotatable::Item(item) => noop_fold_item(item, self),
                _ => panic!()
            }
        }).collect()
    }

    fn fold_trait_item(&mut self, item: ast::TraitItem) -> SmallVector<ast::TraitItem> {
        let annotatable = Annotatable::TraitItem(P(item));
        expand_annotatable(annotatable, self).into_iter().flat_map(|annotatable| {
            match annotatable {
                Annotatable::TraitItem(item) => noop_fold_trait_item(item.unwrap(), self),
                _ => panic!()
            }
        }).collect()
    }

    fn fold_impl_item(&mut self, item: ast::ImplItem) -> SmallVector<ast::ImplItem> {
        let annotatable = Annotatable::ImplItem(P(item));
        expand_annotatable(annotatable, self).into_iter().flat_map(|annotatable| {
            match annotatable {
                Annotatable::ImplItem(item) => noop_fold_impl_item(item.unwrap(), self),
                _ => panic!()
            }
        }).collect()
    }

    fn fold_mac(&mut self, mac: ast::Mac) -> ast::Mac {
        noop_fold_mac(mac, self)
    }
}

fn expand_annotatable(
    mut item: Annotatable,
    fld: &mut MacroExpander,
) -> SmallVector<Annotatable> {
    let mut out_items = SmallVector::zero();
    let mut new_attrs = Vec::new();

    item = expand_1(item, fld, &mut out_items, &mut new_attrs);

    item = item.map_attrs(|_| new_attrs);
    out_items.push(item);

    out_items
}

// Responsible for expanding `cfg_attr` and delegating to expand_2.
//
// The expansion turns this:
//
//     #[cfg_attr(COND1, SPEC1)]
//     #[cfg_attr(COND2, SPEC2)]
//     struct Item { ... }
//
// into this:
//
//     #[cfg(COND1)]
//     impl Trait for Item { ... }
//     #[cfg_attr(COND2, SPEC3)]
//     struct Item { ... }
//
// In the example, SPEC1 was handled by expand_2 to create the impl, and the
// handling of SPEC2 resulted in a new attribute SPEC3 which remains
// conditional.
fn expand_1(
    mut item: Annotatable,
    fld: &mut MacroExpander,
    out_items: &mut SmallVector<Annotatable>,
    new_attrs: &mut Vec<ast::Attribute>,
) -> Annotatable {
    while !item.attrs().is_empty() {
        // Pop the first attribute.
        let mut attr = None;
        item = item.map_attrs(|mut attrs| {
            attr = Some(attrs.remove(0));
            attrs
        });
        let attr = attr.unwrap();

        if let ast::MetaItemKind::List(ref word, ref vec) = attr.node.value.node {
            // #[cfg_attr(COND, SPEC)]
            if word == "cfg_attr" && vec.len() == 2 {
                if let ast::NestedMetaItemKind::MetaItem(ref spec) = vec[1].node {
                    // #[cfg(COND)]
                    let cond = fld.cx.attribute(
                        attr.span,
                        fld.cx.meta_list(
                            attr.node.value.span,
                            intern("cfg").as_str(),
                            vec[..1].to_vec()));
                    // #[SPEC]
                    let spec = fld.cx.attribute(
                        attr.span,
                        spec.clone());
                    let mut items = SmallVector::zero();
                    let mut attrs = Vec::new();
                    item = expand_2(item, &spec, fld, &mut items, &mut attrs);
                    for new_item in items {
                        let new_item = new_item.map_attrs(|mut attrs| {
                            attrs.push(cond.clone());
                            attrs
                        });
                        out_items.push(new_item);
                    }
                    for new_attr in attrs {
                        let new_spec = respan(attr.span,
                            ast::NestedMetaItemKind::MetaItem(new_attr.node.value));
                        // #[cfg_attr(COND, NEW_SPEC)]
                        let new_attr = fld.cx.attribute(
                            attr.span,
                            fld.cx.meta_list(
                                attr.node.value.span,
                                word.clone(),
                                vec![vec[0].clone(), new_spec]));
                        new_attrs.push(new_attr);
                    }
                    continue;
                }
            }
        }
        item = expand_2(item, &attr, fld, out_items, new_attrs);
    }
    item
}

// Responsible for expanding `derive` and delegating to expand_3.
//
// The expansion turns this:
//
//     #[derive(Serialize, Clone)]
//     #[other_attr]
//     struct Item { ... }
//
// into this:
//
//     impl Serialize for Item { ... }
//     #[derive(Clone)]
//     #[other_attr]
//     struct Item { ... }
//
// In the example, `derive_Serialize` was handled by expand_3 to create the impl
// but `derive_Clone` and `other_attr` were not handled. Attributes that are not
// handled by expand_3 are preserved.
fn expand_2(
    mut item: Annotatable,
    attr: &ast::Attribute,
    fld: &mut MacroExpander,
    out_items: &mut SmallVector<Annotatable>,
    new_attrs: &mut Vec<ast::Attribute>,
) -> Annotatable {
    let mname = intern(&attr.name());
    let mitem = &attr.node.value;
    if mname.as_str() == "derive" {
        let traits = mitem.meta_item_list().unwrap_or(&[]);
        if traits.is_empty() {
            fld.cx.span_warn(mitem.span, "empty trait list in `derive`");
        }
        let mut not_handled = Vec::new();
        for titem in traits.iter().rev() {
            let tname = match titem.node {
                ast::NestedMetaItemKind::MetaItem(ref inner) => {
                    match inner.node {
                        ast::MetaItemKind::Word(ref tname) => tname,
                        _ => {
                            fld.cx.span_err(titem.span, "malformed `derive` entry");
                            continue;
                        }
                    }
                }
                _ => {
                    fld.cx.span_err(titem.span, "malformed `derive` entry");
                    continue;
                }
            };
            let tname = intern(&format!("derive_{}", tname));
            // #[derive_Trait]
            let derive = fld.cx.attribute(
                attr.span,
                fld.cx.meta_word(titem.span, tname.as_str()));
            item = match expand_3(item, &derive, fld, out_items, tname) {
                Expansion::Handled(item) => item,
                Expansion::NotHandled(item) => {
                    not_handled.push((*titem).clone());
                    item
                }
            };
        }
        if !not_handled.is_empty() {
            // #[derive(Trait, ...)]
            let derive = fld.cx.attribute(
                attr.span,
                fld.cx.meta_list(mitem.span, mname.as_str(), not_handled));
            new_attrs.push(derive);
        }
        item
    } else {
        match expand_3(item, attr, fld, out_items, mname) {
            Expansion::Handled(item) => item,
            Expansion::NotHandled(item) => {
                new_attrs.push((*attr).clone());
                item
            }
        }
    }
}

enum Expansion {
    Handled(Annotatable),
    /// Here is your `Annotatable` back.
    NotHandled(Annotatable),
}

// Responsible for expanding attributes that match a MultiDecorator or
// MultiModifier registered in the syntax_env. Returns the item to continue
// processing.
//
// Syntex supports only a special case of MultiModifier - those that produce
// exactly one output. If a MultiModifier produces zero or more than one output
// this function panics. The problematic case we cannot support is:
//
//     #[decorator] // not registered with Syntex
//     #[modifier] // registered
//     struct A;
fn expand_3(
    item: Annotatable,
    attr: &ast::Attribute,
    fld: &mut MacroExpander,
    out_items: &mut SmallVector<Annotatable>,
    mname: Name,
) -> Expansion {
    let scope = fld.cx.current_expansion.mark;
    match fld.cx.resolver.find_extension(scope, mname) {
        Some(rc) => match *rc {
            MultiDecorator(ref mac) => {
                attr::mark_used(&attr);
                fld.cx.bt_push(ExpnInfo {
                    call_site: attr.span,
                    callee: NameAndSpan {
                        format: MacroAttribute(mname),
                        span: Some(attr.span),
                        // attributes can do whatever they like, for now.
                        allow_internal_unstable: true,
                    }
                });

                let mut modified = Vec::new();
                mac.expand(fld.cx, attr.span, &attr.node.value, &item,
                        &mut |item| modified.push(item));

                fld.cx.bt_pop();
                out_items.extend(modified.into_iter()
                    .flat_map(|ann| expand_annotatable(ann, fld).into_iter()));
                Expansion::Handled(item)
            }
            MultiModifier(ref mac) => {
                attr::mark_used(&attr);
                fld.cx.bt_push(ExpnInfo {
                    call_site: attr.span,
                    callee: NameAndSpan {
                        format: MacroAttribute(mname),
                        span: Some(attr.span),
                        // attributes can do whatever they like, for now.
                        allow_internal_unstable: true,
                    }
                });

                let mut modified = mac.expand(fld.cx,
                                              attr.span,
                                              &attr.node.value,
                                              item);
                if modified.len() != 1 {
                    panic!("syntex limitation: expected 1 output from `#[{}]` but got {}",
                           mname, modified.len());
                }
                let modified = modified.pop().unwrap();

                fld.cx.bt_pop();

                let mut expanded = expand_annotatable(modified, fld);
                if expanded.is_empty() {
                    panic!("syntex limitation: output of `#[{}]` must not expand further",
                           mname);
                }
                let last = expanded.pop().unwrap();

                out_items.extend(expanded);
                Expansion::Handled(last)
            }
            _ => Expansion::NotHandled(item),
        },
        _ => Expansion::NotHandled(item),
    }
}
