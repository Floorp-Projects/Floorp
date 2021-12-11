use crate::ast::*;

pub fn encode(module: &Module<'_>) -> Vec<u8> {
    match &module.kind {
        ModuleKind::Text(fields) => encode_fields(&module.id, &module.name, fields),
        ModuleKind::Binary(bytes) => bytes.iter().flat_map(|b| b.iter().cloned()).collect(),
    }
}

fn encode_fields(
    module_id: &Option<Id<'_>>,
    module_name: &Option<NameAnnotation<'_>>,
    fields: &[ModuleField<'_>],
) -> Vec<u8> {
    use crate::ast::CustomPlace::*;
    use crate::ast::CustomPlaceAnchor::*;

    let mut types = Vec::new();
    let mut imports = Vec::new();
    let mut funcs = Vec::new();
    let mut tables = Vec::new();
    let mut memories = Vec::new();
    let mut globals = Vec::new();
    let mut exports = Vec::new();
    let mut start = Vec::new();
    let mut elem = Vec::new();
    let mut data = Vec::new();
    let mut tags = Vec::new();
    let mut customs = Vec::new();
    let mut instances = Vec::new();
    let mut modules = Vec::new();
    let mut aliases = Vec::new();
    for field in fields {
        match field {
            ModuleField::Type(i) => types.push(i),
            ModuleField::Import(i) => imports.push(i),
            ModuleField::Func(i) => funcs.push(i),
            ModuleField::Table(i) => tables.push(i),
            ModuleField::Memory(i) => memories.push(i),
            ModuleField::Global(i) => globals.push(i),
            ModuleField::Export(i) => exports.push(i),
            ModuleField::Start(i) => start.push(i),
            ModuleField::Elem(i) => elem.push(i),
            ModuleField::Data(i) => data.push(i),
            ModuleField::Tag(i) => tags.push(i),
            ModuleField::Custom(i) => customs.push(i),
            ModuleField::Instance(i) => instances.push(i),
            ModuleField::NestedModule(i) => modules.push(i),
            ModuleField::Alias(a) => aliases.push(a),
        }
    }

    let mut e = Encoder {
        wasm: Vec::new(),
        tmp: Vec::new(),
        customs: &customs,
    };
    e.wasm.extend(b"\0asm");
    e.wasm.extend(b"\x01\0\0\0");

    e.custom_sections(BeforeFirst);

    let mut items = fields
        .iter()
        .filter(|i| match i {
            ModuleField::Alias(_)
            | ModuleField::Type(_)
            | ModuleField::Import(_)
            | ModuleField::NestedModule(_)
            | ModuleField::Instance(_) => true,
            _ => false,
        })
        .peekable();

    // A special path is used for now to handle non-module-linking modules to
    // work around WebAssembly/annotations#11
    if aliases.len() == 0 && modules.len() == 0 && instances.len() == 0 {
        e.section_list(1, Type, &types);
        e.section_list(2, Import, &imports);
    } else {
        while let Some(field) = items.next() {
            macro_rules! list {
                ($code:expr, $name:ident) => {
                    list!($code, $name, $name)
                };
                ($code:expr, $field:ident, $custom:ident) => {
                    if let ModuleField::$field(f) = field {
                        let mut list = vec![f];
                        while let Some(ModuleField::$field(f)) = items.peek() {
                            list.push(f);
                            items.next();
                        }
                        e.section_list($code, $custom, &list);
                    }
                };
            }
            list!(1, Type);
            list!(2, Import);
            list!(14, NestedModule, Module);
            list!(15, Instance);
            list!(16, Alias);
        }
    }

    let functys = funcs.iter().map(|f| &f.ty).collect::<Vec<_>>();
    e.section_list(3, Func, &functys);
    e.section_list(4, Table, &tables);
    e.section_list(5, Memory, &memories);
    e.section_list(13, Tag, &tags);
    e.section_list(6, Global, &globals);
    e.section_list(7, Export, &exports);
    e.custom_sections(Before(Start));
    if let Some(start) = start.get(0) {
        e.section(8, start);
    }
    e.custom_sections(After(Start));
    e.section_list(9, Elem, &elem);
    if contains_bulk_memory(&funcs) {
        e.section(12, &data.len());
    }
    e.section_list(10, Code, &funcs);
    e.section_list(11, Data, &data);

    let names = find_names(module_id, module_name, fields);
    if !names.is_empty() {
        e.section(0, &("name", names));
    }
    e.custom_sections(AfterLast);

    return e.wasm;

    fn contains_bulk_memory(funcs: &[&crate::ast::Func<'_>]) -> bool {
        funcs
            .iter()
            .filter_map(|f| match &f.kind {
                FuncKind::Inline { expression, .. } => Some(expression),
                _ => None,
            })
            .flat_map(|e| e.instrs.iter())
            .any(|i| match i {
                Instruction::MemoryInit(_) | Instruction::DataDrop(_) => true,
                _ => false,
            })
    }
}

struct Encoder<'a> {
    wasm: Vec<u8>,
    tmp: Vec<u8>,
    customs: &'a [&'a Custom<'a>],
}

impl Encoder<'_> {
    fn section(&mut self, id: u8, section: &dyn Encode) {
        self.tmp.truncate(0);
        section.encode(&mut self.tmp);
        self.wasm.push(id);
        self.tmp.encode(&mut self.wasm);
    }

    fn custom_sections(&mut self, place: CustomPlace) {
        for entry in self.customs.iter() {
            if entry.place == place {
                self.section(0, &(entry.name, entry));
            }
        }
    }

    fn section_list(&mut self, id: u8, anchor: CustomPlaceAnchor, list: &[impl Encode]) {
        self.custom_sections(CustomPlace::Before(anchor));
        if !list.is_empty() {
            self.section(id, &list)
        }
        self.custom_sections(CustomPlace::After(anchor));
    }
}

pub(crate) trait Encode {
    fn encode(&self, e: &mut Vec<u8>);
}

impl<T: Encode + ?Sized> Encode for &'_ T {
    fn encode(&self, e: &mut Vec<u8>) {
        T::encode(self, e)
    }
}

impl<T: Encode> Encode for [T] {
    fn encode(&self, e: &mut Vec<u8>) {
        self.len().encode(e);
        for item in self {
            item.encode(e);
        }
    }
}

impl<T: Encode> Encode for Vec<T> {
    fn encode(&self, e: &mut Vec<u8>) {
        <[T]>::encode(self, e)
    }
}

impl Encode for str {
    fn encode(&self, e: &mut Vec<u8>) {
        self.len().encode(e);
        e.extend_from_slice(self.as_bytes());
    }
}

impl Encode for usize {
    fn encode(&self, e: &mut Vec<u8>) {
        assert!(*self <= u32::max_value() as usize);
        (*self as u32).encode(e)
    }
}

impl Encode for u8 {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(*self);
    }
}

impl Encode for u32 {
    fn encode(&self, e: &mut Vec<u8>) {
        leb128::write::unsigned(e, (*self).into()).unwrap();
    }
}

impl Encode for i32 {
    fn encode(&self, e: &mut Vec<u8>) {
        leb128::write::signed(e, (*self).into()).unwrap();
    }
}

impl Encode for u64 {
    fn encode(&self, e: &mut Vec<u8>) {
        leb128::write::unsigned(e, (*self).into()).unwrap();
    }
}

impl Encode for i64 {
    fn encode(&self, e: &mut Vec<u8>) {
        leb128::write::signed(e, *self).unwrap();
    }
}

impl Encode for FunctionType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.params.len().encode(e);
        for (_, _, ty) in self.params.iter() {
            ty.encode(e);
        }
        self.results.encode(e);
    }
}

impl Encode for StructType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.fields.len().encode(e);
        for field in self.fields.iter() {
            field.ty.encode(e);
            (field.mutable as i32).encode(e);
        }
    }
}

impl Encode for ArrayType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.ty.encode(e);
        (self.mutable as i32).encode(e);
    }
}

impl Encode for ModuleType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.imports.encode(e);
        self.exports.encode(e);
    }
}

impl Encode for InstanceType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.exports.encode(e);
    }
}

impl Encode for ExportType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.name.encode(e);
        self.item.encode(e);
    }
}

impl Encode for Type<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.def {
            TypeDef::Func(func) => {
                e.push(0x60);
                func.encode(e)
            }
            TypeDef::Struct(r#struct) => {
                e.push(0x5f);
                r#struct.encode(e)
            }
            TypeDef::Array(array) => {
                e.push(0x5e);
                array.encode(e)
            }
            TypeDef::Module(module) => {
                e.push(0x61);
                module.encode(e)
            }
            TypeDef::Instance(instance) => {
                e.push(0x62);
                instance.encode(e)
            }
        }
    }
}

impl Encode for Option<Id<'_>> {
    fn encode(&self, _e: &mut Vec<u8>) {
        // used for parameters in the tuple impl as well as instruction labels
    }
}

impl<T: Encode, U: Encode> Encode for (T, U) {
    fn encode(&self, e: &mut Vec<u8>) {
        self.0.encode(e);
        self.1.encode(e);
    }
}

impl<'a> Encode for ValType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ValType::I32 => e.push(0x7f),
            ValType::I64 => e.push(0x7e),
            ValType::F32 => e.push(0x7d),
            ValType::F64 => e.push(0x7c),
            ValType::V128 => e.push(0x7b),
            ValType::Rtt(Some(depth), index) => {
                e.push(0x69);
                depth.encode(e);
                index.encode(e);
            }
            ValType::Rtt(None, index) => {
                e.push(0x68);
                index.encode(e);
            }
            ValType::Ref(ty) => {
                ty.encode(e);
            }
        }
    }
}

impl<'a> Encode for HeapType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            HeapType::Func => e.push(0x70),
            HeapType::Extern => e.push(0x6f),
            HeapType::Any => e.push(0x6e),
            HeapType::Eq => e.push(0x6d),
            HeapType::Data => e.push(0x67),
            HeapType::I31 => e.push(0x6a),
            HeapType::Index(index) => {
                index.encode(e);
            }
        }
    }
}

impl<'a> Encode for RefType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            // The 'funcref' binary abbreviation
            RefType {
                nullable: true,
                heap: HeapType::Func,
            } => e.push(0x70),
            // The 'externref' binary abbreviation
            RefType {
                nullable: true,
                heap: HeapType::Extern,
            } => e.push(0x6f),
            // The 'eqref' binary abbreviation
            RefType {
                nullable: true,
                heap: HeapType::Eq,
            } => e.push(0x6d),
            // The 'dataref' binary abbreviation
            RefType {
                nullable: true,
                heap: HeapType::Data,
            } => e.push(0x67),
            // The 'i31ref' binary abbreviation
            RefType {
                nullable: true,
                heap: HeapType::I31,
            } => e.push(0x6a),

            // Generic 'ref opt <heaptype>' encoding
            RefType {
                nullable: true,
                heap,
            } => {
                e.push(0x6c);
                heap.encode(e);
            }
            // Generic 'ref <heaptype>' encoding
            RefType {
                nullable: false,
                heap,
            } => {
                e.push(0x6b);
                heap.encode(e);
            }
        }
    }
}

impl<'a> Encode for StorageType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            StorageType::I8 => e.push(0x7a),
            StorageType::I16 => e.push(0x79),
            StorageType::Val(ty) => {
                ty.encode(e);
            }
        }
    }
}

impl Encode for Import<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.module.encode(e);
        match self.field {
            Some(s) => s.encode(e),
            None => {
                e.push(0x00);
                e.push(0xff);
            }
        }
        self.item.encode(e);
    }
}

impl Encode for ItemSig<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.kind {
            ItemKind::Func(f) => {
                e.push(0x00);
                f.encode(e);
            }
            ItemKind::Table(f) => {
                e.push(0x01);
                f.encode(e);
            }
            ItemKind::Memory(f) => {
                e.push(0x02);
                f.encode(e);
            }
            ItemKind::Global(f) => {
                e.push(0x03);
                f.encode(e);
            }
            ItemKind::Tag(f) => {
                e.push(0x04);
                f.encode(e);
            }
            ItemKind::Module(m) => {
                e.push(0x05);
                m.encode(e);
            }
            ItemKind::Instance(i) => {
                e.push(0x06);
                i.encode(e);
            }
        }
    }
}

impl<T> Encode for TypeUse<'_, T> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.index
            .as_ref()
            .expect("TypeUse should be filled in by this point")
            .encode(e)
    }
}

impl Encode for Index<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            Index::Num(n, _) => n.encode(e),
            Index::Id(n) => panic!("unresolved index in emission: {:?}", n),
        }
    }
}

impl<T> Encode for IndexOrRef<'_, T> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.0.encode(e);
    }
}

impl<T> Encode for ItemRef<'_, T> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ItemRef::Outer { .. } => panic!("should be expanded previously"),
            ItemRef::Item {
                idx,
                exports,
                #[cfg(wast_check_exhaustive)]
                visited,
                ..
            } => {
                #[cfg(wast_check_exhaustive)]
                assert!(*visited);
                assert!(exports.is_empty());
                idx.encode(e);
            }
        }
    }
}

impl<'a> Encode for TableType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.elem.encode(e);
        self.limits.encode(e);
    }
}

impl Encode for Limits {
    fn encode(&self, e: &mut Vec<u8>) {
        match self.max {
            Some(max) => {
                e.push(0x01);
                self.min.encode(e);
                max.encode(e);
            }
            None => {
                e.push(0x00);
                self.min.encode(e);
            }
        }
    }
}

impl Encode for MemoryType {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            MemoryType::B32 { limits, shared } => {
                let flag_max = limits.max.is_some() as u8;
                let flag_shared = *shared as u8;
                let flags = flag_max | (flag_shared << 1);
                e.push(flags);
                limits.min.encode(e);
                if let Some(max) = limits.max {
                    max.encode(e);
                }
            }
            MemoryType::B64 { limits, shared } => {
                let flag_max = limits.max.is_some() as u8;
                let flag_shared = *shared as u8;
                let flags = flag_max | (flag_shared << 1) | 0x04;
                e.push(flags);
                limits.min.encode(e);
                if let Some(max) = limits.max {
                    max.encode(e);
                }
            }
        }
    }
}

impl<'a> Encode for GlobalType<'a> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.ty.encode(e);
        if self.mutable {
            e.push(0x01);
        } else {
            e.push(0x00);
        }
    }
}

impl Encode for Table<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        assert!(self.exports.names.is_empty());
        match &self.kind {
            TableKind::Normal(t) => t.encode(e),
            _ => panic!("TableKind should be normal during encoding"),
        }
    }
}

impl Encode for Memory<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        assert!(self.exports.names.is_empty());
        match &self.kind {
            MemoryKind::Normal(t) => t.encode(e),
            _ => panic!("MemoryKind should be normal during encoding"),
        }
    }
}

impl Encode for Global<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        assert!(self.exports.names.is_empty());
        self.ty.encode(e);
        match &self.kind {
            GlobalKind::Inline(expr) => expr.encode(e),
            _ => panic!("GlobalKind should be inline during encoding"),
        }
    }
}

impl Encode for Export<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.name.encode(e);
        if let ItemRef::Item { kind, .. } = &self.index {
            kind.encode(e);
        }
        self.index.encode(e);
    }
}

impl Encode for ExportKind {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ExportKind::Func => e.push(0x00),
            ExportKind::Table => e.push(0x01),
            ExportKind::Memory => e.push(0x02),
            ExportKind::Global => e.push(0x03),
            ExportKind::Tag => e.push(0x04),
            ExportKind::Module => e.push(0x05),
            ExportKind::Instance => e.push(0x06),
            ExportKind::Type => e.push(0x07),
        }
    }
}

impl Encode for Elem<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        // Try to switch element expressions to indices if we can which uses a
        // more MVP-compatible encoding.
        //
        // FIXME(WebAssembly/wabt#1447) ideally we wouldn't do this so we could
        // be faithful to the original format.
        let indices;
        let mut to_encode = &self.payload;
        if let ElemPayload::Exprs {
            ty:
                RefType {
                    nullable: true,
                    heap: HeapType::Func,
                },
            exprs,
        } = &to_encode
        {
            if let Some(list) = extract_indices(exprs) {
                indices = ElemPayload::Indices(list);
                to_encode = &indices;
            }
        }

        match (&self.kind, &to_encode) {
            (
                ElemKind::Active {
                    table:
                        ItemRef::Item {
                            idx: Index::Num(0, _),
                            ..
                        },
                    offset,
                },
                ElemPayload::Indices(_),
            ) => {
                e.push(0x00);
                offset.encode(e);
            }
            (ElemKind::Passive, ElemPayload::Indices(_)) => {
                e.push(0x01); // flags
                e.push(0x00); // extern_kind
            }
            (ElemKind::Active { table, offset }, ElemPayload::Indices(_)) => {
                e.push(0x02); // flags
                table.encode(e);
                offset.encode(e);
                e.push(0x00); // extern_kind
            }
            (
                ElemKind::Active {
                    table:
                        ItemRef::Item {
                            idx: Index::Num(0, _),
                            ..
                        },
                    offset,
                },
                ElemPayload::Exprs {
                    ty:
                        RefType {
                            nullable: true,
                            heap: HeapType::Func,
                        },
                    ..
                },
            ) => {
                e.push(0x04);
                offset.encode(e);
            }
            (ElemKind::Passive, ElemPayload::Exprs { ty, .. }) => {
                e.push(0x05);
                ty.encode(e);
            }
            (ElemKind::Active { table, offset }, ElemPayload::Exprs { ty, .. }) => {
                e.push(0x06);
                table.encode(e);
                offset.encode(e);
                ty.encode(e);
            }
            (ElemKind::Declared, ElemPayload::Indices(_)) => {
                e.push(0x03); // flags
                e.push(0x00); // extern_kind
            }
            (ElemKind::Declared, ElemPayload::Exprs { ty, .. }) => {
                e.push(0x07); // flags
                ty.encode(e);
            }
        }

        to_encode.encode(e);

        fn extract_indices<'a>(indices: &[Expression<'a>]) -> Option<Vec<ItemRef<'a, kw::func>>> {
            indices
                .iter()
                .map(|expr| {
                    if expr.instrs.len() != 1 {
                        return None;
                    }
                    match &expr.instrs[0] {
                        Instruction::RefFunc(f) => Some(f.0.clone()),
                        _ => None,
                    }
                })
                .collect()
        }
    }
}

impl Encode for ElemPayload<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ElemPayload::Indices(v) => v.encode(e),
            ElemPayload::Exprs { exprs, ty: _ } => {
                exprs.len().encode(e);
                for expr in exprs {
                    expr.encode(e);
                }
            }
        }
    }
}

impl Encode for Data<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.kind {
            DataKind::Passive => e.push(0x01),
            DataKind::Active { memory, offset } => {
                if let ItemRef::Item {
                    idx: Index::Num(0, _),
                    ..
                } = memory
                {
                    e.push(0x00);
                } else {
                    e.push(0x02);
                    memory.encode(e);
                }
                offset.encode(e);
            }
        }
        self.data.iter().map(|l| l.len()).sum::<usize>().encode(e);
        for val in self.data.iter() {
            val.push_onto(e);
        }
    }
}

impl Encode for Func<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        assert!(self.exports.names.is_empty());
        let mut tmp = Vec::new();
        let (expr, locals) = match &self.kind {
            FuncKind::Inline { expression, locals } => (expression, locals),
            _ => panic!("should only have inline functions in emission"),
        };

        locals.encode(&mut tmp);
        expr.encode(&mut tmp);

        tmp.len().encode(e);
        e.extend_from_slice(&tmp);
    }
}

impl Encode for Vec<Local<'_>> {
    fn encode(&self, e: &mut Vec<u8>) {
        let mut locals_compressed = Vec::<(u32, ValType)>::new();
        for local in self {
            if let Some((cnt, prev)) = locals_compressed.last_mut() {
                if *prev == local.ty {
                    *cnt += 1;
                    continue;
                }
            }
            locals_compressed.push((1, local.ty));
        }
        locals_compressed.encode(e);
    }
}

impl Encode for Expression<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        for instr in self.instrs.iter() {
            instr.encode(e);
        }
        e.push(0x0b);
    }
}

impl Encode for BlockType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        // block types using an index are encoded as an sleb, not a uleb
        if let Some(ItemRef::Item {
            idx: Index::Num(n, _),
            ..
        }) = &self.ty.index
        {
            return i64::from(*n).encode(e);
        }
        let ty = self
            .ty
            .inline
            .as_ref()
            .expect("function type not filled in");
        if ty.params.is_empty() && ty.results.is_empty() {
            return e.push(0x40);
        }
        if ty.params.is_empty() && ty.results.len() == 1 {
            return ty.results[0].encode(e);
        }
        panic!("multi-value block types should have an index");
    }
}

impl Encode for FuncBindType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.ty.encode(e);
    }
}

impl Encode for LetType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.block.encode(e);
        self.locals.encode(e);
    }
}

impl Encode for LaneArg {
    fn encode(&self, e: &mut Vec<u8>) {
        self.lane.encode(e);
    }
}

impl Encode for MemArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.memory {
            ItemRef::Item {
                idx: Index::Num(0, _),
                ..
            } => {
                self.align.trailing_zeros().encode(e);
                self.offset.encode(e);
            }
            n => {
                (self.align.trailing_zeros() | (1 << 6)).encode(e);
                self.offset.encode(e);
                n.encode(e);
            }
        }
    }
}

impl Encode for LoadOrStoreLane<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.memarg.encode(e);
        self.lane.encode(e);
    }
}

impl Encode for CallIndirect<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.ty.encode(e);
        self.table.encode(e);
    }
}

impl Encode for TableInit<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.elem.encode(e);
        self.table.encode(e);
    }
}

impl Encode for TableCopy<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.dst.encode(e);
        self.src.encode(e);
    }
}

impl Encode for TableArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.dst.encode(e);
    }
}

impl Encode for MemoryArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.mem.encode(e);
    }
}

impl Encode for MemoryInit<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.data.encode(e);
        self.mem.encode(e);
    }
}

impl Encode for MemoryCopy<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.dst.encode(e);
        self.src.encode(e);
    }
}

impl Encode for BrTableIndices<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.labels.encode(e);
        self.default.encode(e);
    }
}

impl Encode for Float32 {
    fn encode(&self, e: &mut Vec<u8>) {
        e.extend_from_slice(&self.bits.to_le_bytes());
    }
}

impl Encode for Float64 {
    fn encode(&self, e: &mut Vec<u8>) {
        e.extend_from_slice(&self.bits.to_le_bytes());
    }
}

#[derive(Default)]
struct Names<'a> {
    module: Option<&'a str>,
    funcs: Vec<(u32, &'a str)>,
    func_idx: u32,
    locals: Vec<(u32, Vec<(u32, &'a str)>)>,
    labels: Vec<(u32, Vec<(u32, &'a str)>)>,
    globals: Vec<(u32, &'a str)>,
    global_idx: u32,
    memories: Vec<(u32, &'a str)>,
    memory_idx: u32,
    tables: Vec<(u32, &'a str)>,
    table_idx: u32,
    tags: Vec<(u32, &'a str)>,
    tag_idx: u32,
    modules: Vec<(u32, &'a str)>,
    module_idx: u32,
    instances: Vec<(u32, &'a str)>,
    instance_idx: u32,
    types: Vec<(u32, &'a str)>,
    type_idx: u32,
    data: Vec<(u32, &'a str)>,
    data_idx: u32,
    elems: Vec<(u32, &'a str)>,
    elem_idx: u32,
}

fn find_names<'a>(
    module_id: &Option<Id<'a>>,
    module_name: &Option<NameAnnotation<'a>>,
    fields: &[ModuleField<'a>],
) -> Names<'a> {
    fn get_name<'a>(id: &Option<Id<'a>>, name: &Option<NameAnnotation<'a>>) -> Option<&'a str> {
        name.as_ref().map(|n| n.name).or(id.and_then(|id| {
            if id.is_gensym() {
                None
            } else {
                Some(id.name())
            }
        }))
    }

    enum Name {
        Type,
        Global,
        Func,
        Module,
        Instance,
        Memory,
        Table,
        Tag,
        Elem,
        Data,
    }

    let mut ret = Names::default();
    ret.module = get_name(module_id, module_name);
    for field in fields {
        // Extract the kind/id/name from whatever kind of field this is...
        let (kind, id, name) = match field {
            ModuleField::Import(i) => (
                match i.item.kind {
                    ItemKind::Func(_) => Name::Func,
                    ItemKind::Table(_) => Name::Table,
                    ItemKind::Memory(_) => Name::Memory,
                    ItemKind::Global(_) => Name::Global,
                    ItemKind::Tag(_) => Name::Tag,
                    ItemKind::Instance(_) => Name::Instance,
                    ItemKind::Module(_) => Name::Module,
                },
                &i.item.id,
                &i.item.name,
            ),
            ModuleField::Global(g) => (Name::Global, &g.id, &g.name),
            ModuleField::Table(t) => (Name::Table, &t.id, &t.name),
            ModuleField::Memory(m) => (Name::Memory, &m.id, &m.name),
            ModuleField::Tag(t) => (Name::Tag, &t.id, &t.name),
            ModuleField::NestedModule(m) => (Name::Module, &m.id, &m.name),
            ModuleField::Instance(i) => (Name::Instance, &i.id, &i.name),
            ModuleField::Type(t) => (Name::Type, &t.id, &t.name),
            ModuleField::Elem(e) => (Name::Elem, &e.id, &e.name),
            ModuleField::Data(d) => (Name::Data, &d.id, &d.name),
            ModuleField::Func(f) => (Name::Func, &f.id, &f.name),
            ModuleField::Alias(a) => (
                match a.kind {
                    ExportKind::Func => Name::Func,
                    ExportKind::Table => Name::Table,
                    ExportKind::Memory => Name::Memory,
                    ExportKind::Global => Name::Global,
                    ExportKind::Module => Name::Module,
                    ExportKind::Instance => Name::Instance,
                    ExportKind::Tag => Name::Tag,
                    ExportKind::Type => Name::Type,
                },
                &a.id,
                &a.name,
            ),
            ModuleField::Export(_) | ModuleField::Start(_) | ModuleField::Custom(_) => continue,
        };

        // .. and using the kind we can figure out where to place this name
        let (list, idx) = match kind {
            Name::Func => (&mut ret.funcs, &mut ret.func_idx),
            Name::Table => (&mut ret.tables, &mut ret.table_idx),
            Name::Memory => (&mut ret.memories, &mut ret.memory_idx),
            Name::Global => (&mut ret.globals, &mut ret.global_idx),
            Name::Module => (&mut ret.modules, &mut ret.module_idx),
            Name::Instance => (&mut ret.instances, &mut ret.instance_idx),
            Name::Tag => (&mut ret.tags, &mut ret.tag_idx),
            Name::Type => (&mut ret.types, &mut ret.type_idx),
            Name::Elem => (&mut ret.elems, &mut ret.elem_idx),
            Name::Data => (&mut ret.data, &mut ret.data_idx),
        };
        if let Some(name) = get_name(id, name) {
            list.push((*idx, name));
        }

        // Handle module locals separately from above
        if let ModuleField::Func(f) = field {
            let mut local_names = Vec::new();
            let mut label_names = Vec::new();
            let mut local_idx = 0;
            let mut label_idx = 0;

            // Consult the inline type listed for local names of parameters.
            // This is specifically preserved during the name resolution
            // pass, but only for functions, so here we can look at the
            // original source's names.
            if let Some(ty) = &f.ty.inline {
                for (id, name, _) in ty.params.iter() {
                    if let Some(name) = get_name(id, name) {
                        local_names.push((local_idx, name));
                    }
                    local_idx += 1;
                }
            }
            if let FuncKind::Inline {
                locals, expression, ..
            } = &f.kind
            {
                for local in locals {
                    if let Some(name) = get_name(&local.id, &local.name) {
                        local_names.push((local_idx, name));
                    }
                    local_idx += 1;
                }

                for i in expression.instrs.iter() {
                    match i {
                        Instruction::If(block)
                        | Instruction::Block(block)
                        | Instruction::Loop(block)
                        | Instruction::Try(block)
                        | Instruction::Let(LetType { block, .. }) => {
                            if let Some(name) = get_name(&block.label, &block.label_name) {
                                label_names.push((label_idx, name));
                            }
                            label_idx += 1;
                        }
                        _ => {}
                    }
                }
            }
            if local_names.len() > 0 {
                ret.locals.push((*idx, local_names));
            }
            if label_names.len() > 0 {
                ret.labels.push((*idx, label_names));
            }
        }

        *idx += 1;
    }

    return ret;
}

impl Names<'_> {
    fn is_empty(&self) -> bool {
        self.module.is_none()
            && self.funcs.is_empty()
            && self.locals.is_empty()
            && self.labels.is_empty()
            && self.globals.is_empty()
            && self.memories.is_empty()
            && self.tables.is_empty()
            && self.types.is_empty()
            && self.data.is_empty()
            && self.elems.is_empty()
        // NB: specifically don't check tags/modules/instances since they're
        // not encoded for now.
    }
}

impl Encode for Names<'_> {
    fn encode(&self, dst: &mut Vec<u8>) {
        let mut tmp = Vec::new();

        let mut subsec = |id: u8, data: &mut Vec<u8>| {
            dst.push(id);
            data.encode(dst);
            data.truncate(0);
        };

        if let Some(id) = self.module {
            id.encode(&mut tmp);
            subsec(0, &mut tmp);
        }
        if self.funcs.len() > 0 {
            self.funcs.encode(&mut tmp);
            subsec(1, &mut tmp);
        }
        if self.locals.len() > 0 {
            self.locals.encode(&mut tmp);
            subsec(2, &mut tmp);
        }
        if self.labels.len() > 0 {
            self.labels.encode(&mut tmp);
            subsec(3, &mut tmp);
        }
        if self.types.len() > 0 {
            self.types.encode(&mut tmp);
            subsec(4, &mut tmp);
        }
        if self.tables.len() > 0 {
            self.tables.encode(&mut tmp);
            subsec(5, &mut tmp);
        }
        if self.memories.len() > 0 {
            self.memories.encode(&mut tmp);
            subsec(6, &mut tmp);
        }
        if self.globals.len() > 0 {
            self.globals.encode(&mut tmp);
            subsec(7, &mut tmp);
        }
        if self.elems.len() > 0 {
            self.elems.encode(&mut tmp);
            subsec(8, &mut tmp);
        }
        if self.data.len() > 0 {
            self.data.encode(&mut tmp);
            subsec(9, &mut tmp);
        }
    }
}

impl Encode for Id<'_> {
    fn encode(&self, dst: &mut Vec<u8>) {
        assert!(!self.is_gensym());
        self.name().encode(dst);
    }
}

impl Encode for V128Const {
    fn encode(&self, dst: &mut Vec<u8>) {
        dst.extend_from_slice(&self.to_le_bytes());
    }
}

impl Encode for I8x16Shuffle {
    fn encode(&self, dst: &mut Vec<u8>) {
        dst.extend_from_slice(&self.lanes);
    }
}

impl<'a> Encode for SelectTypes<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        match &self.tys {
            Some(list) => {
                dst.push(0x1c);
                list.encode(dst);
            }
            None => dst.push(0x1b),
        }
    }
}

impl Encode for Custom<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        for list in self.data.iter() {
            e.extend_from_slice(list);
        }
    }
}

impl Encode for Tag<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.ty.encode(e);
        match &self.kind {
            TagKind::Inline() => {}
            _ => panic!("TagKind should be inline during encoding"),
        }
    }
}

impl Encode for TagType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            TagType::Exception(ty) => {
                e.push(0x00);
                ty.encode(e);
            }
        }
    }
}

impl Encode for StructAccess<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.r#struct.encode(e);
        self.field.encode(e);
    }
}

impl Encode for NestedModule<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        let fields = match &self.kind {
            NestedModuleKind::Inline { fields, .. } => fields,
            _ => panic!("should only have inline modules in emission"),
        };

        encode_fields(&self.id, &self.name, fields).encode(e);
    }
}

impl Encode for Instance<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        assert!(self.exports.names.is_empty());
        let (module, args) = match &self.kind {
            InstanceKind::Inline { module, args } => (module, args),
            _ => panic!("should only have inline instances in emission"),
        };
        e.push(0x00);
        module.encode(e);
        args.encode(e);
    }
}

impl Encode for InstanceArg<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.name.encode(e);
        if let ItemRef::Item { kind, .. } = &self.index {
            kind.encode(e);
        }
        self.index.encode(e);
    }
}

impl Encode for Alias<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match &self.source {
            AliasSource::InstanceExport { instance, export } => {
                e.push(0x00);
                instance.encode(e);
                self.kind.encode(e);
                export.encode(e);
            }
            AliasSource::Outer { module, index } => {
                e.push(0x01);
                module.encode(e);
                self.kind.encode(e);
                index.encode(e);
            }
        }
    }
}
