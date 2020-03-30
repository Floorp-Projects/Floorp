use crate::ast::*;

pub fn encode(module: &Module<'_>) -> Vec<u8> {
    use crate::ast::CustomPlace::*;
    use crate::ast::CustomPlaceAnchor::*;

    let fields = match &module.kind {
        ModuleKind::Text(fields) => fields,
        ModuleKind::Binary(bytes) => {
            return bytes.iter().flat_map(|b| b.iter().cloned()).collect();
        }
    };

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
    let mut events = Vec::new();
    let mut gcs = Vec::new();
    let mut customs = Vec::new();
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
            ModuleField::Event(i) => events.push(i),
            ModuleField::GcOptIn(i) => gcs.push(i),
            ModuleField::Custom(i) => customs.push(i),
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
    if let Some(gc) = gcs.get(0) {
        e.section(42, gc);
    }
    e.section_list(1, Type, &types);
    e.section_list(2, Import, &imports);
    let functys = funcs.iter().map(|f| &f.ty).collect::<Vec<_>>();
    e.section_list(3, Func, &functys);
    e.section_list(4, Table, &tables);
    e.section_list(5, Memory, &memories);
    e.section_list(13, Event, &events);
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

    let names = find_names(module, fields);
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
            (field.mutable as i32).encode(e);
            field.ty.encode(e);
        }
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
                e.push(0x50);
                r#struct.encode(e)
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
            ValType::Funcref => e.push(0x70),
            ValType::Anyref => e.push(0x6f),
            ValType::Nullref => e.push(0x6e),
            ValType::Ref(index) => {
                e.push(0x6d);
                index.encode(e);
            }
        }
    }
}

impl Encode for Import<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.module.encode(e);
        self.field.encode(e);
        match &self.kind {
            ImportKind::Func(f) => {
                e.push(0x00);
                f.encode(e);
            }
            ImportKind::Table(f) => {
                e.push(0x01);
                f.encode(e);
            }
            ImportKind::Memory(f) => {
                e.push(0x02);
                f.encode(e);
            }
            ImportKind::Global(f) => {
                e.push(0x03);
                f.encode(e);
            }
            ImportKind::Event(f) => {
                e.push(0x04);
                f.encode(e);
            }
        }
    }
}

impl Encode for TypeUse<'_> {
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
            Index::Num(n) => n.encode(e),
            Index::Id(n) => panic!("unresolved index in emission: {}", n.name()),
        }
    }
}

impl Encode for TableType {
    fn encode(&self, e: &mut Vec<u8>) {
        self.elem.encode(e);
        self.limits.encode(e);
    }
}

impl Encode for TableElemType {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            TableElemType::Funcref => ValType::Funcref.encode(e),
            TableElemType::Anyref => ValType::Anyref.encode(e),
            TableElemType::Nullref => ValType::Nullref.encode(e),
        }
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
        let flag_max = self.limits.max.is_some() as u8;
        let flag_shared = self.shared as u8;
        let flags = flag_max | (flag_shared << 1);
        e.push(flags);
        self.limits.min.encode(e);
        if let Some(max) = self.limits.max {
            max.encode(e);
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
        match &self.kind {
            ExportKind::Func(f) => {
                e.push(0x00);
                f.encode(e);
            }
            ExportKind::Table(f) => {
                e.push(0x01);
                f.encode(e);
            }
            ExportKind::Memory(f) => {
                e.push(0x02);
                f.encode(e);
            }
            ExportKind::Global(f) => {
                e.push(0x03);
                f.encode(e);
            }
            ExportKind::Event(f) => {
                e.push(0x04);
                f.encode(e);
            }
        }
    }
}

impl Encode for Elem<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        // Try to switch element expressions to indices if we can which uses a
        // more MVP-compatible encoding.
        let mut to_encode = self.payload.clone();
        if let ElemPayload::Exprs {
            ty: TableElemType::Funcref,
            exprs,
        } = &to_encode
        {
            if let Some(indices) = extract_indices(exprs) {
                to_encode = ElemPayload::Indices(indices);
            }
        }

        match (&self.kind, &to_encode) {
            (
                ElemKind::Active {
                    table: Index::Num(0),
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
                    table: Index::Num(0),
                    offset,
                },
                ElemPayload::Exprs {
                    ty: TableElemType::Funcref,
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

        fn extract_indices<'a>(indices: &[Option<Index<'a>>]) -> Option<Vec<Index<'a>>> {
            indices.iter().cloned().collect()
        }
    }
}

impl Encode for ElemPayload<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            ElemPayload::Indices(v) => v.encode(e),
            ElemPayload::Exprs { exprs, .. } => {
                exprs.len().encode(e);
                for idx in exprs {
                    match idx {
                        Some(idx) => {
                            Instruction::RefFunc(*idx).encode(e);
                        }
                        None => {
                            Instruction::RefNull.encode(e);
                        }
                    }
                    Instruction::End(None).encode(e);
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
                if *memory == Index::Num(0) {
                    e.push(0x00);
                } else {
                    e.push(0x02);
                    memory.encode(e);
                }
                offset.encode(e);
            }
        }
        self.data.iter().map(|l| l.len()).sum::<usize>().encode(e);
        for list in self.data.iter() {
            e.extend_from_slice(list);
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

        let mut locals_compressed = Vec::<(u32, ValType)>::new();
        for (_, _, ty) in locals {
            if let Some((cnt, prev)) = locals_compressed.last_mut() {
                if prev == ty {
                    *cnt += 1;
                    continue;
                }
            }
            locals_compressed.push((1, *ty));
        }
        locals_compressed.encode(&mut tmp);
        expr.encode(&mut tmp);

        tmp.len().encode(e);
        e.extend_from_slice(&tmp);
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
        if let Some(Index::Num(n)) = &self.ty.index {
            return i64::from(*n).encode(e);
        }
        if self.ty.func_ty.params.is_empty() && self.ty.func_ty.results.is_empty() {
            return e.push(0x40);
        }
        if self.ty.func_ty.params.is_empty() && self.ty.func_ty.results.len() == 1 {
            return self.ty.func_ty.results[0].encode(e);
        }
        panic!("multi-value block types should have an index");
    }
}

impl Encode for MemArg {
    fn encode(&self, e: &mut Vec<u8>) {
        self.align.trailing_zeros().encode(e);
        self.offset.encode(e);
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

impl Encode for MemoryInit<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.data.encode(e);
        e.push(0x00);
    }
}

impl Encode for BrTableIndices<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.labels.encode(e);
        self.default.encode(e);
    }
}

impl Encode for i64 {
    fn encode(&self, e: &mut Vec<u8>) {
        leb128::write::signed(e, *self).unwrap();
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

struct Names<'a> {
    module: Option<&'a str>,
    funcs: Vec<(u32, &'a str)>,
    locals: Vec<(u32, Vec<(u32, &'a str)>)>,
}

fn find_names<'a>(module: &Module<'a>, fields: &[ModuleField<'a>]) -> Names<'a> {
    fn get_name<'a>(id: &Option<Id<'a>>, name: &Option<NameAnnotation<'a>>) -> Option<&'a str> {
        name.as_ref().map(|n| n.name).or(id.map(|id| id.name()))
    }

    let mut funcs = Vec::new();
    let mut locals = Vec::new();
    let mut idx = 0;
    for field in fields {
        match field {
            ModuleField::Import(i) => {
                match i.kind {
                    ImportKind::Func(_) => {}
                    _ => continue,
                }

                if let Some(name) = get_name(&i.id, &i.name) {
                    funcs.push((idx, name));
                }

                idx += 1;
            }
            ModuleField::Func(f) => {
                if let Some(name) = get_name(&f.id, &f.name) {
                    funcs.push((idx, name));
                }
                let mut local_names = Vec::new();
                let mut local_idx = 0;
                for (id, name, _) in f.ty.func_ty.params.iter() {
                    if let Some(name) = get_name(id, name) {
                        local_names.push((local_idx, name));
                    }
                    local_idx += 1;
                }
                if let FuncKind::Inline { locals, .. } = &f.kind {
                    for (id, name, _) in locals {
                        if let Some(name) = get_name(id, name) {
                            local_names.push((local_idx, name));
                        }
                        local_idx += 1;
                    }
                }
                if local_names.len() > 0 {
                    locals.push((idx, local_names));
                }
                idx += 1;
            }
            _ => {}
        }
    }

    Names {
        module: get_name(&module.id, &module.name),
        funcs,
        locals,
    }
}

impl Names<'_> {
    fn is_empty(&self) -> bool {
        self.module.is_none() && self.funcs.is_empty() && self.locals.is_empty()
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
    }
}

impl Encode for Id<'_> {
    fn encode(&self, dst: &mut Vec<u8>) {
        self.name().encode(dst);
    }
}

impl Encode for V128Const {
    fn encode(&self, dst: &mut Vec<u8>) {
        dst.extend_from_slice(&self.to_le_bytes());
    }
}

impl Encode for V8x16Shuffle {
    fn encode(&self, dst: &mut Vec<u8>) {
        dst.extend_from_slice(&self.lanes);
    }
}

impl<'a> Encode for SelectTypes<'a> {
    fn encode(&self, dst: &mut Vec<u8>) {
        if self.tys.len() == 0 {
            dst.push(0x1b);
        } else {
            dst.push(0x1c);
            self.tys.encode(dst);
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

impl Encode for GcOptIn {
    fn encode(&self, e: &mut Vec<u8>) {
        self.version.encode(e);
    }
}

impl Encode for Event<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.ty.encode(e);
    }
}

impl Encode for EventType<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        match self {
            EventType::Exception(ty) => {
                e.push(0x00);
                ty.encode(e);
            }
        }
    }
}

impl Encode for BrOnExn<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.label.encode(e);
        self.exn.encode(e);
    }
}

impl Encode for StructAccess<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.r#struct.encode(e);
        self.field.encode(e);
    }
}

impl Encode for StructNarrow<'_> {
    fn encode(&self, e: &mut Vec<u8>) {
        self.from.encode(e);
        self.to.encode(e);
    }
}
