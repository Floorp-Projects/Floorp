use crate::component::*;
use crate::core;
use crate::kw;
use crate::names::Namespace;
use crate::token::{Id, Index};
use crate::Error;

/// Resolve the fields of a component and everything nested within it, changing
/// `Index::Id` to `Index::Num` and expanding alias syntax sugar.
pub fn resolve(component: &mut Component<'_>) -> Result<(), Error> {
    let fields = match &mut component.kind {
        ComponentKind::Text(fields) => fields,
        ComponentKind::Binary(_) => return Ok(()),
    };
    let mut resolver = Resolver::default();
    resolver.fields(component.id, fields)
}

#[derive(Default)]
struct Resolver<'a> {
    stack: Vec<ComponentState<'a>>,

    // When a name refers to a definition in an outer scope, we'll need to
    // insert an outer alias before it. This collects the aliases to be
    // inserted during resolution.
    aliases_to_insert: Vec<Alias<'a>>,
}

/// Context structure used to perform name resolution.
#[derive(Default)]
struct ComponentState<'a> {
    id: Option<Id<'a>>,

    // Namespaces within each component. Note that each namespace carries
    // with it information about the signature of the item in that namespace.
    // The signature is later used to synthesize the type of a component and
    // inject type annotations if necessary.
    funcs: Namespace<'a>,
    globals: Namespace<'a>,
    tables: Namespace<'a>,
    memories: Namespace<'a>,
    types: Namespace<'a>,
    tags: Namespace<'a>,
    instances: Namespace<'a>,
    modules: Namespace<'a>,
    components: Namespace<'a>,
    values: Namespace<'a>,
}

impl<'a> ComponentState<'a> {
    fn new(id: Option<Id<'a>>) -> ComponentState<'a> {
        ComponentState {
            id,
            ..ComponentState::default()
        }
    }
}

impl<'a> Resolver<'a> {
    fn current(&mut self) -> &mut ComponentState<'a> {
        self.stack
            .last_mut()
            .expect("should have at least one component state")
    }

    fn fields(
        &mut self,
        id: Option<Id<'a>>,
        fields: &mut Vec<ComponentField<'a>>,
    ) -> Result<(), Error> {
        self.stack.push(ComponentState::new(id));
        self.resolve_prepending_aliases(fields, Resolver::field, ComponentState::register)?;
        self.stack.pop();
        Ok(())
    }

    fn resolve_prepending_aliases<T>(
        &mut self,
        fields: &mut Vec<T>,
        resolve: fn(&mut Self, &mut T) -> Result<(), Error>,
        register: fn(&mut ComponentState<'a>, &T) -> Result<(), Error>,
    ) -> Result<(), Error>
    where
        T: From<Alias<'a>>,
    {
        assert!(self.aliases_to_insert.is_empty());

        // Iterate through the fields of the component. We use an index
        // instead of an iterator because we'll be inserting aliases
        // as we go.
        let mut i = 0;
        while i < fields.len() {
            // Resolve names within the field.
            resolve(self, &mut fields[i])?;

            // Name resolution may have emitted some aliases. Insert them before
            // the current definition.
            let amt = self.aliases_to_insert.len();
            fields.splice(i..i, self.aliases_to_insert.drain(..).map(T::from));
            i += amt;

            // Definitions can't refer to themselves or to definitions that appear
            // later in the format. Now that we're done resolving this field,
            // assign it an index for later defintions to refer to.
            register(self.current(), &fields[i])?;

            i += 1;
        }

        Ok(())
    }

    fn field(&mut self, field: &mut ComponentField<'a>) -> Result<(), Error> {
        match field {
            ComponentField::Import(i) => self.item_sig(&mut i.item),

            ComponentField::Type(t) => self.type_field(t),

            ComponentField::Func(f) => {
                let body = match &mut f.kind {
                    ComponentFuncKind::Import { .. } => return Ok(()),
                    ComponentFuncKind::Inline { body } => body,
                };
                let opts = match body {
                    ComponentFuncBody::CanonLift(lift) => {
                        self.type_use(&mut lift.type_)?;
                        self.item_ref(&mut lift.func)?;
                        &mut lift.opts
                    }
                    ComponentFuncBody::CanonLower(lower) => {
                        self.item_ref(&mut lower.func)?;
                        &mut lower.opts
                    }
                };
                for opt in opts {
                    match opt {
                        CanonOpt::StringUtf8
                        | CanonOpt::StringUtf16
                        | CanonOpt::StringLatin1Utf16 => {}
                        CanonOpt::Into(instance) => {
                            self.item_ref(instance)?;
                        }
                    }
                }
                Ok(())
            }

            ComponentField::Instance(i) => match &mut i.kind {
                InstanceKind::Module { module, args } => {
                    self.item_ref(module)?;
                    for arg in args {
                        match &mut arg.arg {
                            ModuleArg::Def(def) => {
                                self.item_ref(def)?;
                            }
                            ModuleArg::BundleOfExports(..) => {
                                unreachable!("should be expanded already");
                            }
                        }
                    }
                    Ok(())
                }
                InstanceKind::Component { component, args } => {
                    self.item_ref(component)?;
                    for arg in args {
                        match &mut arg.arg {
                            ComponentArg::Def(def) => {
                                self.item_ref(def)?;
                            }
                            ComponentArg::BundleOfExports(..) => {
                                unreachable!("should be expanded already")
                            }
                        }
                    }
                    Ok(())
                }
                InstanceKind::BundleOfExports { args } => {
                    for arg in args {
                        self.item_ref(&mut arg.index)?;
                    }
                    Ok(())
                }
                InstanceKind::BundleOfComponentExports { args } => {
                    for arg in args {
                        self.arg(&mut arg.arg)?;
                    }
                    Ok(())
                }
                InstanceKind::Import { .. } => {
                    unreachable!("should be removed by expansion")
                }
            },
            ComponentField::Module(m) => {
                match &mut m.kind {
                    ModuleKind::Inline { fields } => {
                        crate::core::resolve::resolve(fields)?;
                    }

                    ModuleKind::Import { .. } => {
                        unreachable!("should be expanded already")
                    }
                }

                Ok(())
            }
            ComponentField::Component(c) => match &mut c.kind {
                NestedComponentKind::Import { .. } => {
                    unreachable!("should be expanded already")
                }
                NestedComponentKind::Inline(fields) => self.fields(c.id, fields),
            },
            ComponentField::Alias(a) => self.alias(a),

            ComponentField::Start(s) => {
                self.item_ref(&mut s.func)?;
                for arg in s.args.iter_mut() {
                    self.item_ref(arg)?;
                }
                Ok(())
            }

            ComponentField::Export(e) => {
                self.arg(&mut e.arg)?;
                Ok(())
            }
        }
    }

    fn item_sig(&mut self, sig: &mut ItemSig<'a>) -> Result<(), Error> {
        match &mut sig.kind {
            ItemKind::Component(t) => self.type_use(t),
            ItemKind::Module(t) => self.type_use(t),
            ItemKind::Instance(t) => self.type_use(t),
            ItemKind::Func(t) => self.type_use(t),
            ItemKind::Value(t) => self.type_use(t),
        }
    }

    fn alias(&mut self, alias: &mut Alias<'a>) -> Result<(), Error> {
        match &mut alias.target {
            AliasTarget::Export {
                instance,
                export: _,
            } => self.resolve_ns(instance, Ns::Instance),
            AliasTarget::Outer { outer, index } => {
                // Short-circuit when both indices are already resolved as this
                // helps to write tests for invalid modules where wasmparser should
                // be the one returning the error.
                if let Index::Num(..) = outer {
                    if let Index::Num(..) = index {
                        return Ok(());
                    }
                }

                // Resolve `outer`, and compute the depth at which to look up
                // `index`.
                let depth = match outer {
                    Index::Id(id) => {
                        let mut depth = 0;
                        for resolver in self.stack.iter_mut().rev() {
                            if resolver.id == Some(*id) {
                                break;
                            }
                            depth += 1;
                        }
                        if depth as usize == self.stack.len() {
                            return Err(Error::new(
                                alias.span,
                                format!("outer component `{}` not found", id.name()),
                            ));
                        }
                        depth
                    }
                    Index::Num(n, _span) => *n,
                };
                *outer = Index::Num(depth, alias.span);
                if depth as usize >= self.stack.len() {
                    return Err(Error::new(
                        alias.span,
                        format!("component depth of `{}` is too large", depth),
                    ));
                }

                // Resolve `index` within the computed scope depth.
                let ns = match alias.kind {
                    AliasKind::Module => Ns::Module,
                    AliasKind::Component => Ns::Component,
                    AliasKind::Instance => Ns::Instance,
                    AliasKind::Value => Ns::Value,
                    AliasKind::ExportKind(kind) => kind.into(),
                };
                let computed = self.stack.len() - 1 - depth as usize;
                self.stack[computed].resolve(ns, index)?;

                Ok(())
            }
        }
    }

    fn arg(&mut self, arg: &mut ComponentArg<'a>) -> Result<(), Error> {
        match arg {
            ComponentArg::Def(item_ref) => {
                self.item_ref(item_ref)?;
            }
            ComponentArg::BundleOfExports(..) => unreachable!("should be expanded already"),
        }
        Ok(())
    }

    fn type_use<T>(&mut self, ty: &mut ComponentTypeUse<'a, T>) -> Result<(), Error> {
        let item = match ty {
            ComponentTypeUse::Ref(r) => r,
            ComponentTypeUse::Inline(_) => {
                unreachable!("inline type-use should be expanded by now")
            }
        };
        self.item_ref(item)
    }

    fn intertype(&mut self, ty: &mut InterType<'a>) -> Result<(), Error> {
        match ty {
            InterType::Primitive(_) => {}
            InterType::Flags(_) => {}
            InterType::Enum(_) => {}
            InterType::Record(r) => {
                for field in r.fields.iter_mut() {
                    self.intertype_ref(&mut field.type_)?;
                }
            }
            InterType::Variant(v) => {
                for case in v.cases.iter_mut() {
                    self.intertype_ref(&mut case.type_)?;
                }
            }
            InterType::List(l) => {
                self.intertype_ref(&mut *l.element)?;
            }
            InterType::Tuple(t) => {
                for field in t.fields.iter_mut() {
                    self.intertype_ref(field)?;
                }
            }
            InterType::Union(t) => {
                for arm in t.arms.iter_mut() {
                    self.intertype_ref(arm)?;
                }
            }
            InterType::Option(o) => {
                self.intertype_ref(&mut *o.element)?;
            }
            InterType::Expected(r) => {
                self.intertype_ref(&mut *r.ok)?;
                self.intertype_ref(&mut *r.err)?;
            }
        }
        Ok(())
    }

    fn intertype_ref(&mut self, ty: &mut InterTypeRef<'a>) -> Result<(), Error> {
        match ty {
            InterTypeRef::Primitive(_) => Ok(()),
            InterTypeRef::Ref(idx) => self.resolve_ns(idx, Ns::Type),
            InterTypeRef::Inline(_) => unreachable!("should be expanded by now"),
        }
    }

    fn type_field(&mut self, field: &mut TypeField<'a>) -> Result<(), Error> {
        match &mut field.def {
            ComponentTypeDef::DefType(DefType::Func(f)) => {
                for param in f.params.iter_mut() {
                    self.intertype_ref(&mut param.type_)?;
                }
                self.intertype_ref(&mut f.result)?;
            }
            ComponentTypeDef::DefType(DefType::Module(m)) => {
                self.stack.push(ComponentState::new(field.id));
                self.moduletype(m)?;
                self.stack.pop();
            }
            ComponentTypeDef::DefType(DefType::Component(c)) => {
                self.stack.push(ComponentState::new(field.id));
                self.nested_component_type(c)?;
                self.stack.pop();
            }
            ComponentTypeDef::DefType(DefType::Instance(i)) => {
                self.stack.push(ComponentState::new(field.id));
                self.instance_type(i)?;
                self.stack.pop();
            }
            ComponentTypeDef::DefType(DefType::Value(v)) => {
                self.intertype_ref(&mut v.value_type)?
            }
            ComponentTypeDef::InterType(i) => self.intertype(i)?,
        }
        Ok(())
    }

    fn nested_component_type(&mut self, c: &mut ComponentType<'a>) -> Result<(), Error> {
        self.resolve_prepending_aliases(
            &mut c.fields,
            |resolver, field| match field {
                ComponentTypeField::Alias(alias) => resolver.alias(alias),
                ComponentTypeField::Type(ty) => resolver.type_field(ty),
                ComponentTypeField::Import(import) => resolver.item_sig(&mut import.item),
                ComponentTypeField::Export(export) => resolver.item_sig(&mut export.item),
            },
            |state, field| {
                match field {
                    ComponentTypeField::Alias(alias) => {
                        state.register_alias(alias)?;
                    }
                    ComponentTypeField::Type(ty) => {
                        state.types.register(ty.id, "type")?;
                    }
                    // Only the type namespace is populated within the component type
                    // namespace so these are ignored here.
                    ComponentTypeField::Import(_) | ComponentTypeField::Export(_) => {}
                }
                Ok(())
            },
        )
    }

    fn instance_type(&mut self, c: &mut InstanceType<'a>) -> Result<(), Error> {
        self.resolve_prepending_aliases(
            &mut c.fields,
            |resolver, field| match field {
                InstanceTypeField::Alias(alias) => resolver.alias(alias),
                InstanceTypeField::Type(ty) => resolver.type_field(ty),
                InstanceTypeField::Export(export) => resolver.item_sig(&mut export.item),
            },
            |state, field| {
                match field {
                    InstanceTypeField::Alias(alias) => {
                        state.register_alias(alias)?;
                    }
                    InstanceTypeField::Type(ty) => {
                        state.types.register(ty.id, "type")?;
                    }
                    InstanceTypeField::Export(_export) => {}
                }
                Ok(())
            },
        )
    }

    fn item_ref<K>(&mut self, item: &mut ItemRef<'a, K>) -> Result<(), Error>
    where
        K: Into<Ns> + Copy,
    {
        let last_ns = item.kind.into();

        // If there are no extra `export_names` listed then this is a reference to
        // something defined within this component's index space, so resolve as
        // necessary.
        if item.export_names.is_empty() {
            self.resolve_ns(&mut item.idx, last_ns)?;
            return Ok(());
        }

        // ... otherwise the `index` of `item` refers to an intance and the
        // `export_names` refer to recursive exports from this item. Resolve the
        // instance locally and then process the export names one at a time,
        // injecting aliases as necessary.
        let mut index = item.idx.clone();
        self.resolve_ns(&mut index, Ns::Instance)?;
        let span = item.idx.span();
        for (pos, export_name) in item.export_names.iter().enumerate() {
            // The last name is in the namespace of the reference. All others are
            // instances.
            let ns = if pos == item.export_names.len() - 1 {
                last_ns
            } else {
                Ns::Instance
            };

            // Record an outer alias to be inserted in front of the current
            // definition.
            let mut alias = Alias {
                span,
                id: None,
                name: None,
                target: AliasTarget::Export {
                    instance: index,
                    export: export_name,
                },
                kind: match ns {
                    Ns::Module => AliasKind::Module,
                    Ns::Component => AliasKind::Component,
                    Ns::Instance => AliasKind::Instance,
                    Ns::Value => AliasKind::Value,
                    Ns::Func => AliasKind::ExportKind(core::ExportKind::Func),
                    Ns::Table => AliasKind::ExportKind(core::ExportKind::Table),
                    Ns::Global => AliasKind::ExportKind(core::ExportKind::Global),
                    Ns::Memory => AliasKind::ExportKind(core::ExportKind::Memory),
                    Ns::Tag => AliasKind::ExportKind(core::ExportKind::Tag),
                    Ns::Type => AliasKind::ExportKind(core::ExportKind::Type),
                },
            };

            index = Index::Num(self.current().register_alias(&mut alias)?, span);
            self.aliases_to_insert.push(alias);
        }
        item.idx = index;
        item.export_names = Vec::new();

        Ok(())
    }

    fn resolve_ns(&mut self, idx: &mut Index<'a>, ns: Ns) -> Result<(), Error> {
        // Perform resolution on a local clone walking up the stack of components
        // that we have. Note that a local clone is used since we don't want to use
        // the parent's resolved index if a parent matches, instead we want to use
        // the index of the alias that we will automatically insert.
        let mut idx_clone = idx.clone();
        for (depth, resolver) in self.stack.iter_mut().rev().enumerate() {
            let depth = depth as u32;
            let found = match resolver.resolve(ns, &mut idx_clone) {
                Ok(idx) => idx,
                // Try the next parent
                Err(_) => continue,
            };

            // If this is the current component then no extra alias is necessary, so
            // return success.
            if depth == 0 {
                *idx = idx_clone;
                return Ok(());
            }
            let id = match idx {
                Index::Id(id) => id.clone(),
                Index::Num(..) => unreachable!(),
            };

            // When resolution succeeds in a parent then an outer alias is
            // automatically inserted here in this component.
            let span = idx.span();
            let mut alias = Alias {
                span,
                id: Some(id),
                name: None,
                target: AliasTarget::Outer {
                    outer: Index::Num(depth, span),
                    index: Index::Num(found, span),
                },
                kind: match ns {
                    Ns::Module => AliasKind::Module,
                    Ns::Component => AliasKind::Component,
                    Ns::Instance => AliasKind::Instance,
                    Ns::Value => AliasKind::Value,
                    Ns::Func => AliasKind::ExportKind(core::ExportKind::Func),
                    Ns::Table => AliasKind::ExportKind(core::ExportKind::Table),
                    Ns::Global => AliasKind::ExportKind(core::ExportKind::Global),
                    Ns::Memory => AliasKind::ExportKind(core::ExportKind::Memory),
                    Ns::Tag => AliasKind::ExportKind(core::ExportKind::Tag),
                    Ns::Type => AliasKind::ExportKind(core::ExportKind::Type),
                },
            };
            let local_index = self.current().register_alias(&mut alias)?;
            self.aliases_to_insert.push(alias);
            *idx = Index::Num(local_index, span);
            return Ok(());
        }

        // If resolution in any parent failed then simply return the error from our
        // local namespace
        self.current().resolve(ns, idx)?;
        unreachable!()
    }

    fn moduletype(&mut self, ty: &mut ModuleType<'_>) -> Result<(), Error> {
        let mut types = Namespace::default();
        for def in ty.defs.iter_mut() {
            match def {
                ModuleTypeDef::Type(t) => {
                    types.register(t.id, "type")?;
                }
                ModuleTypeDef::Import(t) => resolve_item_sig(&mut t.item, &types)?,
                ModuleTypeDef::Export(_, t) => resolve_item_sig(t, &types)?,
            }
        }
        return Ok(());

        fn resolve_item_sig<'a>(
            sig: &mut core::ItemSig<'a>,
            names: &Namespace<'a>,
        ) -> Result<(), Error> {
            match &mut sig.kind {
                core::ItemKind::Func(ty) | core::ItemKind::Tag(core::TagType::Exception(ty)) => {
                    let idx = ty.index.as_mut().expect("index should be filled in");
                    names.resolve(idx, "type")?;
                }
                core::ItemKind::Memory(_)
                | core::ItemKind::Global(_)
                | core::ItemKind::Table(_) => {}
            }
            Ok(())
        }
    }
}

impl<'a> ComponentState<'a> {
    fn resolve(&mut self, ns: Ns, idx: &mut Index<'a>) -> Result<u32, Error> {
        match ns {
            Ns::Func => self.funcs.resolve(idx, "func"),
            Ns::Table => self.tables.resolve(idx, "table"),
            Ns::Global => self.globals.resolve(idx, "global"),
            Ns::Memory => self.memories.resolve(idx, "memory"),
            Ns::Tag => self.tags.resolve(idx, "tag"),
            Ns::Type => self.types.resolve(idx, "type"),
            Ns::Component => self.components.resolve(idx, "component"),
            Ns::Module => self.modules.resolve(idx, "module"),
            Ns::Instance => self.instances.resolve(idx, "instance"),
            Ns::Value => self.values.resolve(idx, "instance"),
        }
    }

    /// Assign an index to the given field.
    fn register(&mut self, item: &ComponentField<'a>) -> Result<(), Error> {
        match item {
            ComponentField::Import(i) => match &i.item.kind {
                ItemKind::Module(_) => self.modules.register(i.item.id, "module")?,
                ItemKind::Component(_) => self.components.register(i.item.id, "component")?,
                ItemKind::Instance(_) => self.instances.register(i.item.id, "instance")?,
                ItemKind::Value(_) => self.values.register(i.item.id, "value")?,
                ItemKind::Func(_) => self.funcs.register(i.item.id, "func")?,
            },

            ComponentField::Func(i) => self.funcs.register(i.id, "func")?,
            ComponentField::Type(i) => self.types.register(i.id, "type")?,
            ComponentField::Instance(i) => self.instances.register(i.id, "instance")?,
            ComponentField::Module(m) => self.modules.register(m.id, "nested module")?,
            ComponentField::Component(c) => self.components.register(c.id, "nested component")?,
            ComponentField::Alias(a) => self.register_alias(a)?,
            ComponentField::Start(s) => self.values.register(s.result, "value")?,

            // These fields don't define any items in any index space.
            ComponentField::Export(_) => return Ok(()),
        };

        Ok(())
    }

    fn register_alias(&mut self, alias: &Alias<'a>) -> Result<u32, Error> {
        match alias.kind {
            AliasKind::Module => self.modules.register(alias.id, "module"),
            AliasKind::Component => self.components.register(alias.id, "component"),
            AliasKind::Instance => self.instances.register(alias.id, "instance"),
            AliasKind::Value => self.values.register(alias.id, "value"),
            AliasKind::ExportKind(core::ExportKind::Func) => self.funcs.register(alias.id, "func"),
            AliasKind::ExportKind(core::ExportKind::Table) => {
                self.tables.register(alias.id, "table")
            }
            AliasKind::ExportKind(core::ExportKind::Memory) => {
                self.memories.register(alias.id, "memory")
            }
            AliasKind::ExportKind(core::ExportKind::Global) => {
                self.globals.register(alias.id, "global")
            }
            AliasKind::ExportKind(core::ExportKind::Tag) => self.tags.register(alias.id, "tag"),
            AliasKind::ExportKind(core::ExportKind::Type) => self.types.register(alias.id, "type"),
        }
    }
}

#[derive(PartialEq, Eq, Hash, Copy, Clone, Debug)]
enum Ns {
    Func,
    Table,
    Global,
    Memory,
    Tag,
    Type,
    Component,
    Module,
    Instance,
    Value,
}

macro_rules! component_kw_conversions {
    ($($kw:ident => $kind:ident)*) => ($(
        impl From<kw::$kw> for Ns {
            fn from(_: kw::$kw) -> Ns {
                Ns::$kind
            }
        }
    )*);
}

component_kw_conversions! {
    func => Func
    module => Module
    component => Component
    instance => Instance
    value => Value
    table => Table
    memory => Memory
    global => Global
    tag => Tag
    r#type => Type
}

impl From<DefTypeKind> for Ns {
    fn from(kind: DefTypeKind) -> Self {
        match kind {
            DefTypeKind::Module => Ns::Module,
            DefTypeKind::Component => Ns::Component,
            DefTypeKind::Instance => Ns::Instance,
            DefTypeKind::Value => Ns::Value,
            DefTypeKind::Func => Ns::Func,
        }
    }
}

impl From<core::ExportKind> for Ns {
    fn from(kind: core::ExportKind) -> Self {
        match kind {
            core::ExportKind::Func => Ns::Func,
            core::ExportKind::Table => Ns::Table,
            core::ExportKind::Global => Ns::Global,
            core::ExportKind::Memory => Ns::Memory,
            core::ExportKind::Tag => Ns::Tag,
            core::ExportKind::Type => Ns::Type,
        }
    }
}
