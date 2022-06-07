use crate::component::*;
use crate::core;
use crate::gensym;
use crate::kw;
use crate::token::{Id, Index, Span};
use std::collections::HashMap;
use std::mem;

/// Performs an AST "expansion" pass over the component fields provided.
///
/// This expansion is intended to desugar the AST from various parsed constructs
/// to bits and bobs amenable for name resolution as well as binary encoding.
/// For example `(import "" (func))` is split into a type definition followed by
/// the import referencing that type definition.
///
/// Most forms of AST expansion happen in this file and afterwards the AST will
/// be handed to the name resolution pass which will convert `Index::Id` to
/// `Index::Num` wherever it's found.
pub fn expand(fields: &mut Vec<ComponentField<'_>>) {
    Expander::default().expand_component_fields(fields)
}

#[derive(Default)]
struct Expander<'a> {
    /// Fields, during processing, which should be prepended to the
    /// currently-being-processed field. This should always be empty after
    /// processing is complete.
    to_prepend: Vec<TypeField<'a>>,
    component_fields_to_prepend: Vec<ComponentField<'a>>,

    /// Fields that are appended to the end of the module once everything has
    /// finished.
    component_fields_to_append: Vec<ComponentField<'a>>,
}

impl<'a> Expander<'a> {
    fn expand_component_fields(&mut self, fields: &mut Vec<ComponentField<'a>>) {
        let mut cur = 0;
        while cur < fields.len() {
            self.expand_field(&mut fields[cur]);
            let amt = self.to_prepend.len() + self.component_fields_to_prepend.len();
            fields.splice(cur..cur, self.component_fields_to_prepend.drain(..));
            fields.splice(cur..cur, self.to_prepend.drain(..).map(|f| f.into()));
            cur += 1 + amt;
        }
        fields.extend(self.component_fields_to_append.drain(..));
    }

    fn expand_fields<T>(&mut self, fields: &mut Vec<T>, expand: fn(&mut Self, &mut T))
    where
        T: From<TypeField<'a>>,
    {
        let mut cur = 0;
        while cur < fields.len() {
            expand(self, &mut fields[cur]);
            assert!(self.component_fields_to_prepend.is_empty());
            assert!(self.component_fields_to_append.is_empty());
            let amt = self.to_prepend.len();
            fields.splice(cur..cur, self.to_prepend.drain(..).map(T::from));
            cur += 1 + amt;
        }
    }

    fn expand_field(&mut self, item: &mut ComponentField<'a>) {
        match item {
            ComponentField::Type(t) => self.expand_type_field(t),
            ComponentField::Import(t) => {
                self.expand_item_sig(&mut t.item);
            }
            ComponentField::Component(c) => {
                for name in c.exports.names.drain(..) {
                    self.component_fields_to_append.push(export(
                        c.span,
                        name,
                        DefTypeKind::Component,
                        &mut c.id,
                    ));
                }
                match &mut c.kind {
                    NestedComponentKind::Inline(fields) => expand(fields),
                    NestedComponentKind::Import { import, ty } => {
                        let idx = self.expand_component_type_use(ty);
                        *item = ComponentField::Import(ComponentImport {
                            span: c.span,
                            name: import.name,
                            item: ItemSig {
                                span: c.span,
                                id: c.id,
                                name: None,
                                kind: ItemKind::Component(ComponentTypeUse::Ref(idx)),
                            },
                        });
                    }
                }
            }

            ComponentField::Func(f) => {
                for name in f.exports.names.drain(..) {
                    self.component_fields_to_append.push(export(
                        f.span,
                        name,
                        DefTypeKind::Func,
                        &mut f.id,
                    ));
                }
                match &mut f.kind {
                    ComponentFuncKind::Import { import, ty } => {
                        let idx = self.expand_component_type_use(ty);
                        *item = ComponentField::Import(ComponentImport {
                            span: f.span,
                            name: import.name,
                            item: ItemSig {
                                span: f.span,
                                id: f.id,
                                name: None,
                                kind: ItemKind::Func(ComponentTypeUse::Ref(idx)),
                            },
                        });
                    }
                    ComponentFuncKind::Inline { body } => match body {
                        ComponentFuncBody::CanonLift(lift) => {
                            self.expand_component_type_use(&mut lift.type_);
                        }
                        ComponentFuncBody::CanonLower(_) => {}
                    },
                }
            }

            ComponentField::Module(m) => {
                for name in m.exports.names.drain(..) {
                    self.component_fields_to_append.push(export(
                        m.span,
                        name,
                        DefTypeKind::Module,
                        &mut m.id,
                    ));
                }
                match &mut m.kind {
                    // inline modules are expanded later during resolution
                    ModuleKind::Inline { .. } => {}
                    ModuleKind::Import { import, ty } => {
                        let idx = self.expand_component_type_use(ty);
                        *item = ComponentField::Import(ComponentImport {
                            span: m.span,
                            name: import.name,
                            item: ItemSig {
                                span: m.span,
                                id: m.id,
                                name: None,
                                kind: ItemKind::Module(ComponentTypeUse::Ref(idx)),
                            },
                        });
                    }
                }
            }

            ComponentField::Instance(i) => {
                for name in i.exports.names.drain(..) {
                    self.component_fields_to_append.push(export(
                        i.span,
                        name,
                        DefTypeKind::Instance,
                        &mut i.id,
                    ));
                }
                match &mut i.kind {
                    InstanceKind::Import { import, ty } => {
                        let idx = self.expand_component_type_use(ty);
                        *item = ComponentField::Import(ComponentImport {
                            span: i.span,
                            name: import.name,
                            item: ItemSig {
                                span: i.span,
                                id: i.id,
                                name: None,
                                kind: ItemKind::Instance(ComponentTypeUse::Ref(idx)),
                            },
                        });
                    }
                    InstanceKind::Module { args, .. } => {
                        for arg in args {
                            self.expand_module_arg(&mut arg.arg);
                        }
                    }
                    InstanceKind::Component { args, .. } => {
                        for arg in args {
                            self.expand_component_arg(&mut arg.arg);
                        }
                    }
                    InstanceKind::BundleOfExports { .. }
                    | InstanceKind::BundleOfComponentExports { .. } => {}
                }
            }

            ComponentField::Export(e) => {
                self.expand_component_arg(&mut e.arg);
            }

            ComponentField::Alias(_) | ComponentField::Start(_) => {}
        }

        fn export<'a>(
            span: Span,
            name: &'a str,
            kind: DefTypeKind,
            id: &mut Option<Id<'a>>,
        ) -> ComponentField<'a> {
            let id = gensym::fill(span, id);
            ComponentField::Export(ComponentExport {
                span,
                name,
                arg: ComponentArg::Def(ItemRef {
                    idx: Index::Id(id),
                    kind,
                    export_names: Vec::new(),
                }),
            })
        }
    }

    fn expand_type_field(&mut self, field: &mut TypeField<'a>) {
        match &mut field.def {
            ComponentTypeDef::DefType(t) => self.expand_deftype(t),
            ComponentTypeDef::InterType(t) => self.expand_intertype(t),
        }
        let name = gensym::fill(field.span, &mut field.id);
        let name = Index::Id(name);
        match &field.def {
            ComponentTypeDef::DefType(DefType::Func(t)) => t.key().insert(self, name),
            ComponentTypeDef::DefType(DefType::Module(t)) => t.key().insert(self, name),
            ComponentTypeDef::DefType(DefType::Component(t)) => t.key().insert(self, name),
            ComponentTypeDef::DefType(DefType::Instance(t)) => t.key().insert(self, name),
            ComponentTypeDef::DefType(DefType::Value(t)) => t.key().insert(self, name),
            ComponentTypeDef::InterType(t) => t.key().insert(self, name),
        }
    }

    fn expand_deftype(&mut self, ty: &mut DefType<'a>) {
        match ty {
            DefType::Func(t) => self.expand_func_ty(t),
            DefType::Module(m) => self.expand_module_ty(m),
            DefType::Component(c) => self.expand_component_ty(c),
            DefType::Instance(i) => self.expand_instance_ty(i),
            DefType::Value(v) => self.expand_value_ty(v),
        }
    }

    fn expand_func_ty(&mut self, ty: &mut ComponentFunctionType<'a>) {
        for param in ty.params.iter_mut() {
            self.expand_intertype_ref(&mut param.type_);
        }
        self.expand_intertype_ref(&mut ty.result);
    }

    fn expand_module_ty(&mut self, ty: &mut ModuleType<'a>) {
        use crate::core::resolve::types::{FuncKey, TypeKey, TypeReference};

        // Note that this is a custom implementation from everything else in
        // this file since this is using core wasm types instead of component
        // types, so a small part of the core wasm expansion process is
        // inlined here to handle the `TypeUse` from core wasm.

        let mut func_type_to_idx = HashMap::new();
        let mut to_prepend = Vec::new();
        let mut i = 0;
        while i < ty.defs.len() {
            match &mut ty.defs[i] {
                ModuleTypeDef::Type(ty) => match &ty.def {
                    core::TypeDef::Func(f) => {
                        let id = gensym::fill(ty.span, &mut ty.id);
                        func_type_to_idx.insert(f.key(), Index::Id(id));
                    }
                    core::TypeDef::Struct(_) => {}
                    core::TypeDef::Array(_) => {}
                },
                ModuleTypeDef::Import(ty) => {
                    expand_sig(&mut ty.item, &mut to_prepend, &mut func_type_to_idx);
                }
                ModuleTypeDef::Export(_, item) => {
                    expand_sig(item, &mut to_prepend, &mut func_type_to_idx);
                }
            }
            ty.defs.splice(i..i, to_prepend.drain(..));
            i += 1;
        }

        fn expand_sig<'a>(
            item: &mut core::ItemSig<'a>,
            to_prepend: &mut Vec<ModuleTypeDef<'a>>,
            func_type_to_idx: &mut HashMap<FuncKey<'a>, Index<'a>>,
        ) {
            match &mut item.kind {
                core::ItemKind::Func(t) | core::ItemKind::Tag(core::TagType::Exception(t)) => {
                    // If the index is already filled in then this is skipped
                    if t.index.is_some() {
                        return;
                    }

                    // Otherwise the inline type information is used to
                    // generate a type into this module if necessary. If the
                    // function type already exists we reuse the same key,
                    // otherwise a fresh type definition is created and we use
                    // that one instead.
                    let ty = t.inline.take().unwrap_or_default();
                    let key = ty.key();
                    if let Some(idx) = func_type_to_idx.get(&key) {
                        t.index = Some(idx.clone());
                        return;
                    }
                    let id = gensym::gen(item.span);
                    to_prepend.push(ModuleTypeDef::Type(core::Type {
                        span: item.span,
                        id: Some(id),
                        name: None,
                        def: key.to_def(item.span),
                    }));
                    let idx = Index::Id(id);
                    t.index = Some(idx);
                }
                core::ItemKind::Global(_)
                | core::ItemKind::Table(_)
                | core::ItemKind::Memory(_) => {}
            }
        }
    }

    fn expand_component_ty(&mut self, ty: &mut ComponentType<'a>) {
        Expander::default().expand_fields(&mut ty.fields, |e, field| match field {
            ComponentTypeField::Type(t) => e.expand_type_field(t),
            ComponentTypeField::Alias(_) => {}
            ComponentTypeField::Export(t) => e.expand_item_sig(&mut t.item),
            ComponentTypeField::Import(t) => e.expand_item_sig(&mut t.item),
        })
    }

    fn expand_instance_ty(&mut self, ty: &mut InstanceType<'a>) {
        Expander::default().expand_fields(&mut ty.fields, |e, field| match field {
            InstanceTypeField::Type(t) => e.expand_type_field(t),
            InstanceTypeField::Alias(_) => {}
            InstanceTypeField::Export(t) => e.expand_item_sig(&mut t.item),
        })
    }

    fn expand_item_sig(&mut self, sig: &mut ItemSig<'a>) {
        match &mut sig.kind {
            ItemKind::Component(t) => self.expand_component_type_use(t),
            ItemKind::Module(t) => self.expand_component_type_use(t),
            ItemKind::Instance(t) => self.expand_component_type_use(t),
            ItemKind::Value(t) => self.expand_component_type_use(t),
            ItemKind::Func(t) => self.expand_component_type_use(t),
        };
    }

    fn expand_value_ty(&mut self, ty: &mut ValueType<'a>) {
        self.expand_intertype_ref(&mut ty.value_type);
    }

    fn expand_intertype(&mut self, ty: &mut InterType<'a>) {
        match ty {
            InterType::Primitive(_) | InterType::Flags(_) | InterType::Enum(_) => {}
            InterType::Record(r) => {
                for field in r.fields.iter_mut() {
                    self.expand_intertype_ref(&mut field.type_);
                }
            }
            InterType::Variant(v) => {
                for case in v.cases.iter_mut() {
                    self.expand_intertype_ref(&mut case.type_);
                }
            }
            InterType::List(t) => {
                self.expand_intertype_ref(&mut t.element);
            }
            InterType::Tuple(t) => {
                for field in t.fields.iter_mut() {
                    self.expand_intertype_ref(field);
                }
            }
            InterType::Union(u) => {
                for arm in u.arms.iter_mut() {
                    self.expand_intertype_ref(arm);
                }
            }
            InterType::Option(t) => {
                self.expand_intertype_ref(&mut t.element);
            }
            InterType::Expected(e) => {
                self.expand_intertype_ref(&mut e.ok);
                self.expand_intertype_ref(&mut e.err);
            }
        }
    }

    fn expand_intertype_ref(&mut self, ty: &mut InterTypeRef<'a>) {
        let inline = match ty {
            InterTypeRef::Primitive(_) | InterTypeRef::Ref(_) => return,
            InterTypeRef::Inline(inline) => {
                mem::replace(inline, InterType::Primitive(Primitive::Unit))
            }
        };
        // If this inline type has already been defined within this context
        // then reuse the previously defined type to avoid injecting too many
        // types into the type index space.
        if let Some(idx) = inline.key().lookup(self) {
            *ty = InterTypeRef::Ref(idx);
            return;
        }
        // And if this type isn't already defined we append it to the index
        // space with a fresh and unique name.
        let span = Span::from_offset(0); // FIXME(#613): don't manufacture
        let id = gensym::gen(span);
        self.to_prepend.push(TypeField {
            span,
            id: Some(id),
            name: None,
            def: inline.into_def(),
        });
        let idx = Index::Id(id);
        *ty = InterTypeRef::Ref(idx);
    }

    fn expand_component_type_use<T>(
        &mut self,
        item: &mut ComponentTypeUse<'a, T>,
    ) -> ItemRef<'a, kw::r#type>
    where
        T: TypeReference<'a>,
    {
        let span = Span::from_offset(0); // FIXME(#613): don't manufacture
        let dummy = ComponentTypeUse::Ref(ItemRef {
            idx: Index::Num(0, span),
            kind: kw::r#type(span),
            export_names: Vec::new(),
        });
        let mut inline = match mem::replace(item, dummy) {
            // If this type-use was already a reference to an existing type
            // then we put it back the way it was and return the corresponding
            // index.
            ComponentTypeUse::Ref(idx) => {
                *item = ComponentTypeUse::Ref(idx.clone());
                return idx;
            }

            // ... otherwise with an inline type definition we go into
            // processing below.
            ComponentTypeUse::Inline(inline) => inline,
        };
        inline.expand(self);

        // If this inline type has already been defined within this context
        // then reuse the previously defined type to avoid injecting too many
        // types into the type index space.
        if let Some(idx) = inline.key().lookup(self) {
            let ret = ItemRef {
                idx,
                kind: kw::r#type(span),
                export_names: Vec::new(),
            };
            *item = ComponentTypeUse::Ref(ret.clone());
            return ret;
        }

        // And if this type isn't already defined we append it to the index
        // space with a fresh and unique name.
        let id = gensym::gen(span);
        self.to_prepend.push(TypeField {
            span,
            id: Some(id),
            name: None,
            def: inline.into_def(),
        });
        let idx = Index::Id(id);
        let ret = ItemRef {
            idx,
            kind: kw::r#type(span),
            export_names: Vec::new(),
        };
        *item = ComponentTypeUse::Ref(ret.clone());
        return ret;
    }

    fn expand_module_arg(&mut self, arg: &mut ModuleArg<'a>) {
        let (span, args) = match arg {
            ModuleArg::Def(_) => return,
            ModuleArg::BundleOfExports(span, exports) => (*span, mem::take(exports)),
        };
        let id = gensym::gen(span);
        self.component_fields_to_prepend
            .push(ComponentField::Instance(Instance {
                span,
                id: Some(id),
                name: None,
                exports: Default::default(),
                kind: InstanceKind::BundleOfExports { args },
            }));
        *arg = ModuleArg::Def(ItemRef {
            idx: Index::Id(id),
            kind: kw::instance(span),
            export_names: Vec::new(),
        });
    }

    fn expand_component_arg(&mut self, arg: &mut ComponentArg<'a>) {
        let (span, args) = match arg {
            ComponentArg::Def(_) => return,
            ComponentArg::BundleOfExports(span, exports) => (*span, mem::take(exports)),
        };
        let id = gensym::gen(span);
        self.component_fields_to_prepend
            .push(ComponentField::Instance(Instance {
                span,
                id: Some(id),
                name: None,
                exports: Default::default(),
                kind: InstanceKind::BundleOfComponentExports { args },
            }));
        *arg = ComponentArg::Def(ItemRef {
            idx: Index::Id(id),
            kind: DefTypeKind::Instance,
            export_names: Vec::new(),
        });
    }
}

trait TypeReference<'a> {
    type Key: TypeKey<'a>;
    fn key(&self) -> Self::Key;
    fn expand(&mut self, cx: &mut Expander<'a>);
    fn into_def(self) -> ComponentTypeDef<'a>;
}

impl<'a> TypeReference<'a> for InterType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_intertype(self)
    }

    fn into_def(self) -> ComponentTypeDef<'a> {
        ComponentTypeDef::InterType(self)
    }
}

impl<'a> TypeReference<'a> for ComponentType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_component_ty(self)
    }

    fn into_def(self) -> ComponentTypeDef<'a> {
        ComponentTypeDef::DefType(DefType::Component(self))
    }
}

impl<'a> TypeReference<'a> for ModuleType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_module_ty(self)
    }

    fn into_def(self) -> ComponentTypeDef<'a> {
        ComponentTypeDef::DefType(DefType::Module(self))
    }
}

impl<'a> TypeReference<'a> for InstanceType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_instance_ty(self)
    }

    fn into_def(self) -> ComponentTypeDef<'a> {
        ComponentTypeDef::DefType(DefType::Instance(self))
    }
}

impl<'a> TypeReference<'a> for ComponentFunctionType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_func_ty(self)
    }

    fn into_def(self) -> ComponentTypeDef<'a> {
        ComponentTypeDef::DefType(DefType::Func(self))
    }
}

impl<'a> TypeReference<'a> for ValueType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_value_ty(self)
    }

    fn into_def(self) -> ComponentTypeDef<'a> {
        ComponentTypeDef::DefType(DefType::Value(self))
    }
}

trait TypeKey<'a> {
    fn lookup(&self, cx: &Expander<'a>) -> Option<Index<'a>>;
    fn insert(&self, cx: &mut Expander<'a>, id: Index<'a>);
}

struct Todo;

impl<'a> TypeKey<'a> for Todo {
    fn lookup(&self, _cx: &Expander<'a>) -> Option<Index<'a>> {
        None
    }
    fn insert(&self, cx: &mut Expander<'a>, id: Index<'a>) {
        drop((cx, id));
    }
}
