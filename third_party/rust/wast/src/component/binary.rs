use crate::component::*;
use crate::core;
use crate::token::{Id, Index, NameAnnotation, Span};
use wasm_encoder::{
    CanonicalFunctionSection, ComponentAliasSection, ComponentDefinedTypeEncoder,
    ComponentExportSection, ComponentImportSection, ComponentInstanceSection, ComponentNameSection,
    ComponentSection, ComponentSectionId, ComponentStartSection, ComponentTypeEncoder,
    ComponentTypeSection, CoreTypeEncoder, CoreTypeSection, InstanceSection, NameMap,
    NestedComponentSection, RawSection, SectionId,
};

pub fn encode(component: &Component<'_>) -> Vec<u8> {
    match &component.kind {
        ComponentKind::Text(fields) => {
            encode_fields(&component.id, &component.name, fields).finish()
        }
        ComponentKind::Binary(bytes) => bytes.iter().flat_map(|b| b.iter().copied()).collect(),
    }
}

fn encode_fields(
    // TODO: use the id and name for a future names section
    component_id: &Option<Id<'_>>,
    component_name: &Option<NameAnnotation<'_>>,
    fields: &[ComponentField<'_>],
) -> wasm_encoder::Component {
    let mut e = Encoder::default();

    for field in fields {
        match field {
            ComponentField::CoreModule(m) => e.encode_core_module(m),
            ComponentField::CoreInstance(i) => e.encode_core_instance(i),
            ComponentField::CoreType(t) => e.encode_core_type(t),
            ComponentField::Component(c) => e.encode_component(c),
            ComponentField::Instance(i) => e.encode_instance(i),
            ComponentField::Alias(a) => e.encode_alias(a),
            ComponentField::Type(t) => e.encode_type(t),
            ComponentField::CanonicalFunc(f) => e.encode_canonical_func(f),
            ComponentField::CoreFunc(_) | ComponentField::Func(_) => {
                unreachable!("should be expanded already")
            }
            ComponentField::Start(s) => e.encode_start(s),
            ComponentField::Import(i) => e.encode_import(i),
            ComponentField::Export(ex) => e.encode_export(ex),
            ComponentField::Custom(c) => e.encode_custom(c),
            ComponentField::Producers(c) => e.encode_producers(c),
        }
    }

    e.flush(None);
    e.encode_names(component_id, component_name);

    e.component
}

fn encode_core_type(encoder: CoreTypeEncoder, ty: &CoreTypeDef) {
    match ty {
        CoreTypeDef::Def(core::TypeDef::Func(f)) => {
            encoder.function(
                f.params.iter().map(|(_, _, ty)| (*ty).into()),
                f.results.iter().copied().map(Into::into),
            );
        }
        CoreTypeDef::Def(core::TypeDef::Struct(_)) | CoreTypeDef::Def(core::TypeDef::Array(_)) => {
            todo!("encoding of GC proposal types not yet implemented")
        }
        CoreTypeDef::Module(t) => {
            encoder.module(&t.into());
        }
    }
}

fn encode_type(encoder: ComponentTypeEncoder, ty: &TypeDef) {
    match ty {
        TypeDef::Defined(t) => {
            encode_defined_type(encoder.defined_type(), t);
        }
        TypeDef::Func(f) => {
            let mut encoder = encoder.function();
            encoder.params(f.params.iter().map(|p| (p.name, &p.ty)));

            if f.results.len() == 1 && f.results[0].name.is_none() {
                encoder.result(&f.results[0].ty);
            } else {
                encoder.results(f.results.iter().map(|r| (r.name.unwrap_or(""), &r.ty)));
            }
        }
        TypeDef::Component(c) => {
            encoder.component(&c.into());
        }
        TypeDef::Instance(i) => {
            encoder.instance(&i.into());
        }
        TypeDef::Resource(i) => {
            let dtor = i.dtor.as_ref().map(|i| i.idx.into());
            encoder.resource(i.rep.into(), dtor);
        }
    }
}

fn encode_defined_type(encoder: ComponentDefinedTypeEncoder, ty: &ComponentDefinedType) {
    match ty {
        ComponentDefinedType::Primitive(p) => encoder.primitive((*p).into()),
        ComponentDefinedType::Record(r) => {
            encoder.record(r.fields.iter().map(|f| (f.name, &f.ty)));
        }
        ComponentDefinedType::Variant(v) => {
            encoder.variant(v.cases.iter().map(|c| {
                (
                    c.name,
                    c.ty.as_ref().map(Into::into),
                    c.refines.as_ref().map(Into::into),
                )
            }));
        }
        ComponentDefinedType::List(l) => {
            encoder.list(l.element.as_ref());
        }
        ComponentDefinedType::Tuple(t) => {
            encoder.tuple(t.fields.iter());
        }
        ComponentDefinedType::Flags(f) => {
            encoder.flags(f.names.iter().copied());
        }
        ComponentDefinedType::Enum(e) => {
            encoder.enum_type(e.names.iter().copied());
        }
        ComponentDefinedType::Option(o) => {
            encoder.option(o.element.as_ref());
        }
        ComponentDefinedType::Result(e) => {
            encoder.result(
                e.ok.as_deref().map(Into::into),
                e.err.as_deref().map(Into::into),
            );
        }
        ComponentDefinedType::Own(i) => encoder.own((*i).into()),
        ComponentDefinedType::Borrow(i) => encoder.borrow((*i).into()),
    }
}

#[derive(Default)]
struct Encoder<'a> {
    component: wasm_encoder::Component,
    current_section_id: Option<u8>,

    // Core sections
    // Note: module sections are written immediately
    core_instances: InstanceSection,
    core_types: CoreTypeSection,

    // Component sections
    // Note: custom, component, start sections are written immediately
    instances: ComponentInstanceSection,
    aliases: ComponentAliasSection,
    types: ComponentTypeSection,
    funcs: CanonicalFunctionSection,
    imports: ComponentImportSection,
    exports: ComponentExportSection,

    core_func_names: Vec<Option<&'a str>>,
    core_table_names: Vec<Option<&'a str>>,
    core_memory_names: Vec<Option<&'a str>>,
    core_global_names: Vec<Option<&'a str>>,
    core_type_names: Vec<Option<&'a str>>,
    core_module_names: Vec<Option<&'a str>>,
    core_instance_names: Vec<Option<&'a str>>,
    func_names: Vec<Option<&'a str>>,
    value_names: Vec<Option<&'a str>>,
    type_names: Vec<Option<&'a str>>,
    component_names: Vec<Option<&'a str>>,
    instance_names: Vec<Option<&'a str>>,
}

impl<'a> Encoder<'a> {
    fn encode_custom(&mut self, custom: &Custom) {
        // Flush any in-progress section before encoding the customs section
        self.flush(None);
        self.component.section(custom);
    }

    fn encode_producers(&mut self, custom: &core::Producers) {
        use crate::encode::Encode;

        let mut data = Vec::new();
        custom.encode(&mut data);
        self.encode_custom(&Custom {
            name: "producers",
            span: Span::from_offset(0),
            data: vec![&data],
        })
    }

    fn encode_core_module(&mut self, module: &CoreModule<'a>) {
        // Flush any in-progress section before encoding the module
        self.flush(None);

        self.core_module_names
            .push(get_name(&module.id, &module.name));

        match &module.kind {
            CoreModuleKind::Import { .. } => unreachable!("should be expanded already"),
            CoreModuleKind::Inline { fields } => {
                // TODO: replace this with a wasm-encoder based encoding (should return `wasm_encoder::Module`)
                let data = crate::core::binary::encode(&module.id, &module.name, fields);
                self.component.section(&RawSection {
                    id: ComponentSectionId::CoreModule.into(),
                    data: &data,
                });
            }
        }
    }

    fn encode_core_instance(&mut self, instance: &CoreInstance<'a>) {
        self.core_instance_names
            .push(get_name(&instance.id, &instance.name));
        match &instance.kind {
            CoreInstanceKind::Instantiate { module, args } => {
                self.core_instances.instantiate(
                    module.into(),
                    args.iter().map(|arg| (arg.name, (&arg.kind).into())),
                );
            }
            CoreInstanceKind::BundleOfExports(exports) => {
                self.core_instances.export_items(exports.iter().map(|e| {
                    let (kind, index) = (&e.item).into();
                    (e.name, kind, index)
                }));
            }
        }

        self.flush(Some(self.core_instances.id()));
    }

    fn encode_core_type(&mut self, ty: &CoreType<'a>) {
        self.core_type_names.push(get_name(&ty.id, &ty.name));
        encode_core_type(self.core_types.ty(), &ty.def);
        self.flush(Some(self.core_types.id()));
    }

    fn encode_component(&mut self, component: &NestedComponent<'a>) {
        self.component_names
            .push(get_name(&component.id, &component.name));
        // Flush any in-progress section before encoding the component
        self.flush(None);

        match &component.kind {
            NestedComponentKind::Import { .. } => unreachable!("should be expanded already"),
            NestedComponentKind::Inline(fields) => {
                self.component
                    .section(&NestedComponentSection(&encode_fields(
                        &component.id,
                        &component.name,
                        fields,
                    )));
            }
        }
    }

    fn encode_instance(&mut self, instance: &Instance<'a>) {
        self.instance_names
            .push(get_name(&instance.id, &instance.name));
        match &instance.kind {
            InstanceKind::Import { .. } => unreachable!("should be expanded already"),
            InstanceKind::Instantiate { component, args } => {
                self.instances.instantiate(
                    component.into(),
                    args.iter().map(|arg| {
                        let (kind, index) = (&arg.kind).into();
                        (arg.name, kind, index)
                    }),
                );
            }
            InstanceKind::BundleOfExports(exports) => {
                self.instances.export_items(exports.iter().map(|e| {
                    let (kind, index) = (&e.kind).into();
                    (e.name.0, kind, index)
                }));
            }
        }

        self.flush(Some(self.instances.id()));
    }

    fn encode_alias(&mut self, alias: &Alias<'a>) {
        let name = get_name(&alias.id, &alias.name);
        self.aliases.alias((&alias.target).into());
        match &alias.target {
            AliasTarget::Export { kind, .. } => {
                self.names_for_component_export_alias(*kind).push(name);
            }
            AliasTarget::CoreExport { kind, .. } => {
                self.names_for_core_export_alias(*kind).push(name);
            }
            AliasTarget::Outer { kind, .. } => {
                self.names_for_component_outer_alias(*kind).push(name);
            }
        }

        self.flush(Some(self.aliases.id()));
    }

    fn encode_start(&mut self, start: &Start) {
        // Flush any in-progress section before encoding the start section
        self.flush(None);

        self.component.section(&ComponentStartSection {
            function_index: start.func.into(),
            args: start.args.iter().map(|a| a.idx.into()).collect::<Vec<_>>(),
            results: start.results.len() as u32,
        });
    }

    fn encode_type(&mut self, ty: &Type<'a>) {
        self.type_names.push(get_name(&ty.id, &ty.name));
        encode_type(self.types.ty(), &ty.def);
        self.flush(Some(self.types.id()));
    }

    fn encode_canonical_func(&mut self, func: &CanonicalFunc<'a>) {
        let name = get_name(&func.id, &func.name);
        match &func.kind {
            CanonicalFuncKind::Lift { ty, info } => {
                self.func_names.push(name);
                self.funcs.lift(
                    info.func.idx.into(),
                    ty.into(),
                    info.opts.iter().map(Into::into),
                );
            }
            CanonicalFuncKind::Lower(info) => {
                self.core_func_names.push(name);
                self.funcs
                    .lower(info.func.idx.into(), info.opts.iter().map(Into::into));
            }
            CanonicalFuncKind::ResourceNew(info) => {
                self.core_func_names.push(name);
                self.funcs.resource_new(info.ty.into());
            }
            CanonicalFuncKind::ResourceDrop(info) => {
                self.core_func_names.push(name);
                self.funcs.resource_drop(info.ty.into());
            }
            CanonicalFuncKind::ResourceRep(info) => {
                self.core_func_names.push(name);
                self.funcs.resource_rep(info.ty.into());
            }
        }

        self.flush(Some(self.funcs.id()));
    }

    fn encode_import(&mut self, import: &ComponentImport<'a>) {
        let name = get_name(&import.item.id, &import.item.name);
        self.names_for_item_kind(&import.item.kind).push(name);
        self.imports
            .import(import.name.0, (&import.item.kind).into());
        self.flush(Some(self.imports.id()));
    }

    fn encode_export(&mut self, export: &ComponentExport<'a>) {
        let name = get_name(&export.id, &export.debug_name);
        let (kind, index) = (&export.kind).into();
        self.exports.export(
            export.name.0,
            kind,
            index,
            export.ty.as_ref().map(|ty| (&ty.0.kind).into()),
        );
        match &export.kind {
            ComponentExportKind::CoreModule(_) => self.core_module_names.push(name),
            ComponentExportKind::Func(_) => self.func_names.push(name),
            ComponentExportKind::Instance(_) => self.instance_names.push(name),
            ComponentExportKind::Value(_) => self.value_names.push(name),
            ComponentExportKind::Component(_) => self.component_names.push(name),
            ComponentExportKind::Type(_) => self.type_names.push(name),
        }
        self.flush(Some(self.exports.id()));
    }

    fn flush(&mut self, section_id: Option<u8>) {
        if self.current_section_id == section_id {
            return;
        }

        if let Some(id) = self.current_section_id {
            match id {
                // 0 => custom sections are written immediately
                // 1 => core modules sections are written immediately
                2 => {
                    assert_eq!(id, self.core_instances.id());
                    self.component.section(&self.core_instances);
                    self.core_instances = Default::default();
                }
                3 => {
                    assert_eq!(id, self.core_types.id());
                    self.component.section(&self.core_types);
                    self.core_types = Default::default();
                }
                // 4 => components sections are written immediately
                5 => {
                    assert_eq!(id, self.instances.id());
                    self.component.section(&self.instances);
                    self.instances = Default::default();
                }
                6 => {
                    assert_eq!(id, self.aliases.id());
                    self.component.section(&self.aliases);
                    self.aliases = Default::default();
                }
                7 => {
                    assert_eq!(id, self.types.id());
                    self.component.section(&self.types);
                    self.types = Default::default();
                }
                8 => {
                    assert_eq!(id, self.funcs.id());
                    self.component.section(&self.funcs);
                    self.funcs = Default::default();
                }
                // 9 => start sections are written immediately
                10 => {
                    assert_eq!(id, self.imports.id());
                    self.component.section(&self.imports);
                    self.imports = Default::default();
                }
                11 => {
                    assert_eq!(id, self.exports.id());
                    self.component.section(&self.exports);
                    self.exports = Default::default();
                }
                _ => unreachable!("unknown incremental component section id: {}", id),
            }
        }

        self.current_section_id = section_id
    }

    fn encode_names(
        &mut self,
        component_id: &Option<Id<'_>>,
        component_name: &Option<NameAnnotation<'_>>,
    ) {
        let mut names = ComponentNameSection::new();
        if let Some(name) = get_name(component_id, component_name) {
            names.component(name);
        }

        let mut funcs = |list: &[Option<&str>], append: fn(&mut ComponentNameSection, &NameMap)| {
            let mut map = NameMap::new();
            for (i, entry) in list.iter().enumerate() {
                if let Some(name) = entry {
                    map.append(i as u32, name);
                }
            }
            if !map.is_empty() {
                append(&mut names, &map);
            }
        };

        funcs(&self.core_func_names, ComponentNameSection::core_funcs);
        funcs(&self.core_table_names, ComponentNameSection::core_tables);
        funcs(&self.core_memory_names, ComponentNameSection::core_memories);
        funcs(&self.core_global_names, ComponentNameSection::core_globals);
        funcs(&self.core_type_names, ComponentNameSection::core_types);
        funcs(&self.core_module_names, ComponentNameSection::core_modules);
        funcs(
            &self.core_instance_names,
            ComponentNameSection::core_instances,
        );
        funcs(&self.func_names, ComponentNameSection::funcs);
        funcs(&self.value_names, ComponentNameSection::values);
        funcs(&self.type_names, ComponentNameSection::types);
        funcs(&self.component_names, ComponentNameSection::components);
        funcs(&self.instance_names, ComponentNameSection::instances);

        if !names.is_empty() {
            self.component.section(&names);
        }
    }

    fn names_for_component_export_alias(
        &mut self,
        kind: ComponentExportAliasKind,
    ) -> &mut Vec<Option<&'a str>> {
        match kind {
            ComponentExportAliasKind::Func => &mut self.func_names,
            ComponentExportAliasKind::CoreModule => &mut self.core_module_names,
            ComponentExportAliasKind::Value => &mut self.value_names,
            ComponentExportAliasKind::Type => &mut self.type_names,
            ComponentExportAliasKind::Component => &mut self.component_names,
            ComponentExportAliasKind::Instance => &mut self.instance_names,
        }
    }

    fn names_for_component_outer_alias(
        &mut self,
        kind: ComponentOuterAliasKind,
    ) -> &mut Vec<Option<&'a str>> {
        match kind {
            ComponentOuterAliasKind::CoreModule => &mut self.core_module_names,
            ComponentOuterAliasKind::CoreType => &mut self.core_type_names,
            ComponentOuterAliasKind::Component => &mut self.component_names,
            ComponentOuterAliasKind::Type => &mut self.type_names,
        }
    }

    fn names_for_core_export_alias(&mut self, kind: core::ExportKind) -> &mut Vec<Option<&'a str>> {
        match kind {
            core::ExportKind::Func => &mut self.core_func_names,
            core::ExportKind::Global => &mut self.core_global_names,
            core::ExportKind::Table => &mut self.core_table_names,
            core::ExportKind::Memory => &mut self.core_memory_names,
            core::ExportKind::Tag => unimplemented!(),
        }
    }

    fn names_for_item_kind(&mut self, kind: &ItemSigKind) -> &mut Vec<Option<&'a str>> {
        match kind {
            ItemSigKind::CoreModule(_) => &mut self.core_module_names,
            ItemSigKind::Func(_) => &mut self.func_names,
            ItemSigKind::Component(_) => &mut self.component_names,
            ItemSigKind::Instance(_) => &mut self.instance_names,
            ItemSigKind::Value(_) => &mut self.value_names,
            ItemSigKind::Type(_) => &mut self.type_names,
        }
    }
}

fn get_name<'a>(id: &Option<Id<'a>>, name: &Option<NameAnnotation<'a>>) -> Option<&'a str> {
    name.as_ref().map(|n| n.name).or_else(|| {
        id.and_then(|id| {
            if id.is_gensym() {
                None
            } else {
                Some(id.name())
            }
        })
    })
}

// This implementation is much like `wasm_encoder::CustomSection`, except
// that it extends via a list of slices instead of a single slice.
impl wasm_encoder::Encode for Custom<'_> {
    fn encode(&self, sink: &mut Vec<u8>) {
        let mut buf = [0u8; 5];
        let encoded_name_len =
            leb128::write::unsigned(&mut &mut buf[..], u64::try_from(self.name.len()).unwrap())
                .unwrap();
        let data_len = self.data.iter().fold(0, |acc, s| acc + s.len());

        // name length
        (encoded_name_len + self.name.len() + data_len).encode(sink);

        // name
        self.name.encode(sink);

        // data
        for s in &self.data {
            sink.extend(*s);
        }
    }
}

impl wasm_encoder::ComponentSection for Custom<'_> {
    fn id(&self) -> u8 {
        SectionId::Custom.into()
    }
}

// TODO: move these core conversion functions to the core module
// once we update core encoding to use wasm-encoder.
impl From<core::ValType<'_>> for wasm_encoder::ValType {
    fn from(ty: core::ValType) -> Self {
        match ty {
            core::ValType::I32 => Self::I32,
            core::ValType::I64 => Self::I64,
            core::ValType::F32 => Self::F32,
            core::ValType::F64 => Self::F64,
            core::ValType::V128 => Self::V128,
            core::ValType::Ref(r) => Self::Ref(r.into()),
        }
    }
}

impl From<core::RefType<'_>> for wasm_encoder::RefType {
    fn from(r: core::RefType<'_>) -> Self {
        wasm_encoder::RefType {
            nullable: r.nullable,
            heap_type: r.heap.into(),
        }
    }
}

impl From<core::HeapType<'_>> for wasm_encoder::HeapType {
    fn from(r: core::HeapType<'_>) -> Self {
        match r {
            core::HeapType::Func => Self::Func,
            core::HeapType::Extern => Self::Extern,
            core::HeapType::Exn => {
                todo!("encoding of exceptions proposal types not yet implemented")
            }
            core::HeapType::Concrete(Index::Num(i, _)) => Self::Concrete(i),
            core::HeapType::Concrete(_) => panic!("unresolved index"),
            core::HeapType::Any
            | core::HeapType::Eq
            | core::HeapType::Struct
            | core::HeapType::Array
            | core::HeapType::NoFunc
            | core::HeapType::NoExtern
            | core::HeapType::None
            | core::HeapType::I31 => {
                todo!("encoding of GC proposal types not yet implemented")
            }
        }
    }
}

impl From<&core::ItemKind<'_>> for wasm_encoder::EntityType {
    fn from(kind: &core::ItemKind) -> Self {
        match kind {
            core::ItemKind::Func(t) => Self::Function(t.into()),
            core::ItemKind::Table(t) => Self::Table((*t).into()),
            core::ItemKind::Memory(t) => Self::Memory((*t).into()),
            core::ItemKind::Global(t) => Self::Global((*t).into()),
            core::ItemKind::Tag(t) => Self::Tag(t.into()),
        }
    }
}

impl From<core::TableType<'_>> for wasm_encoder::TableType {
    fn from(ty: core::TableType) -> Self {
        Self {
            element_type: ty.elem.into(),
            minimum: ty.limits.min,
            maximum: ty.limits.max,
        }
    }
}

impl From<core::MemoryType> for wasm_encoder::MemoryType {
    fn from(ty: core::MemoryType) -> Self {
        let (minimum, maximum, memory64, shared) = match ty {
            core::MemoryType::B32 { limits, shared } => {
                (limits.min.into(), limits.max.map(Into::into), false, shared)
            }
            core::MemoryType::B64 { limits, shared } => (limits.min, limits.max, true, shared),
        };

        Self {
            minimum,
            maximum,
            memory64,
            shared,
        }
    }
}

impl From<core::GlobalType<'_>> for wasm_encoder::GlobalType {
    fn from(ty: core::GlobalType) -> Self {
        Self {
            val_type: ty.ty.into(),
            mutable: ty.mutable,
        }
    }
}

impl From<&core::TagType<'_>> for wasm_encoder::TagType {
    fn from(ty: &core::TagType) -> Self {
        match ty {
            core::TagType::Exception(r) => Self {
                kind: wasm_encoder::TagKind::Exception,
                func_type_idx: r.into(),
            },
        }
    }
}

impl<T: std::fmt::Debug> From<&core::TypeUse<'_, T>> for u32 {
    fn from(u: &core::TypeUse<'_, T>) -> Self {
        match &u.index {
            Some(i) => (*i).into(),
            None => unreachable!("unresolved type use in encoding: {:?}", u),
        }
    }
}

impl From<&CoreInstantiationArgKind<'_>> for wasm_encoder::ModuleArg {
    fn from(kind: &CoreInstantiationArgKind) -> Self {
        match kind {
            CoreInstantiationArgKind::Instance(i) => {
                wasm_encoder::ModuleArg::Instance(i.idx.into())
            }
            CoreInstantiationArgKind::BundleOfExports(..) => {
                unreachable!("should be expanded already")
            }
        }
    }
}

impl From<&CoreItemRef<'_, core::ExportKind>> for (wasm_encoder::ExportKind, u32) {
    fn from(item: &CoreItemRef<'_, core::ExportKind>) -> Self {
        match &item.kind {
            core::ExportKind::Func => (wasm_encoder::ExportKind::Func, item.idx.into()),
            core::ExportKind::Table => (wasm_encoder::ExportKind::Table, item.idx.into()),
            core::ExportKind::Memory => (wasm_encoder::ExportKind::Memory, item.idx.into()),
            core::ExportKind::Global => (wasm_encoder::ExportKind::Global, item.idx.into()),
            core::ExportKind::Tag => (wasm_encoder::ExportKind::Tag, item.idx.into()),
        }
    }
}

impl From<core::ExportKind> for wasm_encoder::ExportKind {
    fn from(kind: core::ExportKind) -> Self {
        match kind {
            core::ExportKind::Func => Self::Func,
            core::ExportKind::Table => Self::Table,
            core::ExportKind::Memory => Self::Memory,
            core::ExportKind::Global => Self::Global,
            core::ExportKind::Tag => Self::Tag,
        }
    }
}

impl From<Index<'_>> for u32 {
    fn from(i: Index<'_>) -> Self {
        match i {
            Index::Num(i, _) => i,
            Index::Id(_) => unreachable!("unresolved index in encoding: {:?}", i),
        }
    }
}

impl<T> From<&ItemRef<'_, T>> for u32 {
    fn from(i: &ItemRef<'_, T>) -> Self {
        assert!(i.export_names.is_empty());
        i.idx.into()
    }
}

impl<T> From<&CoreTypeUse<'_, T>> for u32 {
    fn from(u: &CoreTypeUse<'_, T>) -> Self {
        match u {
            CoreTypeUse::Inline(_) => unreachable!("should be expanded already"),
            CoreTypeUse::Ref(r) => r.idx.into(),
        }
    }
}

impl<T> From<&ComponentTypeUse<'_, T>> for u32 {
    fn from(u: &ComponentTypeUse<'_, T>) -> Self {
        match u {
            ComponentTypeUse::Inline(_) => unreachable!("should be expanded already"),
            ComponentTypeUse::Ref(r) => r.idx.into(),
        }
    }
}

impl From<&ComponentValType<'_>> for wasm_encoder::ComponentValType {
    fn from(r: &ComponentValType) -> Self {
        match r {
            ComponentValType::Inline(ComponentDefinedType::Primitive(p)) => {
                Self::Primitive((*p).into())
            }
            ComponentValType::Ref(i) => Self::Type(u32::from(*i)),
            ComponentValType::Inline(_) => unreachable!("should be expanded by now"),
        }
    }
}

impl From<PrimitiveValType> for wasm_encoder::PrimitiveValType {
    fn from(p: PrimitiveValType) -> Self {
        match p {
            PrimitiveValType::Bool => Self::Bool,
            PrimitiveValType::S8 => Self::S8,
            PrimitiveValType::U8 => Self::U8,
            PrimitiveValType::S16 => Self::S16,
            PrimitiveValType::U16 => Self::U16,
            PrimitiveValType::S32 => Self::S32,
            PrimitiveValType::U32 => Self::U32,
            PrimitiveValType::S64 => Self::S64,
            PrimitiveValType::U64 => Self::U64,
            PrimitiveValType::Float32 => Self::Float32,
            PrimitiveValType::Float64 => Self::Float64,
            PrimitiveValType::Char => Self::Char,
            PrimitiveValType::String => Self::String,
        }
    }
}

impl From<&Refinement<'_>> for u32 {
    fn from(r: &Refinement) -> Self {
        match r {
            Refinement::Index(..) => unreachable!("should be resolved by now"),
            Refinement::Resolved(i) => *i,
        }
    }
}

impl From<&ItemSigKind<'_>> for wasm_encoder::ComponentTypeRef {
    fn from(k: &ItemSigKind) -> Self {
        match k {
            ItemSigKind::Component(c) => Self::Component(c.into()),
            ItemSigKind::CoreModule(m) => Self::Module(m.into()),
            ItemSigKind::Instance(i) => Self::Instance(i.into()),
            ItemSigKind::Value(v) => Self::Value((&v.0).into()),
            ItemSigKind::Func(f) => Self::Func(f.into()),
            ItemSigKind::Type(TypeBounds::Eq(t)) => {
                Self::Type(wasm_encoder::TypeBounds::Eq((*t).into()))
            }
            ItemSigKind::Type(TypeBounds::SubResource) => {
                Self::Type(wasm_encoder::TypeBounds::SubResource)
            }
        }
    }
}

impl From<&ComponentType<'_>> for wasm_encoder::ComponentType {
    fn from(ty: &ComponentType) -> Self {
        let mut encoded = wasm_encoder::ComponentType::new();

        for decl in &ty.decls {
            match decl {
                ComponentTypeDecl::CoreType(t) => {
                    encode_core_type(encoded.core_type(), &t.def);
                }
                ComponentTypeDecl::Type(t) => {
                    encode_type(encoded.ty(), &t.def);
                }
                ComponentTypeDecl::Alias(a) => {
                    encoded.alias((&a.target).into());
                }
                ComponentTypeDecl::Import(i) => {
                    encoded.import(i.name.0, (&i.item.kind).into());
                }
                ComponentTypeDecl::Export(e) => {
                    encoded.export(e.name.0, (&e.item.kind).into());
                }
            }
        }

        encoded
    }
}

impl From<&InstanceType<'_>> for wasm_encoder::InstanceType {
    fn from(ty: &InstanceType) -> Self {
        let mut encoded = wasm_encoder::InstanceType::new();

        for decl in &ty.decls {
            match decl {
                InstanceTypeDecl::CoreType(t) => {
                    encode_core_type(encoded.core_type(), &t.def);
                }
                InstanceTypeDecl::Type(t) => {
                    encode_type(encoded.ty(), &t.def);
                }
                InstanceTypeDecl::Alias(a) => {
                    encoded.alias((&a.target).into());
                }
                InstanceTypeDecl::Export(e) => {
                    encoded.export(e.name.0, (&e.item.kind).into());
                }
            }
        }

        encoded
    }
}

impl From<&ModuleType<'_>> for wasm_encoder::ModuleType {
    fn from(ty: &ModuleType) -> Self {
        let mut encoded = wasm_encoder::ModuleType::new();

        for decl in &ty.decls {
            match decl {
                ModuleTypeDecl::Type(t) => match &t.def {
                    core::TypeDef::Func(f) => encoded.ty().function(
                        f.params.iter().map(|(_, _, ty)| (*ty).into()),
                        f.results.iter().copied().map(Into::into),
                    ),
                    core::TypeDef::Struct(_) | core::TypeDef::Array(_) => {
                        todo!("encoding of GC proposal types not yet implemented")
                    }
                },
                ModuleTypeDecl::Alias(a) => match &a.target {
                    AliasTarget::Outer {
                        outer,
                        index,
                        kind: ComponentOuterAliasKind::CoreType,
                    } => {
                        encoded.alias_outer_core_type(u32::from(*outer), u32::from(*index));
                    }
                    _ => unreachable!("only outer type aliases are supported"),
                },
                ModuleTypeDecl::Import(i) => {
                    encoded.import(i.module, i.field, (&i.item.kind).into());
                }
                ModuleTypeDecl::Export(name, item) => {
                    encoded.export(name, (&item.kind).into());
                }
            }
        }

        encoded
    }
}

impl From<&InstantiationArgKind<'_>> for (wasm_encoder::ComponentExportKind, u32) {
    fn from(kind: &InstantiationArgKind) -> Self {
        match kind {
            InstantiationArgKind::Item(i) => i.into(),
            InstantiationArgKind::BundleOfExports(..) => unreachable!("should be expanded already"),
        }
    }
}

impl From<&ComponentExportKind<'_>> for (wasm_encoder::ComponentExportKind, u32) {
    fn from(kind: &ComponentExportKind) -> Self {
        match kind {
            ComponentExportKind::CoreModule(m) => {
                (wasm_encoder::ComponentExportKind::Module, m.idx.into())
            }
            ComponentExportKind::Func(f) => (wasm_encoder::ComponentExportKind::Func, f.idx.into()),
            ComponentExportKind::Value(v) => {
                (wasm_encoder::ComponentExportKind::Value, v.idx.into())
            }
            ComponentExportKind::Type(t) => (wasm_encoder::ComponentExportKind::Type, t.idx.into()),
            ComponentExportKind::Component(c) => {
                (wasm_encoder::ComponentExportKind::Component, c.idx.into())
            }
            ComponentExportKind::Instance(i) => {
                (wasm_encoder::ComponentExportKind::Instance, i.idx.into())
            }
        }
    }
}

impl From<ComponentOuterAliasKind> for wasm_encoder::ComponentOuterAliasKind {
    fn from(kind: ComponentOuterAliasKind) -> Self {
        match kind {
            ComponentOuterAliasKind::CoreModule => Self::CoreModule,
            ComponentOuterAliasKind::CoreType => Self::CoreType,
            ComponentOuterAliasKind::Type => Self::Type,
            ComponentOuterAliasKind::Component => Self::Component,
        }
    }
}

impl From<ComponentExportAliasKind> for wasm_encoder::ComponentExportKind {
    fn from(kind: ComponentExportAliasKind) -> Self {
        match kind {
            ComponentExportAliasKind::CoreModule => Self::Module,
            ComponentExportAliasKind::Func => Self::Func,
            ComponentExportAliasKind::Value => Self::Value,
            ComponentExportAliasKind::Type => Self::Type,
            ComponentExportAliasKind::Component => Self::Component,
            ComponentExportAliasKind::Instance => Self::Instance,
        }
    }
}

impl From<&CanonOpt<'_>> for wasm_encoder::CanonicalOption {
    fn from(opt: &CanonOpt) -> Self {
        match opt {
            CanonOpt::StringUtf8 => Self::UTF8,
            CanonOpt::StringUtf16 => Self::UTF16,
            CanonOpt::StringLatin1Utf16 => Self::CompactUTF16,
            CanonOpt::Memory(m) => Self::Memory(m.idx.into()),
            CanonOpt::Realloc(f) => Self::Realloc(f.idx.into()),
            CanonOpt::PostReturn(f) => Self::PostReturn(f.idx.into()),
        }
    }
}

impl<'a> From<&AliasTarget<'a>> for wasm_encoder::Alias<'a> {
    fn from(target: &AliasTarget<'a>) -> Self {
        match target {
            AliasTarget::Export {
                instance,
                name,
                kind,
            } => wasm_encoder::Alias::InstanceExport {
                instance: (*instance).into(),
                kind: (*kind).into(),
                name,
            },
            AliasTarget::CoreExport {
                instance,
                name,
                kind,
            } => wasm_encoder::Alias::CoreInstanceExport {
                instance: (*instance).into(),
                kind: (*kind).into(),
                name,
            },
            AliasTarget::Outer { outer, index, kind } => wasm_encoder::Alias::Outer {
                count: (*outer).into(),
                kind: (*kind).into(),
                index: (*index).into(),
            },
        }
    }
}
