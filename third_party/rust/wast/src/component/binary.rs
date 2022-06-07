use crate::component::*;
use crate::core;
use crate::encode::Encode;
use crate::token::{Id, NameAnnotation};

pub fn encode(component: &Component<'_>) -> Vec<u8> {
    match &component.kind {
        ComponentKind::Text(fields) => encode_fields(&component.id, &component.name, fields),
        ComponentKind::Binary(bytes) => bytes.iter().flat_map(|b| b.iter().cloned()).collect(),
    }
}

fn encode_fields(
    component_id: &Option<Id<'_>>,
    component_name: &Option<NameAnnotation<'_>>,
    fields: &[ComponentField<'_>],
) -> Vec<u8> {
    let mut e = Encoder {
        wasm: Vec::new(),
        tmp: Vec::new(),
        last_section: None,
        last_section_contents: Vec::new(),
        last_section_count: 0,
    };
    e.wasm.extend(b"\0asm");
    e.wasm.extend(b"\x0a\0\x01\0");

    for field in fields {
        match field {
            ComponentField::Type(i) => e.append(1, i),
            ComponentField::Import(i) => e.append(2, i),
            ComponentField::Func(i) => e.append(3, i),
            ComponentField::Module(i) => e.section(4, i),
            ComponentField::Component(i) => e.section(5, i),
            ComponentField::Instance(i) => e.append(6, i),
            ComponentField::Export(i) => e.append(7, i),
            ComponentField::Start(i) => e.section(8, i),
            ComponentField::Alias(i) => e.append(9, i),
        }
    }

    // FIXME(WebAssembly/component-model#14): once a name section is defined it
    // should be encoded here.
    drop((component_id, component_name));

    e.flush();

    return e.wasm;
}

struct Encoder {
    wasm: Vec<u8>,
    tmp: Vec<u8>,

    last_section: Option<u8>,
    last_section_contents: Vec<u8>,
    last_section_count: usize,
}

impl Encoder {
    /// Appends an entire section represented by the `section` provided
    fn section(&mut self, id: u8, section: &dyn Encode) {
        self.flush();
        self.tmp.truncate(0);
        section.encode(&mut self.tmp);
        self.wasm.push(id);
        self.tmp.encode(&mut self.wasm);
    }

    /// Appends an `item` specified within the section `id` specified.
    fn append(&mut self, id: u8, item: &dyn Encode) {
        // Flush if necessary to start a new section
        if self.last_section != Some(id) {
            self.flush();
        }
        // ... and afterwards start building up this section incrementally
        // in case the next item encoded is also part of this section.
        item.encode(&mut self.last_section_contents);
        self.last_section_count += 1;
        self.last_section = Some(id);
    }

    fn flush(&mut self) {
        let id = match self.last_section.take() {
            Some(id) => id,
            None => return,
        };
        self.wasm.push(id);
        self.tmp.truncate(0);
        self.last_section_count.encode(&mut self.tmp);
        self.last_section_count = 0;
        self.tmp.extend(self.last_section_contents.drain(..));
        self.tmp.encode(&mut self.wasm);
    }
}

impl Encode for NestedComponent<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        let fields = match &self.kind {
            NestedComponentKind::Import { .. } => panic!("imports should be gone by now"),
            NestedComponentKind::Inline(fields) => fields,
        };
        e.extend(encode_fields(&self.id, &self.name, fields));
    }
}

impl Encode for Module<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.kind {
            ModuleKind::Import { .. } => panic!("imports should be gone by now"),
            ModuleKind::Inline { fields } => {
                e.extend(crate::core::binary::encode(&self.id, &self.name, fields));
            }
        }
    }
}

impl Encode for Instance<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.kind {
            InstanceKind::Module { module, args } => {
                e.push(0x00);
                e.push(0x00);
                module.idx.encode(e);
                args.encode(e);
            }
            InstanceKind::Component { component, args } => {
                e.push(0x00);
                e.push(0x01);
                component.idx.encode(e);
                args.encode(e);
            }
            InstanceKind::BundleOfComponentExports { args } => {
                e.push(0x01);
                args.encode(e);
            }
            InstanceKind::BundleOfExports { args } => {
                e.push(0x02);
                args.encode(e);
            }
            InstanceKind::Import { .. } => unreachable!("should be removed during expansion"),
        }
    }
}

impl Encode for NamedModuleArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.name.encode(e);
        e.push(0x02);
        self.arg.encode(e);
    }
}

impl Encode for ModuleArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ModuleArg::Def(def) => {
                def.idx.encode(e);
            }
            ModuleArg::BundleOfExports(..) => {
                unreachable!("should be expanded already")
            }
        }
    }
}

impl Encode for NamedComponentArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.name.encode(e);
        self.arg.encode(e);
    }
}

impl Encode for ComponentArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ComponentArg::Def(def) => {
                match def.kind {
                    DefTypeKind::Module => e.push(0x00),
                    DefTypeKind::Component => e.push(0x01),
                    DefTypeKind::Instance => e.push(0x02),
                    DefTypeKind::Func => e.push(0x03),
                    DefTypeKind::Value => e.push(0x04),
                }
                def.idx.encode(e);
            }
            ComponentArg::BundleOfExports(..) => {
                unreachable!("should be expanded already")
            }
        }
    }
}

impl Encode for Start<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.func.encode(e);
        self.args.encode(e);
    }
}

impl Encode for Alias<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self.target {
            AliasTarget::Export { instance, export } => {
                match self.kind {
                    AliasKind::Module => {
                        e.push(0x00);
                        e.push(0x00);
                    }
                    AliasKind::Component => {
                        e.push(0x00);
                        e.push(0x01);
                    }
                    AliasKind::Instance => {
                        e.push(0x00);
                        e.push(0x02);
                    }
                    AliasKind::Value => {
                        e.push(0x00);
                        e.push(0x04);
                    }
                    AliasKind::ExportKind(export_kind) => {
                        e.push(0x01);
                        export_kind.encode(e);
                    }
                }
                instance.encode(e);
                export.encode(e);
            }
            AliasTarget::Outer { outer, index } => {
                e.push(0x02);
                match self.kind {
                    AliasKind::Module => e.push(0x00),
                    AliasKind::Component => e.push(0x01),
                    AliasKind::ExportKind(core::ExportKind::Type) => e.push(0x05),
                    // FIXME(#590): this feels a bit odd but it's also weird to
                    // make this an explicit error somewhere else. Should
                    // revisit this when the encodings of aliases and such have
                    // all settled down. Hopefully #590 and
                    // WebAssembly/component-model#29 will help solve this.
                    _ => e.push(0xff),
                }
                outer.encode(e);
                index.encode(e);
            }
        }
    }
}

impl Encode for CanonLower<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x01);
        self.opts.encode(e);
        self.func.encode(e);
    }
}

impl Encode for CanonLift<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x00);
        self.type_.encode(e);
        self.opts.encode(e);
        self.func.encode(e);
    }
}

impl Encode for CanonOpt<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            CanonOpt::StringUtf8 => e.push(0x00),
            CanonOpt::StringUtf16 => e.push(0x01),
            CanonOpt::StringLatin1Utf16 => e.push(0x02),
            CanonOpt::Into(index) => {
                e.push(0x03);
                index.encode(e);
            }
        }
    }
}

impl Encode for ModuleType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x4f);
        self.defs.encode(e);
    }
}

impl Encode for ModuleTypeDef<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ModuleTypeDef::Type(f) => {
                e.push(0x01);
                f.encode(e);
            }
            ModuleTypeDef::Import(i) => {
                e.push(0x02);
                i.encode(e);
            }
            ModuleTypeDef::Export(name, x) => {
                e.push(0x07);
                name.encode(e);
                x.encode(e);
            }
        }
    }
}

impl Encode for ComponentType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x4e);
        self.fields.encode(e);
    }
}

impl Encode for ComponentTypeField<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ComponentTypeField::Type(ty_) => {
                e.push(0x01);
                ty_.encode(e);
            }
            ComponentTypeField::Alias(alias) => {
                e.push(0x09);
                alias.encode(e);
            }
            ComponentTypeField::Export(export) => {
                e.push(0x07);
                export.encode(e);
            }
            ComponentTypeField::Import(import) => {
                e.push(0x02);
                import.encode(e);
            }
        }
    }
}

impl Encode for InstanceType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x4d);
        self.fields.encode(e);
    }
}

impl Encode for InstanceTypeField<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            InstanceTypeField::Type(ty_) => {
                e.push(0x01);
                ty_.encode(e);
            }
            InstanceTypeField::Alias(alias) => {
                e.push(0x09);
                alias.encode(e);
            }
            InstanceTypeField::Export(export) => {
                e.push(0x07);
                export.encode(e);
            }
        }
    }
}

impl Encode for ComponentExportType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.name.encode(e);
        self.item.encode(e);
    }
}
impl Encode for TypeField<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.def {
            ComponentTypeDef::DefType(d) => d.encode(e),
            ComponentTypeDef::InterType(i) => i.encode(e),
        }
    }
}

impl Encode for Primitive {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            Primitive::Unit => e.push(0x7f),
            Primitive::Bool => e.push(0x7e),
            Primitive::S8 => e.push(0x7d),
            Primitive::U8 => e.push(0x7c),
            Primitive::S16 => e.push(0x7b),
            Primitive::U16 => e.push(0x7a),
            Primitive::S32 => e.push(0x79),
            Primitive::U32 => e.push(0x78),
            Primitive::S64 => e.push(0x77),
            Primitive::U64 => e.push(0x76),
            Primitive::Float32 => e.push(0x75),
            Primitive::Float64 => e.push(0x74),
            Primitive::Char => e.push(0x73),
            Primitive::String => e.push(0x72),
        }
    }
}

impl<'a> Encode for InterType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            InterType::Primitive(p) => p.encode(e),
            InterType::Record(r) => r.encode(e),
            InterType::Variant(v) => v.encode(e),
            InterType::List(l) => l.encode(e),
            InterType::Tuple(t) => t.encode(e),
            InterType::Flags(f) => f.encode(e),
            InterType::Enum(n) => n.encode(e),
            InterType::Union(u) => u.encode(e),
            InterType::Option(o) => o.encode(e),
            InterType::Expected(x) => x.encode(e),
        }
    }
}

impl<'a> Encode for InterTypeRef<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            InterTypeRef::Primitive(p) => p.encode(e),
            InterTypeRef::Ref(i) => i.encode(e),
            InterTypeRef::Inline(_) => unreachable!("should be expanded by now"),
        }
    }
}

impl<'a> Encode for DefType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            DefType::Func(f) => f.encode(e),
            DefType::Module(m) => m.encode(e),
            DefType::Component(c) => c.encode(e),
            DefType::Instance(i) => i.encode(e),
            DefType::Value(v) => v.encode(e),
        }
    }
}

impl<'a> Encode for Record<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x71);
        self.fields.encode(e);
    }
}

impl<'a> Encode for Field<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.name.encode(e);
        self.type_.encode(e);
    }
}

impl<'a> Encode for Variant<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x70);
        self.cases.encode(e);
    }
}

impl<'a> Encode for Case<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.name.encode(e);
        self.type_.encode(e);
        if let Some(defaults_to) = self.defaults_to {
            e.push(0x01);
            defaults_to.encode(e);
        } else {
            e.push(0x00);
        }
    }
}

impl<'a> Encode for List<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x6f);
        self.element.encode(e);
    }
}

impl<'a> Encode for Tuple<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x6e);
        self.fields.encode(e);
    }
}

impl<'a> Encode for Flags<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x6d);
        self.flag_names.encode(e);
    }
}

impl<'a> Encode for Enum<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x6c);
        self.arms.encode(e);
    }
}

impl<'a> Encode for Union<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x6b);
        self.arms.encode(e);
    }
}

impl<'a> Encode for OptionType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x6a);
        self.element.encode(e);
    }
}

impl<'a> Encode for Expected<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x69);
        self.ok.encode(e);
        self.err.encode(e);
    }
}

impl<'a> Encode for ComponentFunctionType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x4c);
        self.params.encode(e);
        self.result.encode(e);
    }
}

impl<'a> Encode for ComponentFunctionParam<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        if let Some(id) = self.name {
            e.push(0x01);
            id.encode(e);
        } else {
            e.push(0x00);
        }
        self.type_.encode(e);
    }
}

impl<'a> Encode for ValueType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(0x4b);
        self.value_type.encode(e)
    }
}

impl<T> Encode for ComponentTypeUse<'_, T> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ComponentTypeUse::Inline(_) => unreachable!("should be expanded already"),
            ComponentTypeUse::Ref(index) => index.encode(e),
        }
    }
}

impl Encode for ComponentExport<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.name.encode(e);
        match &self.arg {
            ComponentArg::Def(item_ref) => {
                match item_ref.kind {
                    DefTypeKind::Module => e.push(0x00),
                    DefTypeKind::Component => e.push(0x01),
                    DefTypeKind::Instance => e.push(0x02),
                    DefTypeKind::Func => e.push(0x03),
                    DefTypeKind::Value => e.push(0x04),
                }
                item_ref.idx.encode(e);
            }
            ComponentArg::BundleOfExports(..) => {
                unreachable!("should be expanded already")
            }
        }
    }
}

impl Encode for ComponentFunc<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.kind {
            ComponentFuncKind::Import { .. } => unreachable!("should be expanded by now"),
            ComponentFuncKind::Inline { body } => {
                body.encode(e);
            }
        }
    }
}

impl Encode for ComponentFuncBody<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ComponentFuncBody::CanonLift(lift) => lift.encode(e),
            ComponentFuncBody::CanonLower(lower) => lower.encode(e),
        }
    }
}

impl Encode for ComponentImport<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.name.encode(e);
        self.item.encode(e);
    }
}

impl Encode for ItemSig<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.kind {
            ItemKind::Component(t) => t.encode(e),
            ItemKind::Module(t) => t.encode(e),
            ItemKind::Func(t) => t.encode(e),
            ItemKind::Instance(t) => t.encode(e),
            ItemKind::Value(t) => t.encode(e),
        }
    }
}

impl<K> Encode for ItemRef<'_, K> {
    fn encode(&self, dst: &mut Vec<u8>) {
        assert!(self.export_names.is_empty());
        self.idx.encode(dst);
    }
}

impl Encode for CoreExport<'_> {
    fn encode(&self, dst: &mut Vec<u8>) {
        self.name.encode(dst);
        self.index.kind.encode(dst);
        self.index.encode(dst);
    }
}
