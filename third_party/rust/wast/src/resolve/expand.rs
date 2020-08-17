use crate::ast::*;
use crate::resolve::gensym;
use crate::resolve::Ns;
use crate::Error;
use std::collections::{HashMap, HashSet};

/// Runs an expansion process on the fields provided to elaborate and expand
/// features from the module-linking proposal. Namely this handles>
///
/// * Injection of child `alias` directives to fill in `$a.$b` identifiers
/// * Injection of parent `alias` directives for names that are likely to be
///   resolved from the parent.
/// * Expansion of export-all annotations in modules definitions and types.
///
/// Note that this pass runs before name resolution so no indices are worked
/// with as part of this pass. Instead everything internally works off
/// identifiers and new items have generated identifiers assigned to them.
pub fn run(fields: &mut Vec<ModuleField>) -> Result<(), Error> {
    Expander::default().process(fields, None)
}

#[derive(Default)]
struct Expander<'a> {
    to_append: Vec<ModuleField<'a>>,

    /// The set of all names reference in a module. Each missing name has an
    /// associated namespace with it as well for which kind of item it's
    /// referring to.
    ///
    /// This map is built as part of `record_missing_name`.
    all_missing: HashSet<(Id<'a>, Ns)>,

    /// This is the set of all names defined in this module. Each name is
    /// defined in a particular namespace as well.
    ///
    /// This map is built as part of `record_missing_name`.
    defined_names: HashSet<(Id<'a>, Ns)>,

    /// This is the set of all missing names which might be filled in as
    /// `$a.$b` names automatically. The first level of this hash map is the
    /// instance name it would come from (e.g. "a") and the second level contains
    /// the item name and namespace that would be used (e.g. "b" and Ns::Func).
    ///
    /// The final value in the map is the full `$a.$b` identifier used to
    /// synthesize as the name of `alias` annotations.
    ///
    /// This map is built as part of `record_missing_name`.
    missing_names: HashMap<&'a str, HashMap<(&'a str, Ns), Id<'a>>>,

    /// This map contains information about the exports of a module, used to
    /// fill in export-all annotations. This is only recorded for inline modules
    /// which have a name associated with them.
    module_exports: HashMap<Id<'a>, ExportInfo<'a>>,

    /// This map contains info about the exports of locally defined instances.
    /// Each locally defined instance, with a name, is recorded here about how
    /// we can learn about its export information.
    instance_exports: HashMap<Id<'a>, InstanceExports<'a>>,

    /// This map is similar to the above, but for instance types rather than
    /// instances themselves.
    instance_type_exports: HashMap<Id<'a>, Vec<ExportType<'a>>>,
}

enum InstanceExports<'a> {
    /// This instance was an import and we have its export information available
    /// inline from the definiton.
    Inline(ExportInfo<'a>),
    /// This instance is the instantiation of the module listed here. The
    /// `module_exports` map will need to be consulted for export information.
    Module(Id<'a>),
}

impl<'a> Expander<'a> {
    /// Process all elements of `fields`, with the specified expansion method.
    ///
    /// This method will handle the `to_append` field of this `Expander`,
    /// appending items after processing a `ModuleField` as necessary.
    fn process(
        &mut self,
        fields: &mut Vec<ModuleField<'a>>,
        parent: Option<&Expander<'a>>,
    ) -> Result<(), Error> {
        // The first step here is to record information about defined/missing
        // names in this module. At the same time we also scrape information
        // about defined modules to learn about the exports of inline modules.
        for field in fields.iter_mut() {
            self.record_missing_name(field);
            self.discover_module_names(field);
        }

        // Next up we can start injecting alias annotations from our parent. Any
        // name defined in our parent but not defined locally is inherited with
        // an `Alias` annotation pointing to the parent.
        if let Some(parent) = parent {
            let mut names_to_add = self
                .all_missing
                .iter()
                // skip anything defined in our own module ...
                .filter(|key| !self.defined_names.contains(key))
                // ... and also skip anything that's not defined in the parent
                // module
                .filter(|key| parent.defined_names.contains(key))
                .collect::<Vec<_>>();

            // Ensure we have deterministic output by inserting aliases in a
            // known order.
            names_to_add.sort_by_key(|e| e.0.name());

            for key in names_to_add {
                // Inject an `(alias (parent ...))` annotation now that we know
                // we're referencing an item from the parent module.
                //
                // For now we insert this at the front since these items may be
                // referred to by later items (like types and modules), and
                // inserting them at the front ensure that they're defined early
                // in the index space.
                fields.insert(
                    0,
                    ModuleField::Alias(Alias {
                        id: Some(key.0),
                        instance: None,
                        name: None,
                        span: key.0.span(),
                        kind: key.1.to_export_kind(Index::Id(key.0)),
                    }),
                );

                // Now that we've defined a new name in our module we want to
                // copy over all the metadata learned about that item into our
                // module as well, so when the name is used in our module we
                // still retain all metadata about it.
                self.defined_names.insert(*key);
                match key.1 {
                    Ns::Module => {
                        if let Some(info) = parent.module_exports.get(&key.0) {
                            self.module_exports.insert(key.0, info.clone());
                        }
                    }
                    Ns::Type => {
                        if let Some(list) = parent.instance_type_exports.get(&key.0) {
                            self.instance_type_exports.insert(key.0, list.clone());
                        }
                    }
                    // When copying over information about instances we just
                    // interpret everything as information listed inline to
                    // avoid the go-through-the-module-map indirection.
                    Ns::Instance => {
                        let info = match parent.instance_exports.get(&key.0) {
                            Some(InstanceExports::Inline(info)) => info,
                            Some(InstanceExports::Module(module_id)) => {
                                &parent.module_exports[module_id]
                            }
                            None => continue,
                        };
                        self.instance_exports
                            .insert(key.0, InstanceExports::Inline(info.clone()));
                    }
                    _ => {}
                }
            }
        }

        // After we've dealt with inherited identifiers from our parent we can
        // start injecting aliases after local `instance` definitions based on
        // referenced identifiers elsewhere in this module.
        self.iter_and_expand(fields, |me, field| {
            me.inject_aliases(field);
            Ok(true)
        })?;
        assert!(self.to_append.is_empty());

        // And last for our own module we can expand all the export-all
        // annotations since we have all information about nested
        // modules/instances at this point.
        self.iter_and_expand(fields, |me, field| me.expand_export_all(field))?;
        assert!(self.to_append.is_empty());

        // And as a final processing step we recurse into our nested modules and
        // process all of them.
        for field in fields.iter_mut() {
            if let ModuleField::NestedModule(m) = field {
                if let NestedModuleKind::Inline { fields, .. } = &mut m.kind {
                    Expander::default().process(fields, Some(self))?;
                }
            }
        }
        Ok(())
    }

    fn discover_module_names(&mut self, field: &ModuleField<'a>) {
        let (id, info) = match field {
            ModuleField::Import(Import {
                item:
                    ItemSig {
                        id: Some(id),
                        kind:
                            ItemKind::Module(TypeUse {
                                inline: Some(ty), ..
                            }),
                        ..
                    },
                ..
            }) => (id, ExportInfo::from_exports(&ty.exports)),

            ModuleField::NestedModule(NestedModule {
                id: Some(id),
                kind: NestedModuleKind::Inline { fields, .. },
                ..
            }) => {
                // This is a bit of a cop-out at this point, but we don't really
                // have full name resolution info available at this point. As a
                // result we just consider `(export "" (func $foo))` as a
                // candidate that `$foo` is available from this module for
                // referencing. If you do, however, `(export "" (func 0))` and
                // the 0th function is `$foo`, we won't realize that.
                let mut info = ExportInfo::default();
                for field in fields {
                    let export = match field {
                        ModuleField::Export(e) => e,
                        _ => continue,
                    };
                    let (idx, ns) = Ns::from_export(&export.kind);
                    if let Index::Id(id) = idx {
                        info.wat_name_to_export_idx
                            .insert((id.name(), ns), info.export_names.len() as u32);
                    }
                    info.export_names.push((export.name, ns));
                }
                (id, info)
            }

            _ => return,
        };
        self.module_exports.insert(*id, info);
    }

    /// Processes each element of `fields` individually and then inserts all
    /// items in `self.to_append` after that field.
    fn iter_and_expand(
        &mut self,
        fields: &mut Vec<ModuleField<'a>>,
        f: impl Fn(&mut Self, &mut ModuleField<'a>) -> Result<bool, Error>,
    ) -> Result<(), Error> {
        assert!(self.to_append.is_empty());
        let mut cur = 0;
        while cur < fields.len() {
            let keep = f(self, &mut fields[cur])?;
            if keep {
                cur += 1;
            } else {
                fields.remove(cur);
            }
            for new in self.to_append.drain(..) {
                fields.insert(cur, new);
                cur += 1;
            }
        }
        Ok(())
    }

    fn record_missing_name(&mut self, field: &ModuleField<'a>) {
        match field {
            ModuleField::Type(t) => {
                self.record_defined(&t.id, Ns::Type);
                match &t.def {
                    TypeDef::Module(m) => m.record_missing(self),
                    TypeDef::Instance(i) => {
                        i.record_missing(self);
                        if let Some(name) = t.id {
                            self.instance_type_exports.insert(name, i.exports.clone());
                        }
                    }
                    TypeDef::Func(f) => f.record_missing(self),
                    TypeDef::Array(_) | TypeDef::Struct(_) => {}
                }
            }

            ModuleField::Import(i) => {
                self.record_defined(&i.item.id, Ns::from_item(&i.item));
                self.record_item_sig(&i.item);
            }

            ModuleField::Func(f) => {
                self.record_defined(&f.id, Ns::Func);
                self.record_type_use(&f.ty);
                if let FuncKind::Inline { expression, .. } = &f.kind {
                    self.record_expr(expression);
                }
            }

            ModuleField::Memory(m) => {
                self.record_defined(&m.id, Ns::Memory);
            }

            ModuleField::Table(t) => {
                self.record_defined(&t.id, Ns::Table);
            }

            ModuleField::Global(g) => {
                self.record_defined(&g.id, Ns::Global);
                if let GlobalKind::Inline(e) = &g.kind {
                    self.record_expr(e);
                }
            }

            ModuleField::Event(e) => {
                self.record_defined(&e.id, Ns::Event);
                match &e.ty {
                    EventType::Exception(ty) => self.record_type_use(ty),
                }
            }

            ModuleField::Instance(i) => {
                self.record_defined(&i.id, Ns::Instance);
                if let InstanceKind::Inline { items, module, .. } = &i.kind {
                    self.record_missing(module, Ns::Module);
                    for item in items {
                        let (idx, ns) = Ns::from_export(item);
                        self.record_missing(&idx, ns);
                    }
                }
            }

            ModuleField::Start(id) => self.record_missing(id, Ns::Func),

            ModuleField::Export(e) => {
                let (idx, ns) = Ns::from_export(&e.kind);
                self.record_missing(&idx, ns);
            }

            ModuleField::Elem(e) => {
                match &e.kind {
                    ElemKind::Active { table, offset } => {
                        self.record_missing(table, Ns::Table);
                        self.record_expr(offset);
                    }
                    ElemKind::Passive { .. } | ElemKind::Declared { .. } => {}
                }
                match &e.payload {
                    ElemPayload::Indices(elems) => {
                        for idx in elems {
                            self.record_missing(idx, Ns::Func);
                        }
                    }
                    ElemPayload::Exprs { exprs, .. } => {
                        for funcref in exprs {
                            if let Some(idx) = funcref {
                                self.record_missing(idx, Ns::Func);
                            }
                        }
                    }
                }
            }

            ModuleField::Data(d) => {
                if let DataKind::Active { memory, offset } = &d.kind {
                    self.record_missing(memory, Ns::Memory);
                    self.record_expr(offset);
                }
            }

            ModuleField::Alias(a) => {
                let (_idx, ns) = Ns::from_export(&a.kind);
                self.record_defined(&a.id, ns);
            }

            ModuleField::NestedModule(m) => {
                self.record_defined(&m.id, Ns::Module);
            }

            ModuleField::ExportAll(_span, instance) => {
                self.record_missing(&Index::Id(*instance), Ns::Instance);
            }

            ModuleField::Custom(_) => {}
        }
    }
    fn record_type_use(&mut self, item: &TypeUse<'a, impl TypeReference<'a>>) {
        if let Some(idx) = &item.index {
            self.record_missing(idx, Ns::Type);
        }
        if let Some(inline) = &item.inline {
            inline.record_missing(self);
        }
    }

    fn record_item_sig(&mut self, sig: &ItemSig<'a>) {
        match &sig.kind {
            ItemKind::Func(f) | ItemKind::Event(EventType::Exception(f)) => self.record_type_use(f),
            ItemKind::Module(m) => self.record_type_use(m),
            ItemKind::Instance(i) => self.record_type_use(i),
            ItemKind::Table(_) | ItemKind::Memory(_) | ItemKind::Global(_) => {}
        }
    }

    fn record_expr(&mut self, expr: &Expression<'a>) {
        use Instruction::*;
        for instr in expr.instrs.iter() {
            match instr {
                Block(t) | Loop(t) | If(t) | Try(t) => {
                    self.record_type_use(&t.ty);
                }
                FuncBind(b) => {
                    self.record_type_use(&b.ty);
                }
                Call(f) | ReturnCall(f) | RefFunc(f) => self.record_missing(f, Ns::Func),
                CallIndirect(f) | ReturnCallIndirect(f) => {
                    self.record_missing(&f.table, Ns::Table);
                    self.record_type_use(&f.ty);
                }
                GlobalGet(g) | GlobalSet(g) => self.record_missing(g, Ns::Global),
                TableFill(g) | TableSize(g) | TableGrow(g) | TableGet(g) | TableSet(g) => {
                    self.record_missing(&g.dst, Ns::Table)
                }
                TableInit(t) => self.record_missing(&t.table, Ns::Table),
                TableCopy(t) => {
                    self.record_missing(&t.src, Ns::Table);
                    self.record_missing(&t.dst, Ns::Table);
                }
                _ => {}
            }
        }
    }

    fn record_defined(&mut self, name: &Option<Id<'a>>, kind: Ns) {
        if let Some(id) = name {
            self.defined_names.insert((*id, kind));
        }
    }

    fn record_missing(&mut self, name: &Index<'a>, kind: Ns) {
        let id = match name {
            Index::Num(..) => return,
            Index::Id(id) => id,
        };
        self.all_missing.insert((*id, kind));
        let (instance, field) = match self.split_name(id) {
            Some(p) => p,
            None => return,
        };
        self.missing_names
            .entry(instance)
            .or_insert(HashMap::new())
            .insert((field, kind), *id);
    }

    /// Splits an identifier into two halves if it looks like `$a.$b`. Returns
    /// `("a", "b")` in that case.
    fn split_name(&self, id: &Id<'a>) -> Option<(&'a str, &'a str)> {
        let name = id.name();
        let i = name.find('.')?;
        let (instance, field) = (&name[..i], &name[i + 1..]);
        if field.starts_with("$") {
            Some((instance, &field[1..]))
        } else {
            None
        }
    }

    fn inject_aliases(&mut self, field: &ModuleField<'a>) {
        let defined_names = &self.defined_names;
        let (id, span, export_info) = match field {
            // For imported instances, only those with a name and an inline type
            // listed are candidates for expansion with `$a.$b`.
            ModuleField::Import(Import {
                item:
                    ItemSig {
                        id: Some(id),
                        span,
                        kind:
                            ItemKind::Instance(TypeUse {
                                inline: Some(ty), ..
                            }),
                        ..
                    },
                ..
            }) => {
                let map = self.instance_exports.entry(*id).or_insert_with(|| {
                    InstanceExports::Inline(ExportInfo::from_exports(&ty.exports))
                });
                (
                    id,
                    span,
                    match map {
                        InstanceExports::Inline(i) => &*i,
                        // If this instance name was already seen earlier we just
                        // skip expansion for now and let name resolution generate
                        // an error about it.
                        InstanceExports::Module(_) => return,
                    },
                )
            }

            // Of locally defined instances, only those which have a name and
            // instantiate a named module are candidates for expansion with
            // `$a.$b`. This isn't the greatest of situations right now but it
            // should be good enough.
            ModuleField::Instance(Instance {
                span,
                id: Some(id),
                kind:
                    InstanceKind::Inline {
                        module: Index::Id(module_id),
                        ..
                    },
                ..
            }) => match self.module_exports.get(module_id) {
                Some(map) => {
                    self.instance_exports
                        .insert(*id, InstanceExports::Module(*module_id));
                    (id, span, map)
                }
                // If this named module doesn't exist let name resolution later
                // on generate an error about that.
                None => return,
            },
            _ => return,
        };

        // Load the list of names missing for our instance's name. Note that
        // there may be no names missing, so we can just skip that.
        let missing = match self.missing_names.get(id.name()) {
            Some(missing) => missing,
            None => return,
        };
        let mut to_add = missing
            .iter()
            // Skip names defined locally in our module already...
            .filter(|((_, ns), full_name)| !defined_names.contains(&(**full_name, *ns)))
            // ... and otherwise only work with names actually defined in the
            // instance.
            .filter_map(|(key, full_name)| {
                export_info
                    .wat_name_to_export_idx
                    .get(key)
                    .map(move |idx| (&key.1, idx, full_name))
            })
            .collect::<Vec<_>>();

        // Sort the list of aliases to add to ensure that we have deterministic
        // output.
        to_add.sort_by_key(|e| e.1);

        for (ns, index, full_name) in to_add {
            let export_index = Index::Num(*index, *span);
            self.defined_names.insert((*full_name, *ns));
            self.to_append.push(ModuleField::Alias(Alias {
                id: Some(*full_name),
                instance: Some(Index::Id(*id)),
                name: None,
                span: *span,
                kind: ns.to_export_kind(export_index),
            }));
        }
    }

    fn expand_export_all(&mut self, field: &mut ModuleField<'a>) -> Result<bool, Error> {
        match field {
            ModuleField::Type(t) => {
                match &mut t.def {
                    TypeDef::Module(m) => m.expand_export_all(self)?,
                    TypeDef::Instance(i) => i.expand_export_all(self)?,
                    TypeDef::Func(f) => f.expand_export_all(self)?,
                    TypeDef::Array(_) | TypeDef::Struct(_) => {}
                }
                Ok(true)
            }
            ModuleField::Import(t) => {
                self.expand_export_all_item_sig(&mut t.item)?;
                Ok(true)
            }
            ModuleField::ExportAll(span, instance) => {
                let info = match self.instance_exports.get(instance) {
                    Some(InstanceExports::Inline(i)) => i,
                    Some(InstanceExports::Module(i)) => &self.module_exports[i],
                    None => {
                        return Err(Error::new(
                            instance.span(),
                            format!("failed to find instance to inline exports"),
                        ))
                    }
                };
                for (i, (name, ns)) in info.export_names.iter().enumerate() {
                    let index = Index::Num(i as u32, *span);
                    let alias_id = gensym::gen(*span);
                    self.to_append.push(ModuleField::Alias(Alias {
                        id: Some(alias_id),
                        instance: Some(Index::Id(*instance)),
                        name: None,
                        span: *span,
                        kind: ns.to_export_kind(index),
                    }));
                    self.to_append.push(ModuleField::Export(Export {
                        span: *span,
                        name,
                        kind: ns.to_export_kind(Index::Id(alias_id)),
                    }));
                }
                Ok(false)
            }
            _ => Ok(true),
        }
    }

    fn expand_export_all_item_sig(&mut self, item: &mut ItemSig<'a>) -> Result<(), Error> {
        match &mut item.kind {
            ItemKind::Module(t) => self.expand_export_all_type_use(t),
            ItemKind::Instance(t) => self.expand_export_all_type_use(t),
            ItemKind::Func(t) | ItemKind::Event(EventType::Exception(t)) => {
                self.expand_export_all_type_use(t)
            }
            ItemKind::Memory(_) | ItemKind::Table(_) | ItemKind::Global(_) => Ok(()),
        }
    }

    fn expand_export_all_type_use(
        &mut self,
        item: &mut TypeUse<'a, impl TypeReference<'a>>,
    ) -> Result<(), Error> {
        if let Some(t) = &mut item.inline {
            t.expand_export_all(self)?;
        }
        Ok(())
    }
}

#[derive(Default, Clone)]
struct ExportInfo<'a> {
    wat_name_to_export_idx: HashMap<(&'a str, Ns), u32>,
    export_names: Vec<(&'a str, Ns)>,
}

impl<'a> ExportInfo<'a> {
    fn from_exports(exports: &[ExportType<'a>]) -> ExportInfo<'a> {
        let mut wat_name_to_export_idx = HashMap::new();
        let mut export_names = Vec::new();
        for (i, export) in exports.iter().enumerate() {
            let ns = Ns::from_item(&export.item);
            if let Some(id) = export.item.id {
                wat_name_to_export_idx.insert((id.name(), ns), i as u32);
            }
            export_names.push((export.name, ns));
        }
        ExportInfo {
            wat_name_to_export_idx,
            export_names,
        }
    }
}

trait TypeReference<'a> {
    fn record_missing(&self, cx: &mut Expander<'a>);
    fn expand_export_all(&mut self, cx: &mut Expander<'a>) -> Result<(), Error>;
}

impl<'a> TypeReference<'a> for FunctionType<'a> {
    fn record_missing(&self, _cx: &mut Expander<'a>) {}
    fn expand_export_all(&mut self, _cx: &mut Expander<'a>) -> Result<(), Error> {
        Ok(())
    }
}

impl<'a> TypeReference<'a> for ModuleType<'a> {
    fn record_missing(&self, cx: &mut Expander<'a>) {
        for i in self.imports.iter() {
            cx.record_item_sig(&i.item);
        }
        for e in self.exports.iter() {
            cx.record_item_sig(&e.item);
        }
        for (_, name) in self.instance_exports.iter() {
            cx.record_missing(&Index::Id(*name), Ns::Type);
        }
    }
    fn expand_export_all(&mut self, cx: &mut Expander<'a>) -> Result<(), Error> {
        for (_span, instance_ty_name) in self.instance_exports.drain(..) {
            match cx.instance_type_exports.get(&instance_ty_name) {
                Some(exports) => self.exports.extend(exports.iter().cloned()),
                None => {
                    return Err(Error::new(
                        instance_ty_name.span(),
                        format!("failed to find instance type to inline exports from"),
                    ))
                }
            }
        }
        for i in self.imports.iter_mut() {
            cx.expand_export_all_item_sig(&mut i.item)?;
        }
        for e in self.exports.iter_mut() {
            cx.expand_export_all_item_sig(&mut e.item)?;
        }
        Ok(())
    }
}

impl<'a> TypeReference<'a> for InstanceType<'a> {
    fn record_missing(&self, cx: &mut Expander<'a>) {
        for e in self.exports.iter() {
            cx.record_item_sig(&e.item);
        }
    }
    fn expand_export_all(&mut self, cx: &mut Expander<'a>) -> Result<(), Error> {
        for e in self.exports.iter_mut() {
            cx.expand_export_all_item_sig(&mut e.item)?;
        }
        Ok(())
    }
}
