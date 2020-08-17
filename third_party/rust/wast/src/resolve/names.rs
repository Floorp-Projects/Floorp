use crate::ast::*;
use crate::resolve::gensym;
use crate::resolve::Ns;
use crate::Error;
use std::collections::HashMap;
use std::mem;

pub fn resolve<'a>(fields: &mut Vec<ModuleField<'a>>) -> Result<Resolver<'a>, Error> {
    let mut resolver = Resolver::default();
    resolver.process(None, fields)?;
    Ok(resolver)
}

/// Context structure used to perform name resolution.
#[derive(Default)]
pub struct Resolver<'a> {
    modules: Vec<Module<'a>>,
    /// Index of the module currently being worked on.
    cur: usize,
}

#[derive(Default)]
pub struct Module<'a> {
    /// The index of this module in `resolver.modules`.
    id: usize,

    /// The parent index of this module, if present.
    parent: Option<usize>,

    /// Whether or not names have been registered in the namespaces below, used
    /// in `key_to_idx` when we're adding new type fields.
    registered: bool,

    // Namespaces within each module. Note that each namespace carries with it
    // information about the signature of the item in that namespace. The
    // signature is later used to synthesize the type of a module and inject
    // type annotations if necessary.
    funcs: Namespace<'a, Index<'a>>,
    globals: Namespace<'a, GlobalType<'a>>,
    tables: Namespace<'a, TableType<'a>>,
    memories: Namespace<'a, MemoryType>,
    types: Namespace<'a, TypeInfo<'a>>,
    events: Namespace<'a, Index<'a>>,
    modules: Namespace<'a, ModuleInfo<'a>>,
    instances: Namespace<'a, InstanceDef<'a>>,
    datas: Namespace<'a, ()>,
    elems: Namespace<'a, ()>,
    fields: Namespace<'a, ()>,

    /// Information about the exports of this module.
    exports: ExportNamespace<'a>,
    /// List of exported items in this module, along with what kind of item is
    /// exported.
    exported_ids: Vec<ExportKind<'a>>,

    // Maps used to "intern" types. These maps are populated as type annotations
    // are seen and inline type annotations use previously defined ones if
    // there's a match.
    func_type_to_idx: HashMap<FuncKey<'a>, Index<'a>>,
    instance_type_to_idx: HashMap<InstanceKey<'a>, Index<'a>>,
    module_type_to_idx: HashMap<ModuleKey<'a>, Index<'a>>,

    /// Fields, during processing, which should be prepended to the
    /// currently-being-processed field. This should always be empty after
    /// processing is complete.
    to_prepend: Vec<ModuleField<'a>>,
}

enum InstanceDef<'a> {
    /// An instance definition that was imported.
    Import {
        /// An index pointing to the type of the instance.
        type_idx: Index<'a>,
        /// Information about the exports of the instance.
        exports: ExportNamespace<'a>,
    },
    /// An instance definition that was the instantiation of a module.
    Instantiate {
        /// The module that is instantiated to create this instance
        module_idx: Index<'a>,
    },
}

enum ModuleInfo<'a> {
    /// An imported module with inline information about the exports.
    Imported {
        /// Information about the exports of this module, inferred from the type.
        exports: ExportNamespace<'a>,
        /// The type of the imported module.
        type_idx: Index<'a>,
    },
    Nested {
        /// The index in `resolver.modules` of where this module resides. It
        /// will need to be consulted for export information.
        idx: usize,
        /// The type of the module.
        type_idx: Index<'a>,
    },
}

impl<'a> Resolver<'a> {
    fn process(
        &mut self,
        parent: Option<usize>,
        fields: &mut Vec<ModuleField<'a>>,
    ) -> Result<(), Error> {
        // First up create a new `Module` structure and add it to our list of
        // modules. Afterwards configure some internal fields about it as well.
        assert_eq!(self.modules.len(), self.cur);
        self.modules.push(Module::default());
        let module = &mut self.modules[self.cur];
        module.id = self.cur;
        module.parent = parent;

        // The first thing we do is expand `type` declarations and their
        // specified-inline types. This will expand inline instance/module types
        // and add more type declarations as necessary. This happens early on
        // since it's pretty easy to do and it also will add more types to the
        // index space, which we'll want to stabilize soon.
        self.iter_and_prepend(
            fields,
            |me, field| Ok(me.modules[me.cur].expand_type(field)),
        )?;

        // FIXME: this is a bad hack and is something we should not do. This is
        // not forwards-compatible with type imports since we're reordering the
        // text file.
        //
        // The purpose of this is:
        //
        // * In `me.expand(...)` below we'll be creating more types. This is
        //   for `TypeUse` on items other than type declarations (e.g. inline
        //   functions) as well as inline module types. Especially for inline
        //   modules we don't know how many types we'll be manufacturing for
        //   each item.
        //
        // * The injected types come *after* we've numbered the existing types
        //   in the module. This means that they need to come last in the index
        //   space. Somehow the binary emission needs to be coordinated with
        //   all this where the emitted types get emitted last.
        //
        // * However, according to the current module linking spec, modules
        //   can be interleaved with the type section. This means that we can't
        //   rely on types being declared before the module section. The module
        //   section can only refer to previously defined types as well.
        //
        // Practically there is no purpose to interleaving the type and module
        // section today. As a result we can safely sort all types to the front.
        // This, however, can break the roundtrip binary-text-binary for
        // strictly-speaking compliant modules with the module linking spec.
        // Anyway, this is a bummer, should figure out a better thing in the
        // future.
        //
        // I've tried to open discussion about this at
        // WebAssembly/module-linking#8
        fields.sort_by_key(|field| match field {
            ModuleField::Type(_)
            | ModuleField::Alias(Alias {
                kind: ExportKind::Type(_),
                ..
            }) => 0,
            _ => 1,
        });

        // Number everything in the module, recording what names correspond to
        // what indices.
        let module = &mut self.modules[self.cur];
        for field in fields.iter_mut() {
            module.register(field)?;
        }
        module.registered = true;

        // After we've got our index spaces we then need to expand inline type
        // annotations on everything other than `type` definitions.
        //
        // This is the key location we start recursively processing nested
        // modules as well. Along the way of expansion we record the signature
        // of all items. This way nested modules have access to all names
        // defined in parent modules, but they only have access to type
        // signatures of previously defined items.
        self.iter_and_prepend(fields, |me, field| me.expand(field))?;

        // This is the same as the comment above, only we're doing it now after
        // the full expansion process since all types should now be present in
        // the module.
        fields.sort_by_key(|field| match field {
            ModuleField::Type(_)
            | ModuleField::Alias(Alias {
                kind: ExportKind::Type(_),
                ..
            }) => 0,
            _ => 1,
        });

        // And finally the last step is to replace all our `Index::Id` instances
        // with `Index::Num` in the AST. This does not recurse into nested
        // modules because that was handled in `expand` above.
        let module = &mut self.modules[self.cur];
        for field in fields.iter_mut() {
            module.resolve_field(field)?;
        }
        Ok(())
    }

    pub fn module(&self) -> &Module<'a> {
        &self.modules[self.cur]
    }

    fn iter_and_prepend(
        &mut self,
        fields: &mut Vec<ModuleField<'a>>,
        cb: impl Fn(&mut Self, &mut ModuleField<'a>) -> Result<(), Error>,
    ) -> Result<(), Error> {
        let module = &self.modules[self.cur];
        assert!(module.to_prepend.is_empty());
        let mut cur = 0;
        // The name on the tin of this method is that we'll be *prepending*
        // items in the `to_prepend` list. The only items we're prepending are
        // types, however. In the module-linking proposal we really need to
        // prepend types because the previous item (like the definition of a
        // module) might refer back to the type and the type must precede the
        // reference. If we don't have nested modules, however, it's fine to
        // throw everything at the end.
        //
        // WABT throws everything at the end (expansion of inline type
        // annotations from functions/etc), and we want to be binary-compatible
        // with that. With all of that in mind we only prepend if some
        // conditions are true:
        //
        // * If names have *not* been registered, then we're expanding types. If
        //   a type is expanded then it's an instance/module type, so we must
        //   prepend.
        //
        // * If `self.cur` is not zero, then we're in a nested module, so we're
        //   in the module linking proposal.
        //
        // * If we have any instances/modules then we're part of the module
        //   linking proposal, so we prepend.
        //
        // Basically if it looks like we're an "MVP-compatible" module we throw
        // types at the end. Otherwise if it looks like module-linking is being
        // used we prepend items. This will likely change as other tools
        // implement module linking.
        let actually_prepend = !module.registered
            || self.cur != 0
            || module.instances.count > 0
            || module.modules.count > 0;
        while cur < fields.len() {
            cb(self, &mut fields[cur])?;
            if actually_prepend {
                for item in self.modules[self.cur].to_prepend.drain(..) {
                    fields.insert(cur, item);
                    cur += 1;
                }
            }
            cur += 1;
        }
        if !actually_prepend {
            fields.extend(self.modules[self.cur].to_prepend.drain(..));
        }
        assert!(self.modules[self.cur].to_prepend.is_empty());
        Ok(())
    }

    fn expand(&mut self, item: &mut ModuleField<'a>) -> Result<(), Error> {
        let module = &mut self.modules[self.cur];
        match item {
            // expansion generally handled above, we're just handling the
            // signature of each item here
            ModuleField::Type(ty) => {
                let info = match &ty.def {
                    TypeDef::Func(f) => TypeInfo::Func(f.key()),
                    TypeDef::Instance(i) => TypeInfo::Instance {
                        info: Module::from_exports(ty.span, self.cur, &i.exports)?,
                        key: i.key(),
                    },
                    TypeDef::Module(m) => TypeInfo::Module {
                        info: Module::from_exports(ty.span, self.cur, &m.exports)?,
                        key: m.key(),
                    },
                    TypeDef::Array(_) | TypeDef::Struct(_) => TypeInfo::Other,
                };
                module.types.push_item(self.cur, info);
            }

            ModuleField::Import(i) => {
                module.expand_item_sig(&mut i.item, false);

                // Record each item's type information in the index space
                // arrays.
                match &mut i.item.kind {
                    ItemKind::Func(t) => module.funcs.push_item(self.cur, t.index.unwrap()),
                    ItemKind::Event(EventType::Exception(t)) => {
                        module.events.push_item(self.cur, t.index.unwrap())
                    }
                    ItemKind::Instance(t) => {
                        let exports = match t.inline.take() {
                            Some(i) => ExportNamespace::from_exports(&i.exports)?,
                            None => ExportNamespace::default(),
                        };
                        module.instances.push_item(
                            self.cur,
                            InstanceDef::Import {
                                type_idx: t.index.unwrap(),
                                exports,
                            },
                        );
                    }
                    ItemKind::Module(m) => {
                        let exports = match m.inline.take() {
                            Some(m) => ExportNamespace::from_exports(&m.exports)?,
                            None => ExportNamespace::default(),
                        };
                        module.modules.push_item(
                            self.cur,
                            ModuleInfo::Imported {
                                type_idx: m.index.unwrap(),
                                exports,
                            },
                        );
                    }
                    ItemKind::Global(g) => module.globals.push_item(self.cur, g.clone()),
                    ItemKind::Table(t) => module.tables.push_item(self.cur, t.clone()),
                    ItemKind::Memory(m) => module.memories.push_item(self.cur, m.clone()),
                }
            }
            ModuleField::Func(f) => {
                let idx = module.expand_type_use(&mut f.ty);
                module.funcs.push_item(self.cur, idx);
                if let FuncKind::Inline { expression, .. } = &mut f.kind {
                    module.expand_expression(expression);
                }
            }
            ModuleField::Global(g) => {
                module.globals.push_item(self.cur, g.ty);
                if let GlobalKind::Inline(expr) = &mut g.kind {
                    module.expand_expression(expr);
                }
            }
            ModuleField::Data(d) => {
                if let DataKind::Active { offset, .. } = &mut d.kind {
                    module.expand_expression(offset);
                }
            }
            ModuleField::Elem(e) => {
                if let ElemKind::Active { offset, .. } = &mut e.kind {
                    module.expand_expression(offset);
                }
            }
            ModuleField::Event(e) => match &mut e.ty {
                EventType::Exception(ty) => {
                    let idx = module.expand_type_use(ty);
                    module.events.push_item(self.cur, idx);
                }
            },
            ModuleField::Instance(i) => {
                if let InstanceKind::Inline {
                    module: module_idx, ..
                } = i.kind
                {
                    module
                        .instances
                        .push_item(self.cur, InstanceDef::Instantiate { module_idx });
                }
            }
            ModuleField::NestedModule(m) => {
                let (fields, ty) = match &mut m.kind {
                    NestedModuleKind::Inline { fields, ty } => (fields, ty),
                    NestedModuleKind::Import { .. } => panic!("should only be inline"),
                };

                // And this is where the magic happens for recursive resolution
                // of modules. Here we switch `self.cur` to a new module that
                // will be injected (the length of the array since a new element
                // is pushed). We then recurse and process, in its entirety, the
                // nested module.
                //
                // Our nested module will have access to all names defined in
                // our module, but it will only have access to item signatures
                // of previously defined items.
                //
                // After this is all done if the inline type annotation is
                // missing then we can infer the type of the inline module and
                // inject the type here.
                let prev = mem::replace(&mut self.cur, self.modules.len());
                self.process(Some(prev), fields)?;
                let child = mem::replace(&mut self.cur, prev);
                if ty.is_none() {
                    *ty = Some(self.infer_module_ty(m.span, child, fields)?);
                }
                self.modules[self.cur].modules.push_item(
                    self.cur,
                    ModuleInfo::Nested {
                        type_idx: ty.unwrap(),
                        idx: child,
                    },
                );
            }
            ModuleField::Table(t) => {
                let ty = match t.kind {
                    TableKind::Normal(ty) => ty,
                    _ => panic!("tables should be normalized by now"),
                };
                module.tables.push_item(self.cur, ty.clone());
            }
            ModuleField::Memory(t) => {
                let ty = match t.kind {
                    MemoryKind::Normal(ty) => ty,
                    _ => panic!("memories should be normalized by now"),
                };
                module.memories.push_item(self.cur, ty.clone());
            }
            ModuleField::Alias(a) => self.expand_alias(a)?,
            ModuleField::Start(_) | ModuleField::Export(_) | ModuleField::Custom(_) => {}
            ModuleField::ExportAll(..) => unreachable!(),
        }
        return Ok(());
    }

    fn expand_alias(&mut self, a: &mut Alias<'a>) -> Result<(), Error> {
        // This is a pretty awful method. I'd like to preemptively apologize for
        // how much of a mess this is. Not only is what this doing inherently
        // complicated but it's also complicated by working around
        // Rust-the-language as well.
        //
        // The general idea is that we're taking a look at where this alias is
        // coming from (nested instance or parent) and we manufacture an
        // appropriate closure which, when invoked, creates a `SigAlias`
        // appropriate for this signature.
        //
        // Cleanups are very welcome to this function if you've got an idea, I
        // don't think anyone will be wed to it in its current form.
        //
        // As some more information on this method, though, the goal of this
        // method is to expand the `alias` directive and fill in all the names
        // here. Note that we do this eagerly because we want to fill in the
        // type signature of the item this alias is introducing. We don't want
        // to fill in the *full* type signature, though, just that it was an
        // alias. The full signature may not yet be available in some valid
        // situations, so we record the bare minimum necessary that an 'alias'
        // was used, and then later on if that alias attempts to get followed
        // (e.g. in `item_for`) we'll do the checks to make sure everything
        // lines up.
        let gen1;
        let gen2;
        let gen3;
        let gen4;
        let gen_alias: &dyn Fn(&mut Index<'a>, Ns) -> Result<SigAlias<'a>, Error> =
            match &mut a.instance {
                Some(i) => {
                    self.modules[self.cur].resolve(i, Ns::Instance)?;
                    let (def, m) = self.instance_for(self.cur, i)?;
                    match def {
                        InstanceDef::Import { type_idx, exports } => {
                            gen1 = move |idx: &mut Index<'a>, ns: Ns| {
                                Ok(SigAlias::TypeExport {
                                    module: m,
                                    ty: *type_idx,
                                    export_idx: exports.resolve(idx, ns)?,
                                })
                            };
                            &gen1
                        }
                        InstanceDef::Instantiate { module_idx } => {
                            let (info, m) = self.module_for(m, module_idx)?;
                            match info {
                                ModuleInfo::Imported { type_idx, exports } => {
                                    gen2 = move |idx: &mut Index<'a>, ns: Ns| {
                                        Ok(SigAlias::TypeExport {
                                            module: m,
                                            ty: *type_idx,
                                            export_idx: exports.resolve(idx, ns)?,
                                        })
                                    };
                                    &gen2
                                }
                                ModuleInfo::Nested {
                                    idx: module_idx, ..
                                } => {
                                    let module = &self.modules[*module_idx];
                                    gen3 = move |idx: &mut Index<'a>, ns: Ns| {
                                        Ok(SigAlias::ModuleExport {
                                            module: *module_idx,
                                            export_idx: module.exports.resolve(idx, ns)?,
                                        })
                                    };
                                    &gen3
                                }
                            }
                        }
                    }
                }
                None => {
                    let module_idx = match self.modules[self.cur].parent {
                        Some(p) => p,
                        None => {
                            return Err(Error::new(
                                a.span,
                                "cannot use alias parent directives in root module".to_string(),
                            ))
                        }
                    };
                    let module = &self.modules[module_idx];
                    gen4 = move |idx: &mut Index<'a>, ns: Ns| {
                        module.resolve(idx, ns)?;
                        Ok(SigAlias::Item {
                            module: module_idx,
                            idx: *idx,
                        })
                    };
                    &gen4
                }
            };
        match &mut a.kind {
            ExportKind::Func(idx) => {
                let item = gen_alias(idx, Ns::Func)?;
                self.modules[self.cur].funcs.push(Sig::Alias(item));
            }
            ExportKind::Table(idx) => {
                let item = gen_alias(idx, Ns::Table)?;
                self.modules[self.cur].tables.push(Sig::Alias(item));
            }
            ExportKind::Global(idx) => {
                let item = gen_alias(idx, Ns::Global)?;
                self.modules[self.cur].globals.push(Sig::Alias(item));
            }
            ExportKind::Memory(idx) => {
                let item = gen_alias(idx, Ns::Memory)?;
                self.modules[self.cur].memories.push(Sig::Alias(item));
            }
            ExportKind::Event(idx) => {
                let item = gen_alias(idx, Ns::Event)?;
                self.modules[self.cur].events.push(Sig::Alias(item));
            }
            ExportKind::Type(idx) => {
                let item = gen_alias(idx, Ns::Type)?;
                self.modules[self.cur].types.push(Sig::Alias(item));
            }
            ExportKind::Instance(idx) => {
                let item = gen_alias(idx, Ns::Instance)?;
                self.modules[self.cur].instances.push(Sig::Alias(item));
            }
            ExportKind::Module(idx) => {
                let item = gen_alias(idx, Ns::Module)?;
                self.modules[self.cur].modules.push(Sig::Alias(item));
            }
        }
        Ok(())
    }

    /// Creates a local type definition that matches the signature of the
    /// `child` module, which is defined by `fields`.
    ///
    /// The goal here is that we're injecting type definitions into the current
    /// module to create a `module` type that matches the module provided, and
    /// then we'll use that new type as the type of the module itself.
    fn infer_module_ty(
        &mut self,
        span: Span,
        child: usize,
        fields: &[ModuleField<'a>],
    ) -> Result<Index<'a>, Error> {
        let mut imports = Vec::new();
        let mut exports = Vec::new();

        for field in fields {
            match field {
                // For each import it already has the type signature listed
                // inline, so we use that and then copy out the signature from
                // the module to our module.
                ModuleField::Import(i) => {
                    let item = match &i.item.kind {
                        ItemKind::Func(f) => Item::Func(f.index.unwrap()),
                        ItemKind::Module(f) => Item::Module(f.index.unwrap()),
                        ItemKind::Instance(f) => Item::Instance(f.index.unwrap()),
                        ItemKind::Event(EventType::Exception(f)) => Item::Event(f.index.unwrap()),
                        ItemKind::Table(t) => Item::Table(t.clone()),
                        ItemKind::Memory(t) => Item::Memory(t.clone()),
                        ItemKind::Global(t) => Item::Global(t.clone()),
                    };
                    let item = self.copy_item_from_module(i.span, child, &item)?;
                    imports.push((i.module, i.field, item));
                }

                // Exports are a little more complicated, we have to use the
                // `*_for` family of functions to follow the trail of
                // definitions to figure out the signature of the item referred
                // to by the export.
                ModuleField::Export(e) => {
                    let (module, item) = match &e.kind {
                        ExportKind::Func(i) => {
                            let (type_idx, m) = self.func_for(child, i)?;
                            (m, Item::Func(*type_idx))
                        }
                        ExportKind::Event(i) => {
                            let (type_idx, m) = self.event_for(child, i)?;
                            (m, Item::Event(*type_idx))
                        }
                        ExportKind::Table(i) => {
                            let (table_ty, m) = self.table_for(child, i)?;
                            (m, Item::Table(table_ty.clone()))
                        }
                        ExportKind::Memory(i) => {
                            let (memory_ty, m) = self.memory_for(child, i)?;
                            (m, Item::Memory(memory_ty.clone()))
                        }
                        ExportKind::Global(i) => {
                            let (global_ty, m) = self.global_for(child, i)?;
                            (m, Item::Global(global_ty.clone()))
                        }
                        ExportKind::Module(i) => {
                            let (info, m) = self.module_for(child, i)?;
                            let type_idx = match info {
                                ModuleInfo::Imported { type_idx, .. } => *type_idx,
                                ModuleInfo::Nested { type_idx, .. } => *type_idx,
                            };
                            (m, Item::Module(type_idx))
                        }
                        ExportKind::Instance(i) => {
                            let (def, m) = self.instance_for(child, i)?;
                            match def {
                                InstanceDef::Import { type_idx, .. } => {
                                    (m, Item::Instance(*type_idx))
                                }
                                InstanceDef::Instantiate { module_idx } => {
                                    let (info, m) = self.module_for(m, module_idx)?;
                                    let type_idx = match info {
                                        ModuleInfo::Imported { type_idx, .. } => *type_idx,
                                        ModuleInfo::Nested { type_idx, .. } => *type_idx,
                                    };
                                    let item =
                                        self.copy_type_from_module(e.span, m, &type_idx, true)?;
                                    exports.push((e.name, item));
                                    continue;
                                }
                            }
                        }
                        ExportKind::Type(_) => {
                            return Err(Error::new(
                                span,
                                "exported types not supported yet".to_string(),
                            ))
                        }
                    };
                    let item = self.copy_item_from_module(e.span, module, &item)?;
                    exports.push((e.name, item));
                }
                _ => {}
            }
        }
        let key = (imports, exports);
        Ok(self.modules[self.cur].key_to_idx(span, key))
    }

    /// Copies an item signature from a child module into the current module.
    /// This will recursively do so for referenced types and value types and
    /// such.
    fn copy_item_from_module(
        &mut self,
        span: Span,
        child: usize,
        item: &Item<'a>,
    ) -> Result<Item<'a>, Error> {
        Ok(match item {
            // These items have everything specified inline, nothing to recurse
            // with, so we just clone it.
            Item::Memory(_) => item.clone(),
            // Items with indirect indices means the contents of the index need
            // to be fully copied into our module.
            Item::Func(idx) | Item::Event(idx) | Item::Module(idx) | Item::Instance(idx) => {
                self.copy_type_from_module(span, child, idx, false)?
            }
            Item::Table(ty) => Item::Table(TableType {
                limits: ty.limits,
                elem: self.copy_reftype_from_module(span, child, ty.elem)?,
            }),
            // Globals just need to copy over the value type and otherwise
            // contain most information inline.
            Item::Global(ty) => Item::Global(GlobalType {
                ty: self.copy_valtype_from_module(span, child, ty.ty)?,
                mutable: ty.mutable,
            }),
        })
    }

    /// Copies a type index from one module into the current module.
    ///
    /// This method will chase the actual definition of `type_idx` within
    /// `child` and recursively copy all contents into `self.cur`.
    fn copy_type_from_module(
        &mut self,
        span: Span,
        child: usize,
        type_idx: &Index<'a>,
        switch_module_to_instance: bool,
    ) -> Result<Item<'a>, Error> {
        let (ty, child) = self.type_for(child, type_idx)?;
        match ty {
            TypeInfo::Func(key) => {
                let key = key.clone();
                let my_key = (
                    key.0
                        .iter()
                        .map(|ty| self.copy_valtype_from_module(span, child, *ty))
                        .collect::<Result<Vec<_>, Error>>()?,
                    key.1
                        .iter()
                        .map(|ty| self.copy_valtype_from_module(span, child, *ty))
                        .collect::<Result<Vec<_>, Error>>()?,
                );
                Ok(Item::Func(self.modules[self.cur].key_to_idx(span, my_key)))
            }

            TypeInfo::Instance { key, .. } => {
                let key = key.clone();
                let my_key = key
                    .iter()
                    .map(|(name, item)| {
                        self.copy_item_from_module(span, child, item)
                            .map(|x| (*name, x))
                    })
                    .collect::<Result<Vec<_>, Error>>()?;
                Ok(Item::Instance(
                    self.modules[self.cur].key_to_idx(span, my_key),
                ))
            }

            TypeInfo::Module { key, .. } => {
                let key = key.clone();
                let exports = key
                    .1
                    .iter()
                    .map(|(name, item)| {
                        self.copy_item_from_module(span, child, item)
                            .map(|x| (*name, x))
                    })
                    .collect::<Result<Vec<_>, Error>>()?;
                if switch_module_to_instance {
                    return Ok(Item::Instance(
                        self.modules[self.cur].key_to_idx(span, exports),
                    ));
                }
                let imports = key
                    .0
                    .iter()
                    .map(|(module, field, item)| {
                        self.copy_item_from_module(span, child, item)
                            .map(|x| (*module, *field, x))
                    })
                    .collect::<Result<Vec<_>, Error>>()?;
                Ok(Item::Module(
                    self.modules[self.cur].key_to_idx(span, (imports, exports)),
                ))
            }

            TypeInfo::Other => Err(Error::new(
                span,
                format!("cannot copy reference types between modules right now"),
            )),
        }
    }

    fn copy_reftype_from_module(
        &mut self,
        span: Span,
        _child: usize,
        ty: RefType<'a>,
    ) -> Result<RefType<'a>, Error> {
        match ty.heap {
            HeapType::Extern
            | HeapType::Any
            | HeapType::Func
            | HeapType::Exn
            | HeapType::Eq
            | HeapType::I31 => Ok(ty),

            // It's not clear to me at this time what to do about this type.
            // What to do here sort of depends on the GC proposal. This method
            // is only invoked with the module-linking proposal, which means
            // that we have two proposals interacting, and that's always weird
            // territory anyway.
            HeapType::Index(_) => Err(Error::new(
                span,
                format!("cannot copy reference types between modules right now"),
            )),
        }
    }

    fn copy_valtype_from_module(
        &mut self,
        span: Span,
        child: usize,
        ty: ValType<'a>,
    ) -> Result<ValType<'a>, Error> {
        match ty {
            ValType::I32 | ValType::I64 | ValType::F32 | ValType::F64 | ValType::V128 => Ok(ty),

            ValType::Ref(ty) => Ok(ValType::Ref(
                self.copy_reftype_from_module(span, child, ty)?,
            )),
            // It's not clear to me at this time what to do about this type.
            // What to do here sort of depends on the GC proposal. This method
            // is only invoked with the module-linking proposal, which means
            // that we have two proposals interacting, and that's always weird
            // territory anyway.
            ValType::Rtt(..) => Err(Error::new(
                span,
                format!("cannot copy reference types between modules right now"),
            )),
        }
    }

    // Below here is a suite of `*_for` family of functions. These all delegate
    // to `item_for` below, which is a crux of the module-linking proposal. Each
    // individual method is specialized for a particular namespace, but they all
    // look the same. For comments on the meat of what's happening here see the
    // `item_for` method below.

    fn type_for(
        &self,
        module_idx: usize,
        idx: &Index<'a>,
    ) -> Result<(&TypeInfo<'a>, usize), Error> {
        self.item_for(&self.modules[module_idx], idx, Ns::Type, &|m| &m.types)
    }

    fn global_for(
        &self,
        module_idx: usize,
        idx: &Index<'a>,
    ) -> Result<(&GlobalType<'a>, usize), Error> {
        self.item_for(&self.modules[module_idx], idx, Ns::Global, &|m| &m.globals)
    }

    fn table_for(
        &self,
        module_idx: usize,
        idx: &Index<'a>,
    ) -> Result<(&TableType<'a>, usize), Error> {
        self.item_for(&self.modules[module_idx], idx, Ns::Table, &|m| &m.tables)
    }

    fn module_for(
        &self,
        module_idx: usize,
        idx: &Index<'a>,
    ) -> Result<(&ModuleInfo<'a>, usize), Error> {
        self.item_for(&self.modules[module_idx], idx, Ns::Module, &|m| &m.modules)
    }

    fn instance_for(
        &self,
        module_idx: usize,
        idx: &Index<'a>,
    ) -> Result<(&InstanceDef<'a>, usize), Error> {
        self.item_for(&self.modules[module_idx], idx, Ns::Instance, &|m| {
            &m.instances
        })
    }

    fn func_for(&self, module_idx: usize, idx: &Index<'a>) -> Result<(&Index<'a>, usize), Error> {
        self.item_for(&self.modules[module_idx], idx, Ns::Func, &|m| &m.funcs)
    }

    fn event_for(&self, module_idx: usize, idx: &Index<'a>) -> Result<(&Index<'a>, usize), Error> {
        self.item_for(&self.modules[module_idx], idx, Ns::Event, &|m| &m.events)
    }

    fn memory_for(
        &self,
        module_idx: usize,
        idx: &Index<'a>,
    ) -> Result<(&MemoryType, usize), Error> {
        self.item_for(&self.modules[module_idx], idx, Ns::Memory, &|m| &m.memories)
    }

    /// A gnarly method that resolves an `idx` within the `module` provided to
    /// the actual signature of that item.
    ///
    /// The goal of this method is to recursively follow, if necessary, chains
    /// of `alias` annotations to figure out the type signature of an item. This
    /// is required to determine the signature of an inline `module` when a type
    /// for it has otherwise not been listed.
    ///
    /// Note that on success this method returns both the type signature and the
    /// index of the module this item was defined in. The index returned may be
    /// different than the index of `module` due to alias annotations.
    fn item_for<'b, T>(
        &'b self,
        module: &'b Module<'a>,
        idx: &Index<'a>,
        ns: Ns,
        get_ns: &impl for<'c> Fn(&'c Module<'a>) -> &'c Namespace<'a, T>,
    ) -> Result<(&'b T, usize), Error> {
        let i = get_ns(module).resolve(&mut idx.clone(), ns.desc())?;
        match get_ns(module).items.get(i as usize) {
            // This can arise if you do something like `(export "" (func
            // 10000))`, in which case we can't infer the type signature of that
            // module, so we generate an error.
            None => Err(Error::new(
                idx.span(),
                format!("reference to {} is out of bounds", ns.desc()),
            )),

            // This happens when a module refers to something that's defined
            // after the module's definition. For example
            //
            //  (module
            //      (module (export "" (func $f))) ;; refer to later item
            //      (func $f)                      ;; item defined too late
            //  )
            //
            // It's not entirely clear how the upstream spec wants to eventually
            // handle these cases, but for now re return this error.
            Some(Sig::Unknown) => Err(Error::new(
                idx.span(),
                format!("reference to {} before item is defined", ns.desc()),
            )),

            // This item was defined in the module and we know its signature!
            // Return as such.
            Some(Sig::Defined { item, module }) => Ok((item, *module)),

            // This item is aliasing the export of another module. To figure out
            // its type signature we need to recursie into the `module` listed
            // here.
            Some(Sig::Alias(SigAlias::ModuleExport {
                module: module_idx,
                export_idx,
            })) => {
                let module = &self.modules[*module_idx];
                // Look up the `export_idx`th export, then try to extract an
                // `Index` from that `ExportKind`, assuming it points to the
                // right kind of item.
                let kind = module
                    .exported_ids
                    .get(*export_idx as usize)
                    .ok_or_else(|| {
                        Error::new(
                            idx.span(),
                            format!("aliased from an export that does not exist"),
                        )
                    })?;
                let (idx, ns2) = Ns::from_export(kind);
                if ns != ns2 {
                    return Err(Error::new(
                        idx.span(),
                        format!("alias points to export of wrong kind of item"),
                    ));
                }

                // Once we have our desired index within `module`, recurse!
                self.item_for(module, &idx, ns, get_ns)
            }

            // This item is aliasing the export of a type. This type could be an
            // imported instance or an imported module. (in the latter case the
            // alias was referring to an instance instantiated from an imported
            // module). In either case we chase the definition of `ty`, get its
            // export information, and go from there.
            Some(Sig::Alias(SigAlias::TypeExport {
                module,
                ty,
                export_idx,
            })) => {
                let (info, _m) = match self.type_for(*module, ty)? {
                    (TypeInfo::Instance { info, .. }, m) => (info, m),
                    (TypeInfo::Module { info, .. }, m) => (info, m),
                    _ => {
                        return Err(Error::new(
                            ty.span(),
                            format!(
                                "aliased from an instance/module that is listed with the wrong type"
                            ),
                        ))
                    }
                };
                let kind = info.exported_ids.get(*export_idx as usize).ok_or_else(|| {
                    Error::new(
                        idx.span(),
                        format!("aliased from an export that does not exist"),
                    )
                })?;
                let (idx, ns2) = Ns::from_export(kind);
                if ns != ns2 {
                    return Err(Error::new(
                        idx.span(),
                        format!("alias points to export of wrong kind of item"),
                    ));
                }
                self.item_for(info, &idx, ns, get_ns)
            }

            // In this final case we're aliasing an item defined in a child
            // module, so we recurse with that!
            Some(Sig::Alias(SigAlias::Item {
                module: module_idx,
                idx,
            })) => self.item_for(&self.modules[*module_idx], &idx, ns, get_ns),
        }
    }
}

impl<'a> Module<'a> {
    /// This is a helper method to create a synthetic `Module` representing the
    /// export information of a module type defined (or instance type). This
    /// is used to resolve metadata for aliases which bottom out in imported
    /// types of some kind.
    fn from_exports(
        span: Span,
        parent: usize,
        exports: &[ExportType<'a>],
    ) -> Result<Module<'a>, Error> {
        let mut ret = Module::default();
        ret.exports = ExportNamespace::from_exports(exports)?;
        for export in exports {
            // Note that the `.unwrap()`s below should be ok because `TypeUse`
            // annotations should all be expanded by this point.
            let kind = match &export.item.kind {
                ItemKind::Func(ty) => {
                    let idx = push(parent, span, &mut ret.funcs, ty.index.unwrap());
                    ExportKind::Func(idx)
                }
                ItemKind::Event(EventType::Exception(ty)) => {
                    let idx = push(parent, span, &mut ret.events, ty.index.unwrap());
                    ExportKind::Event(idx)
                }
                ItemKind::Global(ty) => {
                    let idx = push(parent, span, &mut ret.globals, ty.clone());
                    ExportKind::Global(idx)
                }
                ItemKind::Memory(ty) => {
                    let idx = push(parent, span, &mut ret.memories, ty.clone());
                    ExportKind::Memory(idx)
                }
                ItemKind::Table(ty) => {
                    let idx = push(parent, span, &mut ret.tables, ty.clone());
                    ExportKind::Table(idx)
                }
                ItemKind::Instance(ty) => {
                    let idx = push(
                        parent,
                        span,
                        &mut ret.instances,
                        InstanceDef::Import {
                            type_idx: ty.index.unwrap(),
                            exports: ExportNamespace::default(),
                        },
                    );
                    ExportKind::Instance(idx)
                }
                ItemKind::Module(ty) => {
                    let idx = push(
                        parent,
                        span,
                        &mut ret.modules,
                        ModuleInfo::Imported {
                            type_idx: ty.index.unwrap(),
                            exports: ExportNamespace::default(),
                        },
                    );
                    ExportKind::Module(idx)
                }
            };
            ret.exported_ids.push(kind);
        }
        return Ok(ret);

        fn push<'a, T>(parent: usize, span: Span, ns: &mut Namespace<'a, T>, item: T) -> Index<'a> {
            let ret = Index::Num(ns.alloc(), span);
            ns.push_item(parent, item);
            return ret;
        }
    }

    fn expand_type(&mut self, item: &mut ModuleField<'a>) {
        let ty = match item {
            ModuleField::Type(t) => t,
            _ => return,
        };
        let id = gensym::fill(ty.span, &mut ty.id);
        match &mut ty.def {
            TypeDef::Func(f) => {
                f.key().insert(self, Index::Id(id));
            }
            TypeDef::Instance(i) => {
                i.expand(self);
                i.key().insert(self, Index::Id(id));
            }
            TypeDef::Module(m) => {
                m.expand(self);
                m.key().insert(self, Index::Id(id));
            }
            TypeDef::Array(_) | TypeDef::Struct(_) => {}
        }
    }

    fn register(&mut self, item: &ModuleField<'a>) -> Result<(), Error> {
        match item {
            ModuleField::Import(i) => match &i.item.kind {
                ItemKind::Func(_) => self.funcs.register(i.item.id, "func")?,
                ItemKind::Memory(_) => self.memories.register(i.item.id, "memory")?,
                ItemKind::Table(_) => self.tables.register(i.item.id, "table")?,
                ItemKind::Global(_) => self.globals.register(i.item.id, "global")?,
                ItemKind::Event(_) => self.events.register(i.item.id, "event")?,
                ItemKind::Module(_) => self.modules.register(i.item.id, "module")?,
                ItemKind::Instance(_) => self.instances.register(i.item.id, "instance")?,
            },
            ModuleField::Global(i) => self.globals.register(i.id, "global")?,
            ModuleField::Memory(i) => self.memories.register(i.id, "memory")?,
            ModuleField::Func(i) => self.funcs.register(i.id, "func")?,
            ModuleField::Table(i) => self.tables.register(i.id, "table")?,
            ModuleField::NestedModule(m) => self.modules.register(m.id, "module")?,
            ModuleField::Instance(i) => self.instances.register(i.id, "instance")?,

            ModuleField::Type(i) => {
                match &i.def {
                    // For GC structure types we need to be sure to populate the
                    // field namespace here as well.
                    //
                    // The field namespace is global, but the resolved indices
                    // are relative to the struct they are defined in
                    TypeDef::Struct(r#struct) => {
                        for (i, field) in r#struct.fields.iter().enumerate() {
                            if let Some(id) = field.id {
                                self.fields.register_specific(id, i as u32, "field")?;
                            }
                        }
                    }

                    TypeDef::Instance(_)
                    | TypeDef::Array(_)
                    | TypeDef::Func(_)
                    | TypeDef::Module(_) => {}
                }
                self.types.register(i.id, "type")?
            }
            ModuleField::Elem(e) => self.elems.register(e.id, "elem")?,
            ModuleField::Data(d) => self.datas.register(d.id, "data")?,
            ModuleField::Event(e) => self.events.register(e.id, "event")?,
            ModuleField::Alias(e) => match e.kind {
                ExportKind::Func(_) => self.funcs.register(e.id, "func")?,
                ExportKind::Table(_) => self.tables.register(e.id, "table")?,
                ExportKind::Memory(_) => self.memories.register(e.id, "memory")?,
                ExportKind::Global(_) => self.globals.register(e.id, "global")?,
                ExportKind::Instance(_) => self.instances.register(e.id, "instance")?,
                ExportKind::Module(_) => self.modules.register(e.id, "module")?,
                ExportKind::Event(_) => self.events.register(e.id, "event")?,
                ExportKind::Type(_) => self.types.register(e.id, "type")?,
            },
            ModuleField::Export(e) => {
                let (idx, ns) = Ns::from_export(&e.kind);
                self.exports.push(
                    ns,
                    match idx {
                        Index::Id(id) => Some(id),
                        Index::Num(..) => None,
                    },
                )?;
                self.exported_ids.push(e.kind.clone());
                return Ok(());
            }
            ModuleField::Start(_) | ModuleField::Custom(_) => return Ok(()),

            // should not be present in AST by this point
            ModuleField::ExportAll(..) => unreachable!(),
        };

        Ok(())
    }

    fn expand_item_sig(&mut self, item: &mut ItemSig<'a>, take_inline: bool) {
        match &mut item.kind {
            ItemKind::Func(t) | ItemKind::Event(EventType::Exception(t)) => {
                self.expand_type_use(t);
            }
            ItemKind::Instance(t) => {
                self.expand_type_use(t);
                if take_inline {
                    t.inline.take();
                }
            }
            ItemKind::Module(m) => {
                self.expand_type_use(m);
                if take_inline {
                    m.inline.take();
                }
            }
            ItemKind::Global(_) | ItemKind::Table(_) | ItemKind::Memory(_) => {}
        }
    }

    fn expand_expression(&mut self, expr: &mut Expression<'a>) {
        for instr in expr.instrs.iter_mut() {
            self.expand_instr(instr);
        }
    }

    fn expand_instr(&mut self, instr: &mut Instruction<'a>) {
        match instr {
            Instruction::Block(bt)
            | Instruction::If(bt)
            | Instruction::Loop(bt)
            | Instruction::Let(LetType { block: bt, .. })
            | Instruction::Try(bt) => {
                // No expansion necessary, a type reference is already here.
                // We'll verify that it's the same as the inline type, if any,
                // later.
                if bt.ty.index.is_some() {
                    return;
                }

                match &bt.ty.inline {
                    // Only actually expand `TypeUse` with an index which appends a
                    // type if it looks like we need one. This way if the
                    // multi-value proposal isn't enabled and/or used we won't
                    // encode it.
                    Some(inline) => {
                        if inline.params.len() == 0 && inline.results.len() <= 1 {
                            return;
                        }
                    }

                    // If we didn't have either an index or an inline type
                    // listed then assume our block has no inputs/outputs, so
                    // fill in the inline type here.
                    //
                    // Do not fall through to expanding the `TypeUse` because
                    // this doesn't force an empty function type to go into the
                    // type section.
                    None => {
                        bt.ty.inline = Some(FunctionType::default());
                        return;
                    }
                }
                self.expand_type_use(&mut bt.ty);
            }
            Instruction::FuncBind(b) => {
                self.expand_type_use(&mut b.ty);
            }
            Instruction::CallIndirect(c) | Instruction::ReturnCallIndirect(c) => {
                self.expand_type_use(&mut c.ty);
            }
            _ => {}
        }
    }

    fn expand_type_use<T>(&mut self, item: &mut TypeUse<'a, T>) -> Index<'a>
    where
        T: TypeReference<'a>,
    {
        if let Some(idx) = &item.index {
            return idx.clone();
        }
        let key = match item.inline.as_mut() {
            Some(ty) => {
                ty.expand(self);
                ty.key()
            }
            None => T::default().key(),
        };
        let span = Span::from_offset(0); // FIXME: don't manufacture
        let idx = self.key_to_idx(span, key);
        item.index = Some(idx);
        return idx;
    }

    fn key_to_idx(&mut self, span: Span, key: impl TypeKey<'a>) -> Index<'a> {
        // First see if this `key` already exists in the type definitions we've
        // seen so far...
        if let Some(idx) = key.lookup(self) {
            return idx;
        }

        // ... and failing that we insert a new type definition.
        let id = gensym::gen(span);
        self.to_prepend.push(ModuleField::Type(Type {
            span,
            id: Some(id),
            def: key.to_def(span),
        }));
        let idx = Index::Id(id);
        key.insert(self, idx);

        // If we're a late call to `key_to_idx` and name registration has
        // already happened then we are appending to the index space, so
        // register the new type's information here as well.
        if self.registered {
            let idx = self.types.register(Some(id), "type").unwrap();
            self.types.items[idx as usize] = Sig::Defined {
                item: key.into_info(span, self.id),
                module: self.id,
            };
        }

        return idx;
    }

    fn resolve_field(&self, field: &mut ModuleField<'a>) -> Result<(), Error> {
        match field {
            ModuleField::Import(i) => {
                self.resolve_item_sig(&mut i.item)?;
                Ok(())
            }

            ModuleField::Type(ty) => {
                match &mut ty.def {
                    TypeDef::Func(func) => func.resolve(self)?,
                    TypeDef::Struct(struct_) => {
                        for field in &mut struct_.fields {
                            self.resolve_storagetype(&mut field.ty)?;
                        }
                    }
                    TypeDef::Array(array) => self.resolve_storagetype(&mut array.ty)?,
                    TypeDef::Module(m) => m.resolve(self)?,
                    TypeDef::Instance(i) => i.resolve(self)?,
                }
                Ok(())
            }

            ModuleField::Func(f) => {
                let (idx, inline) = self.resolve_type_use(&mut f.ty)?;
                let n = match idx {
                    Index::Num(n, _) => *n,
                    Index::Id(_) => panic!("expected `Num`"),
                };
                if let FuncKind::Inline { locals, expression } = &mut f.kind {
                    // Resolve (ref T) in locals
                    for local in locals.iter_mut() {
                        self.resolve_valtype(&mut local.ty)?;
                    }

                    // Build a scope with a local namespace for the function
                    // body
                    let mut scope = Namespace::default();

                    // Parameters come first in the scope...
                    if let Some(inline) = &inline {
                        for (id, _, _) in inline.params.iter() {
                            scope.register(*id, "local")?;
                        }
                    } else if let Some(TypeInfo::Func((params, _))) =
                        self.types.items.get(n as usize).and_then(|i| i.item())
                    {
                        for _ in 0..params.len() {
                            scope.register(None, "local")?;
                        }
                    }

                    // .. followed by locals themselves
                    for local in locals {
                        scope.register(local.id, "local")?;
                    }

                    // Initialize the expression resolver with this scope
                    let mut resolver = ExprResolver::new(self, scope);

                    // and then we can resolve the expression!
                    resolver.resolve(expression)?;

                    // specifically save the original `sig`, if it was present,
                    // because that's what we're using for local names.
                    f.ty.inline = inline;
                }
                Ok(())
            }

            ModuleField::Elem(e) => {
                match &mut e.kind {
                    ElemKind::Active { table, offset } => {
                        self.resolve(table, Ns::Table)?;
                        self.resolve_expr(offset)?;
                    }
                    ElemKind::Passive { .. } | ElemKind::Declared { .. } => {}
                }
                match &mut e.payload {
                    ElemPayload::Indices(elems) => {
                        for idx in elems {
                            self.resolve(idx, Ns::Func)?;
                        }
                    }
                    ElemPayload::Exprs { exprs, ty } => {
                        for funcref in exprs {
                            if let Some(idx) = funcref {
                                self.resolve(idx, Ns::Func)?;
                            }
                        }
                        self.resolve_heaptype(&mut ty.heap)?;
                    }
                }
                Ok(())
            }

            ModuleField::Data(d) => {
                if let DataKind::Active { memory, offset } = &mut d.kind {
                    self.resolve(memory, Ns::Memory)?;
                    self.resolve_expr(offset)?;
                }
                Ok(())
            }

            ModuleField::Start(i) => {
                self.resolve(i, Ns::Func)?;
                Ok(())
            }

            ModuleField::Export(e) => {
                let (idx, ns) = Ns::from_export_mut(&mut e.kind);
                self.resolve(idx, ns)?;
                Ok(())
            }

            ModuleField::ExportAll(..) => unreachable!(),

            ModuleField::Global(g) => {
                self.resolve_valtype(&mut g.ty.ty)?;
                if let GlobalKind::Inline(expr) = &mut g.kind {
                    self.resolve_expr(expr)?;
                }
                Ok(())
            }

            ModuleField::Event(e) => {
                match &mut e.ty {
                    EventType::Exception(ty) => {
                        self.resolve_type_use(ty)?;
                    }
                }
                Ok(())
            }

            ModuleField::Instance(i) => {
                if let InstanceKind::Inline { module, items } = &mut i.kind {
                    self.resolve(module, Ns::Module)?;
                    for item in items {
                        let (idx, ns) = Ns::from_export_mut(item);
                        self.resolve(idx, ns)?;
                    }
                }
                Ok(())
            }

            ModuleField::NestedModule(i) => {
                if let NestedModuleKind::Inline { ty, .. } = &mut i.kind {
                    let ty = ty.as_mut().expect("type index should be filled in");
                    self.resolve(ty, Ns::Type)?;
                }
                Ok(())
            }

            ModuleField::Table(t) => {
                if let TableKind::Normal(t) = &mut t.kind {
                    self.resolve_heaptype(&mut t.elem.heap)?;
                }
                Ok(())
            }

            // Everything about aliases is handled in `expand` above
            ModuleField::Alias(_) => Ok(()),

            ModuleField::Memory(_) | ModuleField::Custom(_) => Ok(()),
        }
    }

    fn resolve_valtype(&self, ty: &mut ValType<'a>) -> Result<(), Error> {
        match ty {
            ValType::Ref(ty) => self.resolve_heaptype(&mut ty.heap)?,
            ValType::Rtt(_d, i) => {
                self.resolve(i, Ns::Type)?;
            }
            _ => {}
        }
        Ok(())
    }

    fn resolve_heaptype(&self, ty: &mut HeapType<'a>) -> Result<(), Error> {
        match ty {
            HeapType::Index(i) => {
                self.resolve(i, Ns::Type)?;
            }
            _ => {}
        }
        Ok(())
    }

    fn resolve_storagetype(&self, ty: &mut StorageType<'a>) -> Result<(), Error> {
        match ty {
            StorageType::Val(ty) => self.resolve_valtype(ty)?,
            _ => {}
        }
        Ok(())
    }

    fn resolve_item_sig(&self, item: &mut ItemSig<'a>) -> Result<(), Error> {
        match &mut item.kind {
            ItemKind::Func(t) | ItemKind::Event(EventType::Exception(t)) => {
                self.resolve_type_use(t)?;
            }
            ItemKind::Global(t) => self.resolve_valtype(&mut t.ty)?,
            ItemKind::Instance(t) => {
                self.resolve_type_use(t)?;
            }
            ItemKind::Module(m) => {
                self.resolve_type_use(m)?;
            }
            ItemKind::Table(t) => {
                self.resolve_heaptype(&mut t.elem.heap)?;
            }
            ItemKind::Memory(_) => {}
        }
        Ok(())
    }

    fn resolve_type_use<'b, T>(
        &self,
        ty: &'b mut TypeUse<'a, T>,
    ) -> Result<(&'b Index<'a>, Option<T>), Error>
    where
        T: TypeReference<'a>,
    {
        let idx = ty.index.as_mut().unwrap();
        self.resolve(idx, Ns::Type)?;

        // If the type was listed inline *and* it was specified via a type index
        // we need to assert they're the same.
        //
        // Note that we resolve the type first to transform all names to
        // indices to ensure that all the indices line up.
        if let Some(inline) = &mut ty.inline {
            inline.resolve(self)?;
            inline.check_matches(idx, self)?;
        }

        Ok((idx, ty.inline.take()))
    }

    fn resolve_expr(&self, expr: &mut Expression<'a>) -> Result<(), Error> {
        ExprResolver::new(self, Namespace::default()).resolve(expr)
    }

    pub fn resolve(&self, idx: &mut Index<'a>, ns: Ns) -> Result<u32, Error> {
        match ns {
            Ns::Func => self.funcs.resolve(idx, "func"),
            Ns::Table => self.tables.resolve(idx, "table"),
            Ns::Global => self.globals.resolve(idx, "global"),
            Ns::Memory => self.memories.resolve(idx, "memory"),
            Ns::Instance => self.instances.resolve(idx, "instance"),
            Ns::Module => self.modules.resolve(idx, "module"),
            Ns::Event => self.events.resolve(idx, "event"),
            Ns::Type => self.types.resolve(idx, "type"),
        }
    }
}

pub struct Namespace<'a, T> {
    names: HashMap<Id<'a>, u32>,
    count: u32,
    next_item: usize,
    items: Vec<Sig<'a, T>>,
}

enum Sig<'a, T> {
    Defined { item: T, module: usize },
    Alias(SigAlias<'a>),
    Unknown,
}

enum SigAlias<'a> {
    TypeExport {
        module: usize,
        ty: Index<'a>,
        export_idx: u32,
    },
    ModuleExport {
        module: usize,
        export_idx: u32,
    },
    Item {
        module: usize,
        idx: Index<'a>,
    },
}

impl<'a, T> Namespace<'a, T> {
    fn register(&mut self, name: Option<Id<'a>>, desc: &str) -> Result<u32, Error> {
        let index = self.alloc();
        if let Some(name) = name {
            if let Some(_prev) = self.names.insert(name, index) {
                // FIXME: temporarily allow duplicately-named data and element
                // segments. This is a sort of dumb hack to get the spec test
                // suite working (ironically).
                //
                // So as background, the text format disallows duplicate
                // identifiers, causing a parse error if they're found. There
                // are two tests currently upstream, however, data.wast and
                // elem.wast, which *look* like they have duplicately named
                // element and data segments. These tests, however, are using
                // pre-bulk-memory syntax where a bare identifier was the
                // table/memory being initialized. In post-bulk-memory this
                // identifier is the name of the segment. Since we implement
                // post-bulk-memory features that means that we're parsing the
                // memory/table-to-initialize as the name of the segment.
                //
                // This is technically incorrect behavior but no one is
                // hopefully relying on this too much. To get the spec tests
                // passing we ignore errors for elem/data segments. Once the
                // spec tests get updated enough we can remove this condition
                // and return errors for them.
                if desc != "elem" && desc != "data" {
                    return Err(Error::new(
                        name.span(),
                        format!("duplicate {} identifier", desc),
                    ));
                }
            }
        }
        Ok(index)
    }

    fn alloc(&mut self) -> u32 {
        let index = self.count;
        self.items.push(Sig::Unknown);
        self.count += 1;
        return index;
    }

    fn register_specific(&mut self, name: Id<'a>, index: u32, desc: &str) -> Result<(), Error> {
        if let Some(_prev) = self.names.insert(name, index) {
            return Err(Error::new(
                name.span(),
                format!("duplicate identifier for {}", desc),
            ));
        }
        Ok(())
    }

    fn resolve(&self, idx: &mut Index<'a>, desc: &str) -> Result<u32, Error> {
        let id = match idx {
            Index::Num(n, _) => return Ok(*n),
            Index::Id(id) => id,
        };
        if let Some(&n) = self.names.get(id) {
            *idx = Index::Num(n, id.span());
            return Ok(n);
        }
        Err(resolve_error(*id, desc))
    }

    fn push_item(&mut self, module: usize, item: T) {
        self.push(Sig::Defined { item, module });
    }

    fn push(&mut self, item: Sig<'a, T>) {
        match self.items[self.next_item] {
            Sig::Unknown => {}
            _ => panic!("item already known"),
        }
        self.items[self.next_item] = item;
        self.next_item += 1;
    }
}

impl<T> Sig<'_, T> {
    fn item(&self) -> Option<&T> {
        match self {
            Sig::Defined { item, .. } => Some(item),
            _ => None,
        }
    }
}

fn resolve_error(id: Id<'_>, ns: &str) -> Error {
    assert!(!id.is_gensym());
    Error::new(
        id.span(),
        format!("failed to find {} named `${}`", ns, id.name()),
    )
}

impl<'a, T> Default for Namespace<'a, T> {
    fn default() -> Namespace<'a, T> {
        Namespace {
            names: HashMap::new(),
            count: 0,
            next_item: 0,
            items: Vec::new(),
        }
    }
}

#[derive(Debug, Clone)]
struct ExprBlock<'a> {
    // The label of the block
    label: Option<Id<'a>>,
    // Whether this block pushed a new scope for resolving locals
    pushed_scope: bool,
}

struct ExprResolver<'a, 'b> {
    module: &'b Module<'a>,
    // Scopes tracks the local namespace and dynamically grows as we enter/exit
    // `let` blocks
    scopes: Vec<Namespace<'a, ()>>,
    blocks: Vec<ExprBlock<'a>>,
}

impl<'a, 'b> ExprResolver<'a, 'b> {
    fn new(module: &'b Module<'a>, initial_scope: Namespace<'a, ()>) -> ExprResolver<'a, 'b> {
        ExprResolver {
            module,
            scopes: vec![initial_scope],
            blocks: Vec::new(),
        }
    }

    fn resolve(&mut self, expr: &mut Expression<'a>) -> Result<(), Error> {
        for instr in expr.instrs.iter_mut() {
            self.resolve_instr(instr)?;
        }
        Ok(())
    }

    fn resolve_block_type(&mut self, bt: &mut BlockType<'a>) -> Result<(), Error> {
        // Ok things get interesting here. First off when parsing `bt`
        // *optionally* has an index and a function type listed. If
        // they're both not present it's equivalent to 0 params and 0
        // results.
        //
        // In MVP wasm blocks can have 0 params and 0-1 results. Now
        // there's also multi-value. We want to prefer MVP wasm wherever
        // possible (for backcompat) so we want to list this block as
        // being an "MVP" block if we can. The encoder only has
        // `BlockType` to work with, so it'll be looking at `params` and
        // `results` to figure out what to encode. If `params` and
        // `results` fit within MVP, then it uses MVP encoding
        //
        // To put all that together, here we handle:
        //
        // * If the `index` was specified, resolve it and use it as the
        //   source of truth. If this turns out to be an MVP type,
        //   record it as such.
        // * Otherwise use `params` and `results` as the source of
        //   truth. *If* this were a non-MVP compatible block `index`
        //   would be filled by by `tyexpand.rs`.
        //
        // tl;dr; we handle the `index` here if it's set and then fill
        // out `params` and `results` if we can, otherwise no work
        // happens.
        if bt.ty.index.is_some() {
            let (ty, _) = self.module.resolve_type_use(&mut bt.ty)?;
            let n = match ty {
                Index::Num(n, _) => *n,
                Index::Id(_) => panic!("expected `Num`"),
            };
            let ty = match self
                .module
                .types
                .items
                .get(n as usize)
                .and_then(|s| s.item())
            {
                Some(TypeInfo::Func(ty)) => ty,
                _ => return Ok(()),
            };
            if ty.0.len() == 0 && ty.1.len() <= 1 {
                let mut inline = FunctionType::default();
                inline.results = ty.1.clone();
                bt.ty.inline = Some(inline);
                bt.ty.index = None;
            }
        }

        // If the inline annotation persists to this point then resolve
        // all of its inline value types.
        if let Some(inline) = &mut bt.ty.inline {
            inline.resolve(self.module)?;
        }
        Ok(())
    }

    fn resolve_instr(&mut self, instr: &mut Instruction<'a>) -> Result<(), Error> {
        use crate::ast::Instruction::*;

        match instr {
            MemorySize(i) | MemoryGrow(i) | MemoryFill(i) => {
                self.module.resolve(&mut i.mem, Ns::Memory)?;
            }
            MemoryInit(i) => {
                self.module.datas.resolve(&mut i.data, "data")?;
                self.module.resolve(&mut i.mem, Ns::Memory)?;
            }
            MemoryCopy(i) => {
                self.module.resolve(&mut i.src, Ns::Memory)?;
                self.module.resolve(&mut i.dst, Ns::Memory)?;
            }
            DataDrop(i) => {
                self.module.datas.resolve(i, "data")?;
            }

            TableInit(i) => {
                self.module.elems.resolve(&mut i.elem, "elem")?;
                self.module.resolve(&mut i.table, Ns::Table)?;
            }
            ElemDrop(i) => {
                self.module.elems.resolve(i, "elem")?;
            }

            TableCopy(i) => {
                self.module.resolve(&mut i.dst, Ns::Table)?;
                self.module.resolve(&mut i.src, Ns::Table)?;
            }

            TableFill(i) | TableSet(i) | TableGet(i) | TableSize(i) | TableGrow(i) => {
                self.module.resolve(&mut i.dst, Ns::Table)?;
            }

            GlobalSet(i) | GlobalGet(i) => {
                self.module.resolve(i, Ns::Global)?;
            }

            LocalSet(i) | LocalGet(i) | LocalTee(i) => {
                assert!(self.scopes.len() > 0);
                // Resolve a local by iterating over scopes from most recent
                // to less recent. This allows locals added by `let` blocks to
                // shadow less recent locals.
                for (depth, scope) in self.scopes.iter().enumerate().rev() {
                    if let Err(e) = scope.resolve(i, "local") {
                        if depth == 0 {
                            // There are no more scopes left, report this as
                            // the result
                            return Err(e);
                        }
                    } else {
                        break;
                    }
                }
                // We must have taken the `break` and resolved the local
                assert!(i.is_resolved());
            }

            Call(i) | RefFunc(i) | ReturnCall(i) => {
                self.module.resolve(i, Ns::Func)?;
            }

            CallIndirect(c) | ReturnCallIndirect(c) => {
                self.module.resolve(&mut c.table, Ns::Table)?;
                self.module.resolve_type_use(&mut c.ty)?;
            }

            FuncBind(b) => {
                self.module.resolve_type_use(&mut b.ty)?;
            }

            Let(t) => {
                // Resolve (ref T) in locals
                for local in &mut t.locals {
                    self.module.resolve_valtype(&mut local.ty)?;
                }

                // Register all locals defined in this let
                let mut scope = Namespace::default();
                for local in &t.locals {
                    scope.register(local.id, "local")?;
                }
                self.scopes.push(scope);
                self.blocks.push(ExprBlock {
                    label: t.block.label,
                    pushed_scope: true,
                });

                self.resolve_block_type(&mut t.block)?;
            }

            Block(bt) | If(bt) | Loop(bt) | Try(bt) => {
                self.blocks.push(ExprBlock {
                    label: bt.label,
                    pushed_scope: false,
                });
                self.resolve_block_type(bt)?;
            }

            // On `End` instructions we pop a label from the stack, and for both
            // `End` and `Else` instructions if they have labels listed we
            // verify that they match the label at the beginning of the block.
            Else(_) | End(_) => {
                let (matching_block, label) = match instr {
                    Else(label) => (self.blocks.last().cloned(), label),
                    End(label) => (self.blocks.pop(), label),
                    _ => unreachable!(),
                };
                let matching_block = match matching_block {
                    Some(l) => l,
                    None => return Ok(()),
                };

                // Reset the local scopes to before this block was entered
                if matching_block.pushed_scope {
                    self.scopes.pop();
                }

                let label = match label {
                    Some(l) => l,
                    None => return Ok(()),
                };
                if Some(*label) == matching_block.label {
                    return Ok(());
                }
                return Err(Error::new(
                    label.span(),
                    "mismatching labels between end and block".to_string(),
                ));
            }

            Br(i) | BrIf(i) | BrOnNull(i) => {
                self.resolve_label(i)?;
            }

            BrTable(i) => {
                for label in i.labels.iter_mut() {
                    self.resolve_label(label)?;
                }
                self.resolve_label(&mut i.default)?;
            }

            Throw(i) => {
                self.module.resolve(i, Ns::Event)?;
            }
            BrOnExn(b) => {
                self.resolve_label(&mut b.label)?;
                self.module.resolve(&mut b.exn, Ns::Event)?;
            }
            BrOnCast(b) => {
                self.resolve_label(&mut b.label)?;
                self.module.resolve_heaptype(&mut b.val)?;
                self.module.resolve_heaptype(&mut b.rtt)?;
            }

            Select(s) => {
                if let Some(list) = &mut s.tys {
                    for ty in list {
                        self.module.resolve_valtype(ty)?;
                    }
                }
            }

            StructNew(i)
            | StructNewWithRtt(i)
            | StructNewDefaultWithRtt(i)
            | ArrayNewWithRtt(i)
            | ArrayNewDefaultWithRtt(i)
            | ArrayGet(i)
            | ArrayGetS(i)
            | ArrayGetU(i)
            | ArraySet(i)
            | ArrayLen(i) => {
                self.module.resolve(i, Ns::Type)?;
            }
            RTTCanon(t) => {
                self.module.resolve_heaptype(t)?;
            }
            RTTSub(s) => {
                self.module.resolve_heaptype(&mut s.input_rtt)?;
                self.module.resolve_heaptype(&mut s.output_rtt)?;
            }
            RefTest(t) | RefCast(t) => {
                self.module.resolve_heaptype(&mut t.val)?;
                self.module.resolve_heaptype(&mut t.rtt)?;
            }

            StructSet(s) | StructGet(s) | StructGetS(s) | StructGetU(s) => {
                self.module.resolve(&mut s.r#struct, Ns::Type)?;
                self.module.fields.resolve(&mut s.field, "field")?;
            }
            StructNarrow(s) => {
                self.module.resolve_valtype(&mut s.from)?;
                self.module.resolve_valtype(&mut s.to)?;
            }

            RefNull(ty) => self.module.resolve_heaptype(ty)?,

            I32Load(m)
            | I64Load(m)
            | F32Load(m)
            | F64Load(m)
            | I32Load8s(m)
            | I32Load8u(m)
            | I32Load16s(m)
            | I32Load16u(m)
            | I64Load8s(m)
            | I64Load8u(m)
            | I64Load16s(m)
            | I64Load16u(m)
            | I64Load32s(m)
            | I64Load32u(m)
            | I32Store(m)
            | I64Store(m)
            | F32Store(m)
            | F64Store(m)
            | I32Store8(m)
            | I32Store16(m)
            | I64Store8(m)
            | I64Store16(m)
            | I64Store32(m)
            | I32AtomicLoad(m)
            | I64AtomicLoad(m)
            | I32AtomicLoad8u(m)
            | I32AtomicLoad16u(m)
            | I64AtomicLoad8u(m)
            | I64AtomicLoad16u(m)
            | I64AtomicLoad32u(m)
            | I32AtomicStore(m)
            | I64AtomicStore(m)
            | I32AtomicStore8(m)
            | I32AtomicStore16(m)
            | I64AtomicStore8(m)
            | I64AtomicStore16(m)
            | I64AtomicStore32(m)
            | I32AtomicRmwAdd(m)
            | I64AtomicRmwAdd(m)
            | I32AtomicRmw8AddU(m)
            | I32AtomicRmw16AddU(m)
            | I64AtomicRmw8AddU(m)
            | I64AtomicRmw16AddU(m)
            | I64AtomicRmw32AddU(m)
            | I32AtomicRmwSub(m)
            | I64AtomicRmwSub(m)
            | I32AtomicRmw8SubU(m)
            | I32AtomicRmw16SubU(m)
            | I64AtomicRmw8SubU(m)
            | I64AtomicRmw16SubU(m)
            | I64AtomicRmw32SubU(m)
            | I32AtomicRmwAnd(m)
            | I64AtomicRmwAnd(m)
            | I32AtomicRmw8AndU(m)
            | I32AtomicRmw16AndU(m)
            | I64AtomicRmw8AndU(m)
            | I64AtomicRmw16AndU(m)
            | I64AtomicRmw32AndU(m)
            | I32AtomicRmwOr(m)
            | I64AtomicRmwOr(m)
            | I32AtomicRmw8OrU(m)
            | I32AtomicRmw16OrU(m)
            | I64AtomicRmw8OrU(m)
            | I64AtomicRmw16OrU(m)
            | I64AtomicRmw32OrU(m)
            | I32AtomicRmwXor(m)
            | I64AtomicRmwXor(m)
            | I32AtomicRmw8XorU(m)
            | I32AtomicRmw16XorU(m)
            | I64AtomicRmw8XorU(m)
            | I64AtomicRmw16XorU(m)
            | I64AtomicRmw32XorU(m)
            | I32AtomicRmwXchg(m)
            | I64AtomicRmwXchg(m)
            | I32AtomicRmw8XchgU(m)
            | I32AtomicRmw16XchgU(m)
            | I64AtomicRmw8XchgU(m)
            | I64AtomicRmw16XchgU(m)
            | I64AtomicRmw32XchgU(m)
            | I32AtomicRmwCmpxchg(m)
            | I64AtomicRmwCmpxchg(m)
            | I32AtomicRmw8CmpxchgU(m)
            | I32AtomicRmw16CmpxchgU(m)
            | I64AtomicRmw8CmpxchgU(m)
            | I64AtomicRmw16CmpxchgU(m)
            | I64AtomicRmw32CmpxchgU(m)
            | V128Load(m)
            | I16x8Load8x8S(m)
            | I16x8Load8x8U(m)
            | I32x4Load16x4S(m)
            | I32x4Load16x4U(m)
            | I64x2Load32x2S(m)
            | I64x2Load32x2U(m)
            | V8x16LoadSplat(m)
            | V16x8LoadSplat(m)
            | V32x4LoadSplat(m)
            | V64x2LoadSplat(m)
            | V128Store(m)
            | MemoryAtomicNotify(m)
            | MemoryAtomicWait32(m)
            | MemoryAtomicWait64(m) => {
                self.module.resolve(&mut m.memory, Ns::Memory)?;
            }

            _ => {}
        }
        Ok(())
    }

    fn resolve_label(&self, label: &mut Index<'a>) -> Result<(), Error> {
        let id = match label {
            Index::Num(..) => return Ok(()),
            Index::Id(id) => *id,
        };
        let idx = self
            .blocks
            .iter()
            .rev()
            .enumerate()
            .filter_map(|(i, b)| b.label.map(|l| (i, l)))
            .find(|(_, l)| *l == id);
        match idx {
            Some((idx, _)) => {
                *label = Index::Num(idx as u32, id.span());
                Ok(())
            }
            None => Err(resolve_error(id, "label")),
        }
    }
}

enum TypeInfo<'a> {
    Func(FuncKey<'a>),
    Module {
        key: ModuleKey<'a>,
        info: Module<'a>,
    },
    Instance {
        key: InstanceKey<'a>,
        info: Module<'a>,
    },
    Other,
}

trait TypeReference<'a>: Default {
    type Key: TypeKey<'a>;
    fn key(&self) -> Self::Key;
    fn expand(&mut self, cx: &mut Module<'a>);
    fn check_matches(&mut self, idx: &Index<'a>, cx: &Module<'a>) -> Result<(), Error>;
    fn resolve(&mut self, cx: &Module<'a>) -> Result<(), Error>;
}

trait TypeKey<'a> {
    fn lookup(&self, cx: &Module<'a>) -> Option<Index<'a>>;
    fn to_def(&self, span: Span) -> TypeDef<'a>;
    fn insert(&self, cx: &mut Module<'a>, id: Index<'a>);
    fn into_info(self, span: Span, cur: usize) -> TypeInfo<'a>;
}

type FuncKey<'a> = (Vec<ValType<'a>>, Vec<ValType<'a>>);

impl<'a> TypeReference<'a> for FunctionType<'a> {
    type Key = FuncKey<'a>;

    fn key(&self) -> Self::Key {
        let params = self.params.iter().map(|p| p.2).collect::<Vec<_>>();
        let results = self.results.clone();
        (params, results)
    }

    fn expand(&mut self, _cx: &mut Module<'a>) {}

    fn check_matches(&mut self, idx: &Index<'a>, cx: &Module<'a>) -> Result<(), Error> {
        let n = match idx {
            Index::Num(n, _) => *n,
            Index::Id(_) => panic!("expected `Num`"),
        };
        let (params, results) = match cx.types.items.get(n as usize).and_then(|i| i.item()) {
            Some(TypeInfo::Func(ty)) => ty,
            _ => return Ok(()),
        };

        // Here we need to check that the inline type listed (ourselves) matches
        // what was listed in the module itself (the `params` and `results`
        // above). The listed values in `types` are not resolved yet, although
        // we should be resolved. In any case we do name resolution
        // opportunistically here to see if the values are equal.

        let types_not_equal = |a: &ValType, b: &ValType| {
            let mut a = a.clone();
            let mut b = b.clone();
            drop(cx.resolve_valtype(&mut a));
            drop(cx.resolve_valtype(&mut b));
            a != b
        };

        let not_equal = params.len() != self.params.len()
            || results.len() != self.results.len()
            || params
                .iter()
                .zip(self.params.iter())
                .any(|(a, (_, _, b))| types_not_equal(a, b))
            || results
                .iter()
                .zip(self.results.iter())
                .any(|(a, b)| types_not_equal(a, b));
        if not_equal {
            return Err(Error::new(
                idx.span(),
                format!("inline function type doesn't match type reference"),
            ));
        }

        Ok(())
    }

    fn resolve(&mut self, cx: &Module<'a>) -> Result<(), Error> {
        // Resolve the (ref T) value types in the final function type
        for param in &mut self.params {
            cx.resolve_valtype(&mut param.2)?;
        }
        for result in &mut self.results {
            cx.resolve_valtype(result)?;
        }
        Ok(())
    }
}

impl<'a> TypeKey<'a> for FuncKey<'a> {
    fn lookup(&self, cx: &Module<'a>) -> Option<Index<'a>> {
        cx.func_type_to_idx.get(self).cloned()
    }

    fn to_def(&self, _span: Span) -> TypeDef<'a> {
        TypeDef::Func(FunctionType {
            params: self.0.iter().map(|t| (None, None, *t)).collect(),
            results: self.1.clone(),
        })
    }

    fn insert(&self, cx: &mut Module<'a>, idx: Index<'a>) {
        cx.func_type_to_idx.entry(self.clone()).or_insert(idx);
    }

    fn into_info(self, _span: Span, _cur: usize) -> TypeInfo<'a> {
        TypeInfo::Func(self)
    }
}

// A list of the exports of a module as well as the signature they export.
type InstanceKey<'a> = Vec<(&'a str, Item<'a>)>;

impl<'a> TypeReference<'a> for InstanceType<'a> {
    type Key = InstanceKey<'a>;

    fn key(&self) -> Self::Key {
        self.exports
            .iter()
            .map(|export| (export.name, Item::new(&export.item)))
            .collect()
    }

    fn expand(&mut self, cx: &mut Module<'a>) {
        for export in self.exports.iter_mut() {
            cx.expand_item_sig(&mut export.item, true);
        }
    }

    fn check_matches(&mut self, idx: &Index<'a>, cx: &Module<'a>) -> Result<(), Error> {
        drop(cx);
        Err(Error::new(
            idx.span(),
            format!("cannot specify instance type as a reference and inline"),
        ))
    }

    fn resolve(&mut self, cx: &Module<'a>) -> Result<(), Error> {
        for export in self.exports.iter_mut() {
            cx.resolve_item_sig(&mut export.item)?;
        }
        Ok(())
    }
}

impl<'a> TypeKey<'a> for InstanceKey<'a> {
    fn lookup(&self, cx: &Module<'a>) -> Option<Index<'a>> {
        cx.instance_type_to_idx.get(self).cloned()
    }

    fn to_def(&self, span: Span) -> TypeDef<'a> {
        let exports = self
            .iter()
            .map(|(name, item)| ExportType {
                span,
                name,
                item: item.to_sig(span),
            })
            .collect();
        TypeDef::Instance(InstanceType { exports })
    }

    fn insert(&self, cx: &mut Module<'a>, idx: Index<'a>) {
        cx.instance_type_to_idx.entry(self.clone()).or_insert(idx);
    }

    fn into_info(self, span: Span, cur: usize) -> TypeInfo<'a> {
        let exports = self
            .iter()
            .map(|(name, item)| ExportType {
                span,
                name,
                item: item.to_sig(span),
            })
            .collect::<Vec<_>>();
        TypeInfo::Instance {
            key: self,
            // if exports information fails to get generated for duplicate
            // information we just don't make any names available for
            // resolution to have errors generated elsewhere.
            info: Module::from_exports(span, cur, &exports).unwrap_or_default(),
        }
    }
}

// The first element of this pair is the list of imports in the module, and the
// second element is the list of exports.
type ModuleKey<'a> = (
    Vec<(&'a str, Option<&'a str>, Item<'a>)>,
    Vec<(&'a str, Item<'a>)>,
);

impl<'a> TypeReference<'a> for ModuleType<'a> {
    type Key = ModuleKey<'a>;

    fn key(&self) -> Self::Key {
        assert!(self.instance_exports.is_empty());
        let imports = self
            .imports
            .iter()
            .map(|import| (import.module, import.field, Item::new(&import.item)))
            .collect();
        let exports = self
            .exports
            .iter()
            .map(|export| (export.name, Item::new(&export.item)))
            .collect();
        (imports, exports)
    }

    fn expand(&mut self, cx: &mut Module<'a>) {
        assert!(self.instance_exports.is_empty());
        for export in self.exports.iter_mut() {
            cx.expand_item_sig(&mut export.item, true);
        }
        for import in self.imports.iter_mut() {
            cx.expand_item_sig(&mut import.item, true);
        }
    }

    fn check_matches(&mut self, idx: &Index<'a>, cx: &Module<'a>) -> Result<(), Error> {
        drop(cx);
        Err(Error::new(
            idx.span(),
            format!("cannot specify module type as a reference and inline"),
        ))
    }

    fn resolve(&mut self, cx: &Module<'a>) -> Result<(), Error> {
        assert!(self.instance_exports.is_empty());
        for i in self.imports.iter_mut() {
            cx.resolve_item_sig(&mut i.item)?;
        }
        for e in self.exports.iter_mut() {
            cx.resolve_item_sig(&mut e.item)?;
        }
        Ok(())
    }
}

impl<'a> TypeKey<'a> for ModuleKey<'a> {
    fn lookup(&self, cx: &Module<'a>) -> Option<Index<'a>> {
        cx.module_type_to_idx.get(self).cloned()
    }

    fn to_def(&self, span: Span) -> TypeDef<'a> {
        let imports = self
            .0
            .iter()
            .map(|(module, field, item)| Import {
                span,
                module,
                field: *field,
                item: item.to_sig(span),
            })
            .collect();
        let exports = self
            .1
            .iter()
            .map(|(name, item)| ExportType {
                span,
                name,
                item: item.to_sig(span),
            })
            .collect();
        TypeDef::Module(ModuleType {
            imports,
            exports,
            instance_exports: Vec::new(),
        })
    }

    fn insert(&self, cx: &mut Module<'a>, idx: Index<'a>) {
        cx.module_type_to_idx.entry(self.clone()).or_insert(idx);
    }

    fn into_info(self, span: Span, cur: usize) -> TypeInfo<'a> {
        let exports = self
            .1
            .iter()
            .map(|(name, item)| ExportType {
                span,
                name,
                item: item.to_sig(span),
            })
            .collect::<Vec<_>>();
        TypeInfo::Module {
            key: self,
            // see above comment for why `unwrap_or_default` is used here.
            info: Module::from_exports(span, cur, &exports).unwrap_or_default(),
        }
    }
}

// A lookalike to `ItemKind` except without all non-relevant information for
// hashing. This is used as a hash key for instance/module type lookup.
#[derive(Clone, PartialEq, Eq, Hash)]
enum Item<'a> {
    Func(Index<'a>),
    Table(TableType<'a>),
    Memory(MemoryType),
    Global(GlobalType<'a>),
    Event(Index<'a>),
    Module(Index<'a>),
    Instance(Index<'a>),
}

impl<'a> Item<'a> {
    fn new(item: &ItemSig<'a>) -> Item<'a> {
        match &item.kind {
            ItemKind::Func(f) => Item::Func(f.index.expect("should have index")),
            ItemKind::Instance(f) => Item::Instance(f.index.expect("should have index")),
            ItemKind::Module(f) => Item::Module(f.index.expect("should have index")),
            ItemKind::Event(EventType::Exception(f)) => {
                Item::Event(f.index.expect("should have index"))
            }
            ItemKind::Table(t) => Item::Table(t.clone()),
            ItemKind::Memory(t) => Item::Memory(t.clone()),
            ItemKind::Global(t) => Item::Global(t.clone()),
        }
    }

    fn to_sig(&self, span: Span) -> ItemSig<'a> {
        let kind = match self {
            Item::Func(index) => ItemKind::Func(TypeUse::new_with_index(*index)),
            Item::Event(index) => {
                ItemKind::Event(EventType::Exception(TypeUse::new_with_index(*index)))
            }
            Item::Instance(index) => ItemKind::Instance(TypeUse::new_with_index(*index)),
            Item::Module(index) => ItemKind::Module(TypeUse::new_with_index(*index)),
            Item::Table(t) => ItemKind::Table(t.clone()),
            Item::Memory(t) => ItemKind::Memory(t.clone()),
            Item::Global(t) => ItemKind::Global(t.clone()),
        };
        ItemSig {
            span,
            id: None,
            name: None,
            kind,
        }
    }
}

#[derive(Default)]
struct ExportNamespace<'a> {
    names: HashMap<(Ns, Id<'a>), u32>,
    num: u32,
}

impl<'a> ExportNamespace<'a> {
    fn from_exports(exports: &[ExportType<'a>]) -> Result<ExportNamespace<'a>, Error> {
        let mut ret = ExportNamespace::default();
        for export in exports {
            ret.push(Ns::from_item(&export.item), export.item.id)?;
        }
        Ok(ret)
    }

    fn push(&mut self, ns: Ns, id: Option<Id<'a>>) -> Result<(), Error> {
        if let Some(id) = id {
            self.names.insert((ns, id), self.num);
        }
        self.num += 1;
        Ok(())
    }

    fn resolve(&self, idx: &mut Index<'a>, ns: Ns) -> Result<u32, Error> {
        let id = match idx {
            Index::Num(n, _) => return Ok(*n),
            Index::Id(id) => id,
        };
        if let Some(&n) = self.names.get(&(ns, *id)) {
            *idx = Index::Num(n, id.span());
            return Ok(n);
        }
        Err(resolve_error(*id, ns.desc()))
    }
}
