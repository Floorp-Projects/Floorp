use super::*;
use std::convert::TryFrom;

impl Module {
    /// Encode this Wasm module into bytes.
    pub fn to_bytes(&self) -> Vec<u8> {
        self.encoded().finish()
    }

    fn encoded(&self) -> wasm_encoder::Module {
        let mut module = wasm_encoder::Module::new();

        self.encode_initializers(&mut module);
        self.encode_funcs(&mut module);
        self.encode_tables(&mut module);
        self.encode_memories(&mut module);
        self.encode_globals(&mut module);
        self.encode_exports(&mut module);
        self.encode_start(&mut module);
        self.encode_elems(&mut module);
        self.encode_data_count(&mut module);
        self.encode_code(&mut module);
        self.encode_data(&mut module);

        module
    }

    fn encode_initializers(&self, module: &mut wasm_encoder::Module) {
        for init in self.initial_sections.iter() {
            match init {
                InitialSection::Type(types) => self.encode_types(module, types),
                InitialSection::Import(imports) => self.encode_imports(module, imports),
                InitialSection::Alias(aliases) => self.encode_aliases(module, aliases),
                InitialSection::Instance(list) => self.encode_instances(module, list),
                InitialSection::Module(list) => self.encode_modules(module, list),
            }
        }
    }

    fn encode_types(&self, module: &mut wasm_encoder::Module, types: &[Type]) {
        let mut section = wasm_encoder::TypeSection::new();
        for ty in types {
            match ty {
                Type::Func(ty) => {
                    section.function(
                        ty.params.iter().map(|t| translate_val_type(*t)),
                        ty.results.iter().map(|t| translate_val_type(*t)),
                    );
                }
                Type::Module(ty) => {
                    section.module(
                        ty.imports.iter().map(|(module, name, ty)| {
                            (module.as_str(), name.as_deref(), translate_entity_type(ty))
                        }),
                        ty.exports
                            .exports
                            .iter()
                            .map(|(name, ty)| (name.as_str(), translate_entity_type(ty))),
                    );
                }
                Type::Instance(ty) => {
                    section.instance(
                        ty.exports
                            .iter()
                            .map(|(name, ty)| (name.as_str(), translate_entity_type(ty))),
                    );
                }
            }
        }
        module.section(&section);
    }

    fn encode_imports(
        &self,
        module: &mut wasm_encoder::Module,
        imports: &[(String, Option<String>, EntityType)],
    ) {
        let mut section = wasm_encoder::ImportSection::new();
        for (module, name, ty) in imports {
            section.import(module, name.as_deref(), translate_entity_type(ty));
        }
        module.section(&section);
    }

    fn encode_aliases(&self, module: &mut wasm_encoder::Module, imports: &[Alias]) {
        let mut section = wasm_encoder::AliasSection::new();
        for alias in imports {
            match alias {
                Alias::InstanceExport {
                    instance,
                    kind,
                    name,
                } => {
                    section.instance_export(*instance, translate_item_kind(kind), name);
                }
                Alias::OuterType { depth, index } => {
                    section.outer_type(*depth, *index);
                }
                Alias::OuterModule { depth, index } => {
                    section.outer_module(*depth, *index);
                }
            }
        }
        module.section(&section);
    }

    fn encode_instances(&self, module: &mut wasm_encoder::Module, list: &[Instance]) {
        let mut section = wasm_encoder::InstanceSection::new();
        for instance in list {
            section.instantiate(
                instance.module,
                instance
                    .args
                    .iter()
                    .map(|(name, export)| (name.as_str(), translate_export(export))),
            );
        }
        module.section(&section);
    }

    fn encode_modules(&self, module: &mut wasm_encoder::Module, list: &[Self]) {
        let mut section = wasm_encoder::ModuleSection::new();
        for module in list {
            let encoded = module.encoded();
            section.module(&encoded);
        }
        module.section(&section);
    }

    fn encode_funcs(&self, module: &mut wasm_encoder::Module) {
        if self.num_defined_funcs == 0 {
            return;
        }
        let mut funcs = wasm_encoder::FunctionSection::new();
        for (ty, _) in self.funcs[self.funcs.len() - self.num_defined_funcs..].iter() {
            funcs.function(ty.unwrap());
        }
        module.section(&funcs);
    }

    fn encode_tables(&self, module: &mut wasm_encoder::Module) {
        if self.num_defined_tables == 0 {
            return;
        }
        let mut tables = wasm_encoder::TableSection::new();
        for t in self.tables[self.tables.len() - self.num_defined_tables..].iter() {
            tables.table(translate_table_type(t));
        }
        module.section(&tables);
    }

    fn encode_memories(&self, module: &mut wasm_encoder::Module) {
        if self.num_defined_memories == 0 {
            return;
        }
        let mut mems = wasm_encoder::MemorySection::new();
        for m in self.memories[self.memories.len() - self.num_defined_memories..].iter() {
            mems.memory(translate_memory_type(m));
        }
        module.section(&mems);
    }

    fn encode_globals(&self, module: &mut wasm_encoder::Module) {
        if self.globals.is_empty() {
            return;
        }
        let mut globals = wasm_encoder::GlobalSection::new();
        for (idx, expr) in &self.defined_globals {
            let ty = &self.globals[*idx as usize];
            globals.global(translate_global_type(ty), translate_instruction(expr));
        }
        module.section(&globals);
    }

    fn encode_exports(&self, module: &mut wasm_encoder::Module) {
        if self.exports.is_empty() {
            return;
        }
        let mut exports = wasm_encoder::ExportSection::new();
        for (name, export) in &self.exports {
            exports.export(name, translate_export(export));
        }
        module.section(&exports);
    }

    fn encode_start(&self, module: &mut wasm_encoder::Module) {
        if let Some(f) = self.start {
            module.section(&wasm_encoder::StartSection { function_index: f });
        }
    }

    fn encode_elems(&self, module: &mut wasm_encoder::Module) {
        if self.elems.is_empty() {
            return;
        }
        let mut elems = wasm_encoder::ElementSection::new();
        let mut exps = vec![];
        for el in &self.elems {
            let elem_ty = translate_val_type(el.ty);
            let elements = match &el.items {
                Elements::Expressions(es) => {
                    exps.clear();
                    exps.extend(es.iter().map(|e| match e {
                        Some(i) => wasm_encoder::Element::Func(*i),
                        None => wasm_encoder::Element::Null,
                    }));
                    wasm_encoder::Elements::Expressions(&exps)
                }
                Elements::Functions(fs) => wasm_encoder::Elements::Functions(fs),
            };
            match &el.kind {
                ElementKind::Active { table, offset } => {
                    elems.active(*table, translate_instruction(offset), elem_ty, elements);
                }
                ElementKind::Passive => {
                    elems.passive(elem_ty, elements);
                }
                ElementKind::Declared => {
                    elems.declared(elem_ty, elements);
                }
            }
        }
        module.section(&elems);
    }

    fn encode_data_count(&self, module: &mut wasm_encoder::Module) {
        // Without bulk memory there's no need for a data count section,
        if !self.config.bulk_memory_enabled() {
            return;
        }
        // ... and also if there's no data no need for a data count section.
        if self.data.is_empty() {
            return;
        }
        module.section(&wasm_encoder::DataCountSection {
            count: u32::try_from(self.data.len()).unwrap(),
        });
    }

    fn encode_code(&self, module: &mut wasm_encoder::Module) {
        if self.code.is_empty() {
            return;
        }
        let mut code = wasm_encoder::CodeSection::new();
        for c in &self.code {
            // Skip the run-length encoding because it is a little
            // annoying to compute; use a length of one for every local.
            let mut func =
                wasm_encoder::Function::new(c.locals.iter().map(|l| (1, translate_val_type(*l))));
            match &c.instructions {
                Instructions::Generated(instrs) => {
                    for instr in instrs {
                        func.instruction(translate_instruction(instr));
                    }
                    func.instruction(wasm_encoder::Instruction::End);
                }
                Instructions::Arbitrary(body) => {
                    func.raw(body.iter().copied());
                }
            }
            code.function(&func);
        }
        module.section(&code);
    }

    fn encode_data(&self, module: &mut wasm_encoder::Module) {
        if self.data.is_empty() {
            return;
        }
        let mut data = wasm_encoder::DataSection::new();
        for seg in &self.data {
            match &seg.kind {
                DataSegmentKind::Active {
                    memory_index,
                    offset,
                } => {
                    data.active(
                        *memory_index,
                        translate_instruction(offset),
                        seg.init.iter().copied(),
                    );
                }
                DataSegmentKind::Passive => {
                    data.passive(seg.init.iter().copied());
                }
            }
        }
        module.section(&data);
    }
}

fn translate_val_type(ty: ValType) -> wasm_encoder::ValType {
    match ty {
        ValType::I32 => wasm_encoder::ValType::I32,
        ValType::I64 => wasm_encoder::ValType::I64,
        ValType::F32 => wasm_encoder::ValType::F32,
        ValType::F64 => wasm_encoder::ValType::F64,
        ValType::V128 => wasm_encoder::ValType::V128,
        ValType::FuncRef => wasm_encoder::ValType::FuncRef,
        ValType::ExternRef => wasm_encoder::ValType::ExternRef,
    }
}

fn translate_entity_type(ty: &EntityType) -> wasm_encoder::EntityType {
    match ty {
        EntityType::Func(f, _) => wasm_encoder::EntityType::Function(*f),
        EntityType::Instance(i, _) => wasm_encoder::EntityType::Instance(*i),
        EntityType::Module(i, _) => wasm_encoder::EntityType::Module(*i),
        EntityType::Table(ty) => translate_table_type(ty).into(),
        EntityType::Memory(m) => translate_memory_type(m).into(),
        EntityType::Global(g) => translate_global_type(g).into(),
    }
}

fn translate_table_type(ty: &TableType) -> wasm_encoder::TableType {
    wasm_encoder::TableType {
        element_type: translate_val_type(ty.elem_ty),
        minimum: ty.minimum,
        maximum: ty.maximum,
    }
}

fn translate_memory_type(ty: &MemoryType) -> wasm_encoder::MemoryType {
    wasm_encoder::MemoryType {
        minimum: ty.minimum,
        maximum: ty.maximum,
        memory64: ty.memory64,
    }
}

fn translate_global_type(ty: &GlobalType) -> wasm_encoder::GlobalType {
    wasm_encoder::GlobalType {
        val_type: translate_val_type(ty.val_type),
        mutable: ty.mutable,
    }
}

fn translate_block_type(ty: BlockType) -> wasm_encoder::BlockType {
    match ty {
        BlockType::Empty => wasm_encoder::BlockType::Empty,
        BlockType::Result(ty) => wasm_encoder::BlockType::Result(translate_val_type(ty)),
        BlockType::FuncType(f) => wasm_encoder::BlockType::FunctionType(f),
    }
}

fn translate_mem_arg(m: MemArg) -> wasm_encoder::MemArg {
    wasm_encoder::MemArg {
        offset: m.offset,
        align: m.align,
        memory_index: m.memory_index,
    }
}

fn translate_item_kind(kind: &ItemKind) -> wasm_encoder::ItemKind {
    match kind {
        ItemKind::Func => wasm_encoder::ItemKind::Function,
        ItemKind::Table => wasm_encoder::ItemKind::Table,
        ItemKind::Memory => wasm_encoder::ItemKind::Memory,
        ItemKind::Global => wasm_encoder::ItemKind::Global,
        ItemKind::Instance => wasm_encoder::ItemKind::Instance,
        ItemKind::Module => wasm_encoder::ItemKind::Module,
    }
}

fn translate_export(export: &Export) -> wasm_encoder::Export {
    match export {
        Export::Func(idx) => wasm_encoder::Export::Function(*idx),
        Export::Table(idx) => wasm_encoder::Export::Table(*idx),
        Export::Memory(idx) => wasm_encoder::Export::Memory(*idx),
        Export::Global(idx) => wasm_encoder::Export::Global(*idx),
        Export::Instance(idx) => wasm_encoder::Export::Instance(*idx),
        Export::Module(idx) => wasm_encoder::Export::Module(*idx),
    }
}

fn translate_instruction(inst: &Instruction) -> wasm_encoder::Instruction {
    use Instruction::*;
    match *inst {
        // Control instructions.
        Unreachable => wasm_encoder::Instruction::Unreachable,
        Nop => wasm_encoder::Instruction::Nop,
        Block(bt) => wasm_encoder::Instruction::Block(translate_block_type(bt)),
        Loop(bt) => wasm_encoder::Instruction::Loop(translate_block_type(bt)),
        If(bt) => wasm_encoder::Instruction::If(translate_block_type(bt)),
        Else => wasm_encoder::Instruction::Else,
        End => wasm_encoder::Instruction::End,
        Br(x) => wasm_encoder::Instruction::Br(x),
        BrIf(x) => wasm_encoder::Instruction::BrIf(x),
        BrTable(ref ls, l) => wasm_encoder::Instruction::BrTable(ls, l),
        Return => wasm_encoder::Instruction::Return,
        Call(x) => wasm_encoder::Instruction::Call(x),
        CallIndirect { ty, table } => wasm_encoder::Instruction::CallIndirect { ty, table },

        // Parametric instructions.
        Drop => wasm_encoder::Instruction::Drop,
        Select => wasm_encoder::Instruction::Select,

        // Variable instructions.
        LocalGet(x) => wasm_encoder::Instruction::LocalGet(x),
        LocalSet(x) => wasm_encoder::Instruction::LocalSet(x),
        LocalTee(x) => wasm_encoder::Instruction::LocalTee(x),
        GlobalGet(x) => wasm_encoder::Instruction::GlobalGet(x),
        GlobalSet(x) => wasm_encoder::Instruction::GlobalSet(x),

        // Memory instructions.
        I32Load(m) => wasm_encoder::Instruction::I32Load(translate_mem_arg(m)),
        I64Load(m) => wasm_encoder::Instruction::I64Load(translate_mem_arg(m)),
        F32Load(m) => wasm_encoder::Instruction::F32Load(translate_mem_arg(m)),
        F64Load(m) => wasm_encoder::Instruction::F64Load(translate_mem_arg(m)),
        I32Load8_S(m) => wasm_encoder::Instruction::I32Load8_S(translate_mem_arg(m)),
        I32Load8_U(m) => wasm_encoder::Instruction::I32Load8_U(translate_mem_arg(m)),
        I32Load16_S(m) => wasm_encoder::Instruction::I32Load16_S(translate_mem_arg(m)),
        I32Load16_U(m) => wasm_encoder::Instruction::I32Load16_U(translate_mem_arg(m)),
        I64Load8_S(m) => wasm_encoder::Instruction::I64Load8_S(translate_mem_arg(m)),
        I64Load8_U(m) => wasm_encoder::Instruction::I64Load8_U(translate_mem_arg(m)),
        I64Load16_S(m) => wasm_encoder::Instruction::I64Load16_S(translate_mem_arg(m)),
        I64Load16_U(m) => wasm_encoder::Instruction::I64Load16_U(translate_mem_arg(m)),
        I64Load32_S(m) => wasm_encoder::Instruction::I64Load32_S(translate_mem_arg(m)),
        I64Load32_U(m) => wasm_encoder::Instruction::I64Load32_U(translate_mem_arg(m)),
        I32Store(m) => wasm_encoder::Instruction::I32Store(translate_mem_arg(m)),
        I64Store(m) => wasm_encoder::Instruction::I64Store(translate_mem_arg(m)),
        F32Store(m) => wasm_encoder::Instruction::F32Store(translate_mem_arg(m)),
        F64Store(m) => wasm_encoder::Instruction::F64Store(translate_mem_arg(m)),
        I32Store8(m) => wasm_encoder::Instruction::I32Store8(translate_mem_arg(m)),
        I32Store16(m) => wasm_encoder::Instruction::I32Store16(translate_mem_arg(m)),
        I64Store8(m) => wasm_encoder::Instruction::I64Store8(translate_mem_arg(m)),
        I64Store16(m) => wasm_encoder::Instruction::I64Store16(translate_mem_arg(m)),
        I64Store32(m) => wasm_encoder::Instruction::I64Store32(translate_mem_arg(m)),
        MemorySize(x) => wasm_encoder::Instruction::MemorySize(x),
        MemoryGrow(x) => wasm_encoder::Instruction::MemoryGrow(x),
        MemoryInit { mem, data } => wasm_encoder::Instruction::MemoryInit { mem, data },
        DataDrop(x) => wasm_encoder::Instruction::DataDrop(x),
        MemoryCopy { src, dst } => wasm_encoder::Instruction::MemoryCopy { src, dst },
        MemoryFill(x) => wasm_encoder::Instruction::MemoryFill(x),

        // Numeric instructions.
        I32Const(x) => wasm_encoder::Instruction::I32Const(x),
        I64Const(x) => wasm_encoder::Instruction::I64Const(x),
        F32Const(x) => wasm_encoder::Instruction::F32Const(x),
        F64Const(x) => wasm_encoder::Instruction::F64Const(x),
        I32Eqz => wasm_encoder::Instruction::I32Eqz,
        I32Eq => wasm_encoder::Instruction::I32Eq,
        I32Neq => wasm_encoder::Instruction::I32Neq,
        I32LtS => wasm_encoder::Instruction::I32LtS,
        I32LtU => wasm_encoder::Instruction::I32LtU,
        I32GtS => wasm_encoder::Instruction::I32GtS,
        I32GtU => wasm_encoder::Instruction::I32GtU,
        I32LeS => wasm_encoder::Instruction::I32LeS,
        I32LeU => wasm_encoder::Instruction::I32LeU,
        I32GeS => wasm_encoder::Instruction::I32GeS,
        I32GeU => wasm_encoder::Instruction::I32GeU,
        I64Eqz => wasm_encoder::Instruction::I64Eqz,
        I64Eq => wasm_encoder::Instruction::I64Eq,
        I64Neq => wasm_encoder::Instruction::I64Neq,
        I64LtS => wasm_encoder::Instruction::I64LtS,
        I64LtU => wasm_encoder::Instruction::I64LtU,
        I64GtS => wasm_encoder::Instruction::I64GtS,
        I64GtU => wasm_encoder::Instruction::I64GtU,
        I64LeS => wasm_encoder::Instruction::I64LeS,
        I64LeU => wasm_encoder::Instruction::I64LeU,
        I64GeS => wasm_encoder::Instruction::I64GeS,
        I64GeU => wasm_encoder::Instruction::I64GeU,
        F32Eq => wasm_encoder::Instruction::F32Eq,
        F32Neq => wasm_encoder::Instruction::F32Neq,
        F32Lt => wasm_encoder::Instruction::F32Lt,
        F32Gt => wasm_encoder::Instruction::F32Gt,
        F32Le => wasm_encoder::Instruction::F32Le,
        F32Ge => wasm_encoder::Instruction::F32Ge,
        F64Eq => wasm_encoder::Instruction::F64Eq,
        F64Neq => wasm_encoder::Instruction::F64Neq,
        F64Lt => wasm_encoder::Instruction::F64Lt,
        F64Gt => wasm_encoder::Instruction::F64Gt,
        F64Le => wasm_encoder::Instruction::F64Le,
        F64Ge => wasm_encoder::Instruction::F64Ge,
        I32Clz => wasm_encoder::Instruction::I32Clz,
        I32Ctz => wasm_encoder::Instruction::I32Ctz,
        I32Popcnt => wasm_encoder::Instruction::I32Popcnt,
        I32Add => wasm_encoder::Instruction::I32Add,
        I32Sub => wasm_encoder::Instruction::I32Sub,
        I32Mul => wasm_encoder::Instruction::I32Mul,
        I32DivS => wasm_encoder::Instruction::I32DivS,
        I32DivU => wasm_encoder::Instruction::I32DivU,
        I32RemS => wasm_encoder::Instruction::I32RemS,
        I32RemU => wasm_encoder::Instruction::I32RemU,
        I32And => wasm_encoder::Instruction::I32And,
        I32Or => wasm_encoder::Instruction::I32Or,
        I32Xor => wasm_encoder::Instruction::I32Xor,
        I32Shl => wasm_encoder::Instruction::I32Shl,
        I32ShrS => wasm_encoder::Instruction::I32ShrS,
        I32ShrU => wasm_encoder::Instruction::I32ShrU,
        I32Rotl => wasm_encoder::Instruction::I32Rotl,
        I32Rotr => wasm_encoder::Instruction::I32Rotr,
        I64Clz => wasm_encoder::Instruction::I64Clz,
        I64Ctz => wasm_encoder::Instruction::I64Ctz,
        I64Popcnt => wasm_encoder::Instruction::I64Popcnt,
        I64Add => wasm_encoder::Instruction::I64Add,
        I64Sub => wasm_encoder::Instruction::I64Sub,
        I64Mul => wasm_encoder::Instruction::I64Mul,
        I64DivS => wasm_encoder::Instruction::I64DivS,
        I64DivU => wasm_encoder::Instruction::I64DivU,
        I64RemS => wasm_encoder::Instruction::I64RemS,
        I64RemU => wasm_encoder::Instruction::I64RemU,
        I64And => wasm_encoder::Instruction::I64And,
        I64Or => wasm_encoder::Instruction::I64Or,
        I64Xor => wasm_encoder::Instruction::I64Xor,
        I64Shl => wasm_encoder::Instruction::I64Shl,
        I64ShrS => wasm_encoder::Instruction::I64ShrS,
        I64ShrU => wasm_encoder::Instruction::I64ShrU,
        I64Rotl => wasm_encoder::Instruction::I64Rotl,
        I64Rotr => wasm_encoder::Instruction::I64Rotr,
        F32Abs => wasm_encoder::Instruction::F32Abs,
        F32Neg => wasm_encoder::Instruction::F32Neg,
        F32Ceil => wasm_encoder::Instruction::F32Ceil,
        F32Floor => wasm_encoder::Instruction::F32Floor,
        F32Trunc => wasm_encoder::Instruction::F32Trunc,
        F32Nearest => wasm_encoder::Instruction::F32Nearest,
        F32Sqrt => wasm_encoder::Instruction::F32Sqrt,
        F32Add => wasm_encoder::Instruction::F32Add,
        F32Sub => wasm_encoder::Instruction::F32Sub,
        F32Mul => wasm_encoder::Instruction::F32Mul,
        F32Div => wasm_encoder::Instruction::F32Div,
        F32Min => wasm_encoder::Instruction::F32Min,
        F32Max => wasm_encoder::Instruction::F32Max,
        F32Copysign => wasm_encoder::Instruction::F32Copysign,
        F64Abs => wasm_encoder::Instruction::F64Abs,
        F64Neg => wasm_encoder::Instruction::F64Neg,
        F64Ceil => wasm_encoder::Instruction::F64Ceil,
        F64Floor => wasm_encoder::Instruction::F64Floor,
        F64Trunc => wasm_encoder::Instruction::F64Trunc,
        F64Nearest => wasm_encoder::Instruction::F64Nearest,
        F64Sqrt => wasm_encoder::Instruction::F64Sqrt,
        F64Add => wasm_encoder::Instruction::F64Add,
        F64Sub => wasm_encoder::Instruction::F64Sub,
        F64Mul => wasm_encoder::Instruction::F64Mul,
        F64Div => wasm_encoder::Instruction::F64Div,
        F64Min => wasm_encoder::Instruction::F64Min,
        F64Max => wasm_encoder::Instruction::F64Max,
        F64Copysign => wasm_encoder::Instruction::F64Copysign,
        I32WrapI64 => wasm_encoder::Instruction::I32WrapI64,
        I32TruncF32S => wasm_encoder::Instruction::I32TruncF32S,
        I32TruncF32U => wasm_encoder::Instruction::I32TruncF32U,
        I32TruncF64S => wasm_encoder::Instruction::I32TruncF64S,
        I32TruncF64U => wasm_encoder::Instruction::I32TruncF64U,
        I64ExtendI32S => wasm_encoder::Instruction::I64ExtendI32S,
        I64ExtendI32U => wasm_encoder::Instruction::I64ExtendI32U,
        I64TruncF32S => wasm_encoder::Instruction::I64TruncF32S,
        I64TruncF32U => wasm_encoder::Instruction::I64TruncF32U,
        I64TruncF64S => wasm_encoder::Instruction::I64TruncF64S,
        I64TruncF64U => wasm_encoder::Instruction::I64TruncF64U,
        F32ConvertI32S => wasm_encoder::Instruction::F32ConvertI32S,
        F32ConvertI32U => wasm_encoder::Instruction::F32ConvertI32U,
        F32ConvertI64S => wasm_encoder::Instruction::F32ConvertI64S,
        F32ConvertI64U => wasm_encoder::Instruction::F32ConvertI64U,
        F32DemoteF64 => wasm_encoder::Instruction::F32DemoteF64,
        F64ConvertI32S => wasm_encoder::Instruction::F64ConvertI32S,
        F64ConvertI32U => wasm_encoder::Instruction::F64ConvertI32U,
        F64ConvertI64S => wasm_encoder::Instruction::F64ConvertI64S,
        F64ConvertI64U => wasm_encoder::Instruction::F64ConvertI64U,
        F64PromoteF32 => wasm_encoder::Instruction::F64PromoteF32,
        I32ReinterpretF32 => wasm_encoder::Instruction::I32ReinterpretF32,
        I64ReinterpretF64 => wasm_encoder::Instruction::I64ReinterpretF64,
        F32ReinterpretI32 => wasm_encoder::Instruction::F32ReinterpretI32,
        F64ReinterpretI64 => wasm_encoder::Instruction::F64ReinterpretI64,
        I32Extend8S => wasm_encoder::Instruction::I32Extend8S,
        I32Extend16S => wasm_encoder::Instruction::I32Extend16S,
        I64Extend8S => wasm_encoder::Instruction::I64Extend8S,
        I64Extend16S => wasm_encoder::Instruction::I64Extend16S,
        I64Extend32S => wasm_encoder::Instruction::I64Extend32S,
        I32TruncSatF32S => wasm_encoder::Instruction::I32TruncSatF32S,
        I32TruncSatF32U => wasm_encoder::Instruction::I32TruncSatF32U,
        I32TruncSatF64S => wasm_encoder::Instruction::I32TruncSatF64S,
        I32TruncSatF64U => wasm_encoder::Instruction::I32TruncSatF64U,
        I64TruncSatF32S => wasm_encoder::Instruction::I64TruncSatF32S,
        I64TruncSatF32U => wasm_encoder::Instruction::I64TruncSatF32U,
        I64TruncSatF64S => wasm_encoder::Instruction::I64TruncSatF64S,
        I64TruncSatF64U => wasm_encoder::Instruction::I64TruncSatF64U,
        TypedSelect(ty) => wasm_encoder::Instruction::TypedSelect(translate_val_type(ty)),
        RefNull(ty) => wasm_encoder::Instruction::RefNull(translate_val_type(ty)),
        RefIsNull => wasm_encoder::Instruction::RefIsNull,
        RefFunc(x) => wasm_encoder::Instruction::RefFunc(x),
        TableInit { segment, table } => wasm_encoder::Instruction::TableInit { segment, table },
        ElemDrop { segment } => wasm_encoder::Instruction::ElemDrop { segment },
        TableFill { table } => wasm_encoder::Instruction::TableFill { table },
        TableSet { table } => wasm_encoder::Instruction::TableSet { table },
        TableGet { table } => wasm_encoder::Instruction::TableGet { table },
        TableGrow { table } => wasm_encoder::Instruction::TableGrow { table },
        TableSize { table } => wasm_encoder::Instruction::TableSize { table },
        TableCopy { src, dst } => wasm_encoder::Instruction::TableCopy { src, dst },

        // SIMD instructions.
        V128Load { memarg } => wasm_encoder::Instruction::V128Load {
            memarg: translate_mem_arg(memarg),
        },
        V128Load8x8S { memarg } => wasm_encoder::Instruction::V128Load8x8S {
            memarg: translate_mem_arg(memarg),
        },
        V128Load8x8U { memarg } => wasm_encoder::Instruction::V128Load8x8U {
            memarg: translate_mem_arg(memarg),
        },
        V128Load16x4S { memarg } => wasm_encoder::Instruction::V128Load16x4S {
            memarg: translate_mem_arg(memarg),
        },
        V128Load16x4U { memarg } => wasm_encoder::Instruction::V128Load16x4U {
            memarg: translate_mem_arg(memarg),
        },
        V128Load32x2S { memarg } => wasm_encoder::Instruction::V128Load32x2S {
            memarg: translate_mem_arg(memarg),
        },
        V128Load32x2U { memarg } => wasm_encoder::Instruction::V128Load32x2U {
            memarg: translate_mem_arg(memarg),
        },
        V128Load8Splat { memarg } => wasm_encoder::Instruction::V128Load8Splat {
            memarg: translate_mem_arg(memarg),
        },
        V128Load16Splat { memarg } => wasm_encoder::Instruction::V128Load16Splat {
            memarg: translate_mem_arg(memarg),
        },
        V128Load32Splat { memarg } => wasm_encoder::Instruction::V128Load32Splat {
            memarg: translate_mem_arg(memarg),
        },
        V128Load64Splat { memarg } => wasm_encoder::Instruction::V128Load64Splat {
            memarg: translate_mem_arg(memarg),
        },
        V128Load32Zero { memarg } => wasm_encoder::Instruction::V128Load32Zero {
            memarg: translate_mem_arg(memarg),
        },
        V128Load64Zero { memarg } => wasm_encoder::Instruction::V128Load64Zero {
            memarg: translate_mem_arg(memarg),
        },
        V128Store { memarg } => wasm_encoder::Instruction::V128Store {
            memarg: translate_mem_arg(memarg),
        },
        V128Load8Lane { memarg, lane } => wasm_encoder::Instruction::V128Load8Lane {
            memarg: translate_mem_arg(memarg),
            lane,
        },
        V128Load16Lane { memarg, lane } => wasm_encoder::Instruction::V128Load16Lane {
            memarg: translate_mem_arg(memarg),
            lane,
        },
        V128Load32Lane { memarg, lane } => wasm_encoder::Instruction::V128Load32Lane {
            memarg: translate_mem_arg(memarg),
            lane,
        },
        V128Load64Lane { memarg, lane } => wasm_encoder::Instruction::V128Load64Lane {
            memarg: translate_mem_arg(memarg),
            lane,
        },
        V128Store8Lane { memarg, lane } => wasm_encoder::Instruction::V128Store8Lane {
            memarg: translate_mem_arg(memarg),
            lane,
        },
        V128Store16Lane { memarg, lane } => wasm_encoder::Instruction::V128Store16Lane {
            memarg: translate_mem_arg(memarg),
            lane,
        },
        V128Store32Lane { memarg, lane } => wasm_encoder::Instruction::V128Store32Lane {
            memarg: translate_mem_arg(memarg),
            lane,
        },
        V128Store64Lane { memarg, lane } => wasm_encoder::Instruction::V128Store64Lane {
            memarg: translate_mem_arg(memarg),
            lane,
        },
        V128Const(c) => wasm_encoder::Instruction::V128Const(c),
        I8x16Shuffle { lanes } => wasm_encoder::Instruction::I8x16Shuffle { lanes },
        I8x16ExtractLaneS { lane } => wasm_encoder::Instruction::I8x16ExtractLaneS { lane },
        I8x16ExtractLaneU { lane } => wasm_encoder::Instruction::I8x16ExtractLaneU { lane },
        I8x16ReplaceLane { lane } => wasm_encoder::Instruction::I8x16ReplaceLane { lane },
        I16x8ExtractLaneS { lane } => wasm_encoder::Instruction::I16x8ExtractLaneS { lane },
        I16x8ExtractLaneU { lane } => wasm_encoder::Instruction::I16x8ExtractLaneU { lane },
        I16x8ReplaceLane { lane } => wasm_encoder::Instruction::I16x8ReplaceLane { lane },
        I32x4ExtractLane { lane } => wasm_encoder::Instruction::I32x4ExtractLane { lane },
        I32x4ReplaceLane { lane } => wasm_encoder::Instruction::I32x4ReplaceLane { lane },
        I64x2ExtractLane { lane } => wasm_encoder::Instruction::I64x2ExtractLane { lane },
        I64x2ReplaceLane { lane } => wasm_encoder::Instruction::I64x2ReplaceLane { lane },
        F32x4ExtractLane { lane } => wasm_encoder::Instruction::F32x4ExtractLane { lane },
        F32x4ReplaceLane { lane } => wasm_encoder::Instruction::F32x4ReplaceLane { lane },
        F64x2ExtractLane { lane } => wasm_encoder::Instruction::F64x2ExtractLane { lane },
        F64x2ReplaceLane { lane } => wasm_encoder::Instruction::F64x2ReplaceLane { lane },
        I8x16Swizzle => wasm_encoder::Instruction::I8x16Swizzle,
        I8x16Splat => wasm_encoder::Instruction::I8x16Splat,
        I16x8Splat => wasm_encoder::Instruction::I16x8Splat,
        I32x4Splat => wasm_encoder::Instruction::I32x4Splat,
        I64x2Splat => wasm_encoder::Instruction::I64x2Splat,
        F32x4Splat => wasm_encoder::Instruction::F32x4Splat,
        F64x2Splat => wasm_encoder::Instruction::F64x2Splat,
        I8x16Eq => wasm_encoder::Instruction::I8x16Eq,
        I8x16Ne => wasm_encoder::Instruction::I8x16Ne,
        I8x16LtS => wasm_encoder::Instruction::I8x16LtS,
        I8x16LtU => wasm_encoder::Instruction::I8x16LtU,
        I8x16GtS => wasm_encoder::Instruction::I8x16GtS,
        I8x16GtU => wasm_encoder::Instruction::I8x16GtU,
        I8x16LeS => wasm_encoder::Instruction::I8x16LeS,
        I8x16LeU => wasm_encoder::Instruction::I8x16LeU,
        I8x16GeS => wasm_encoder::Instruction::I8x16GeS,
        I8x16GeU => wasm_encoder::Instruction::I8x16GeU,
        I16x8Eq => wasm_encoder::Instruction::I16x8Eq,
        I16x8Ne => wasm_encoder::Instruction::I16x8Ne,
        I16x8LtS => wasm_encoder::Instruction::I16x8LtS,
        I16x8LtU => wasm_encoder::Instruction::I16x8LtU,
        I16x8GtS => wasm_encoder::Instruction::I16x8GtS,
        I16x8GtU => wasm_encoder::Instruction::I16x8GtU,
        I16x8LeS => wasm_encoder::Instruction::I16x8LeS,
        I16x8LeU => wasm_encoder::Instruction::I16x8LeU,
        I16x8GeS => wasm_encoder::Instruction::I16x8GeS,
        I16x8GeU => wasm_encoder::Instruction::I16x8GeU,
        I32x4Eq => wasm_encoder::Instruction::I32x4Eq,
        I32x4Ne => wasm_encoder::Instruction::I32x4Ne,
        I32x4LtS => wasm_encoder::Instruction::I32x4LtS,
        I32x4LtU => wasm_encoder::Instruction::I32x4LtU,
        I32x4GtS => wasm_encoder::Instruction::I32x4GtS,
        I32x4GtU => wasm_encoder::Instruction::I32x4GtU,
        I32x4LeS => wasm_encoder::Instruction::I32x4LeS,
        I32x4LeU => wasm_encoder::Instruction::I32x4LeU,
        I32x4GeS => wasm_encoder::Instruction::I32x4GeS,
        I32x4GeU => wasm_encoder::Instruction::I32x4GeU,
        I64x2Eq => wasm_encoder::Instruction::I64x2Eq,
        I64x2Ne => wasm_encoder::Instruction::I64x2Ne,
        I64x2LtS => wasm_encoder::Instruction::I64x2LtS,
        I64x2GtS => wasm_encoder::Instruction::I64x2GtS,
        I64x2LeS => wasm_encoder::Instruction::I64x2LeS,
        I64x2GeS => wasm_encoder::Instruction::I64x2GeS,
        F32x4Eq => wasm_encoder::Instruction::F32x4Eq,
        F32x4Ne => wasm_encoder::Instruction::F32x4Ne,
        F32x4Lt => wasm_encoder::Instruction::F32x4Lt,
        F32x4Gt => wasm_encoder::Instruction::F32x4Gt,
        F32x4Le => wasm_encoder::Instruction::F32x4Le,
        F32x4Ge => wasm_encoder::Instruction::F32x4Ge,
        F64x2Eq => wasm_encoder::Instruction::F64x2Eq,
        F64x2Ne => wasm_encoder::Instruction::F64x2Ne,
        F64x2Lt => wasm_encoder::Instruction::F64x2Lt,
        F64x2Gt => wasm_encoder::Instruction::F64x2Gt,
        F64x2Le => wasm_encoder::Instruction::F64x2Le,
        F64x2Ge => wasm_encoder::Instruction::F64x2Ge,
        V128Not => wasm_encoder::Instruction::V128Not,
        V128And => wasm_encoder::Instruction::V128And,
        V128AndNot => wasm_encoder::Instruction::V128AndNot,
        V128Or => wasm_encoder::Instruction::V128Or,
        V128Xor => wasm_encoder::Instruction::V128Xor,
        V128Bitselect => wasm_encoder::Instruction::V128Bitselect,
        V128AnyTrue => wasm_encoder::Instruction::V128AnyTrue,
        I8x16Abs => wasm_encoder::Instruction::I8x16Abs,
        I8x16Neg => wasm_encoder::Instruction::I8x16Neg,
        I8x16Popcnt => wasm_encoder::Instruction::I8x16Popcnt,
        I8x16AllTrue => wasm_encoder::Instruction::I8x16AllTrue,
        I8x16Bitmask => wasm_encoder::Instruction::I8x16Bitmask,
        I8x16NarrowI16x8S => wasm_encoder::Instruction::I8x16NarrowI16x8S,
        I8x16NarrowI16x8U => wasm_encoder::Instruction::I8x16NarrowI16x8U,
        I8x16Shl => wasm_encoder::Instruction::I8x16Shl,
        I8x16ShrS => wasm_encoder::Instruction::I8x16ShrS,
        I8x16ShrU => wasm_encoder::Instruction::I8x16ShrU,
        I8x16Add => wasm_encoder::Instruction::I8x16Add,
        I8x16AddSatS => wasm_encoder::Instruction::I8x16AddSatS,
        I8x16AddSatU => wasm_encoder::Instruction::I8x16AddSatU,
        I8x16Sub => wasm_encoder::Instruction::I8x16Sub,
        I8x16SubSatS => wasm_encoder::Instruction::I8x16SubSatS,
        I8x16SubSatU => wasm_encoder::Instruction::I8x16SubSatU,
        I8x16MinS => wasm_encoder::Instruction::I8x16MinS,
        I8x16MinU => wasm_encoder::Instruction::I8x16MinU,
        I8x16MaxS => wasm_encoder::Instruction::I8x16MaxS,
        I8x16MaxU => wasm_encoder::Instruction::I8x16MaxU,
        I8x16RoundingAverageU => wasm_encoder::Instruction::I8x16RoundingAverageU,
        I16x8ExtAddPairwiseI8x16S => wasm_encoder::Instruction::I16x8ExtAddPairwiseI8x16S,
        I16x8ExtAddPairwiseI8x16U => wasm_encoder::Instruction::I16x8ExtAddPairwiseI8x16U,
        I16x8Abs => wasm_encoder::Instruction::I16x8Abs,
        I16x8Neg => wasm_encoder::Instruction::I16x8Neg,
        I16x8Q15MulrSatS => wasm_encoder::Instruction::I16x8Q15MulrSatS,
        I16x8AllTrue => wasm_encoder::Instruction::I16x8AllTrue,
        I16x8Bitmask => wasm_encoder::Instruction::I16x8Bitmask,
        I16x8NarrowI32x4S => wasm_encoder::Instruction::I16x8NarrowI32x4S,
        I16x8NarrowI32x4U => wasm_encoder::Instruction::I16x8NarrowI32x4U,
        I16x8ExtendLowI8x16S => wasm_encoder::Instruction::I16x8ExtendLowI8x16S,
        I16x8ExtendHighI8x16S => wasm_encoder::Instruction::I16x8ExtendHighI8x16S,
        I16x8ExtendLowI8x16U => wasm_encoder::Instruction::I16x8ExtendLowI8x16U,
        I16x8ExtendHighI8x16U => wasm_encoder::Instruction::I16x8ExtendHighI8x16U,
        I16x8Shl => wasm_encoder::Instruction::I16x8Shl,
        I16x8ShrS => wasm_encoder::Instruction::I16x8ShrS,
        I16x8ShrU => wasm_encoder::Instruction::I16x8ShrU,
        I16x8Add => wasm_encoder::Instruction::I16x8Add,
        I16x8AddSatS => wasm_encoder::Instruction::I16x8AddSatS,
        I16x8AddSatU => wasm_encoder::Instruction::I16x8AddSatU,
        I16x8Sub => wasm_encoder::Instruction::I16x8Sub,
        I16x8SubSatS => wasm_encoder::Instruction::I16x8SubSatS,
        I16x8SubSatU => wasm_encoder::Instruction::I16x8SubSatU,
        I16x8Mul => wasm_encoder::Instruction::I16x8Mul,
        I16x8MinS => wasm_encoder::Instruction::I16x8MinS,
        I16x8MinU => wasm_encoder::Instruction::I16x8MinU,
        I16x8MaxS => wasm_encoder::Instruction::I16x8MaxS,
        I16x8MaxU => wasm_encoder::Instruction::I16x8MaxU,
        I16x8RoundingAverageU => wasm_encoder::Instruction::I16x8RoundingAverageU,
        I16x8ExtMulLowI8x16S => wasm_encoder::Instruction::I16x8ExtMulLowI8x16S,
        I16x8ExtMulHighI8x16S => wasm_encoder::Instruction::I16x8ExtMulHighI8x16S,
        I16x8ExtMulLowI8x16U => wasm_encoder::Instruction::I16x8ExtMulLowI8x16U,
        I16x8ExtMulHighI8x16U => wasm_encoder::Instruction::I16x8ExtMulHighI8x16U,
        I32x4ExtAddPairwiseI16x8S => wasm_encoder::Instruction::I32x4ExtAddPairwiseI16x8S,
        I32x4ExtAddPairwiseI16x8U => wasm_encoder::Instruction::I32x4ExtAddPairwiseI16x8U,
        I32x4Abs => wasm_encoder::Instruction::I32x4Abs,
        I32x4Neg => wasm_encoder::Instruction::I32x4Neg,
        I32x4AllTrue => wasm_encoder::Instruction::I32x4AllTrue,
        I32x4Bitmask => wasm_encoder::Instruction::I32x4Bitmask,
        I32x4ExtendLowI16x8S => wasm_encoder::Instruction::I32x4ExtendLowI16x8S,
        I32x4ExtendHighI16x8S => wasm_encoder::Instruction::I32x4ExtendHighI16x8S,
        I32x4ExtendLowI16x8U => wasm_encoder::Instruction::I32x4ExtendLowI16x8U,
        I32x4ExtendHighI16x8U => wasm_encoder::Instruction::I32x4ExtendHighI16x8U,
        I32x4Shl => wasm_encoder::Instruction::I32x4Shl,
        I32x4ShrS => wasm_encoder::Instruction::I32x4ShrS,
        I32x4ShrU => wasm_encoder::Instruction::I32x4ShrU,
        I32x4Add => wasm_encoder::Instruction::I32x4Add,
        I32x4Sub => wasm_encoder::Instruction::I32x4Sub,
        I32x4Mul => wasm_encoder::Instruction::I32x4Mul,
        I32x4MinS => wasm_encoder::Instruction::I32x4MinS,
        I32x4MinU => wasm_encoder::Instruction::I32x4MinU,
        I32x4MaxS => wasm_encoder::Instruction::I32x4MaxS,
        I32x4MaxU => wasm_encoder::Instruction::I32x4MaxU,
        I32x4DotI16x8S => wasm_encoder::Instruction::I32x4DotI16x8S,
        I32x4ExtMulLowI16x8S => wasm_encoder::Instruction::I32x4ExtMulLowI16x8S,
        I32x4ExtMulHighI16x8S => wasm_encoder::Instruction::I32x4ExtMulHighI16x8S,
        I32x4ExtMulLowI16x8U => wasm_encoder::Instruction::I32x4ExtMulLowI16x8U,
        I32x4ExtMulHighI16x8U => wasm_encoder::Instruction::I32x4ExtMulHighI16x8U,
        I64x2Abs => wasm_encoder::Instruction::I64x2Abs,
        I64x2Neg => wasm_encoder::Instruction::I64x2Neg,
        I64x2AllTrue => wasm_encoder::Instruction::I64x2AllTrue,
        I64x2Bitmask => wasm_encoder::Instruction::I64x2Bitmask,
        I64x2ExtendLowI32x4S => wasm_encoder::Instruction::I64x2ExtendLowI32x4S,
        I64x2ExtendHighI32x4S => wasm_encoder::Instruction::I64x2ExtendHighI32x4S,
        I64x2ExtendLowI32x4U => wasm_encoder::Instruction::I64x2ExtendLowI32x4U,
        I64x2ExtendHighI32x4U => wasm_encoder::Instruction::I64x2ExtendHighI32x4U,
        I64x2Shl => wasm_encoder::Instruction::I64x2Shl,
        I64x2ShrS => wasm_encoder::Instruction::I64x2ShrS,
        I64x2ShrU => wasm_encoder::Instruction::I64x2ShrU,
        I64x2Add => wasm_encoder::Instruction::I64x2Add,
        I64x2Sub => wasm_encoder::Instruction::I64x2Sub,
        I64x2Mul => wasm_encoder::Instruction::I64x2Mul,
        I64x2ExtMulLowI32x4S => wasm_encoder::Instruction::I64x2ExtMulLowI32x4S,
        I64x2ExtMulHighI32x4S => wasm_encoder::Instruction::I64x2ExtMulHighI32x4S,
        I64x2ExtMulLowI32x4U => wasm_encoder::Instruction::I64x2ExtMulLowI32x4U,
        I64x2ExtMulHighI32x4U => wasm_encoder::Instruction::I64x2ExtMulHighI32x4U,
        F32x4Ceil => wasm_encoder::Instruction::F32x4Ceil,
        F32x4Floor => wasm_encoder::Instruction::F32x4Floor,
        F32x4Trunc => wasm_encoder::Instruction::F32x4Trunc,
        F32x4Nearest => wasm_encoder::Instruction::F32x4Nearest,
        F32x4Abs => wasm_encoder::Instruction::F32x4Abs,
        F32x4Neg => wasm_encoder::Instruction::F32x4Neg,
        F32x4Sqrt => wasm_encoder::Instruction::F32x4Sqrt,
        F32x4Add => wasm_encoder::Instruction::F32x4Add,
        F32x4Sub => wasm_encoder::Instruction::F32x4Sub,
        F32x4Mul => wasm_encoder::Instruction::F32x4Mul,
        F32x4Div => wasm_encoder::Instruction::F32x4Div,
        F32x4Min => wasm_encoder::Instruction::F32x4Min,
        F32x4Max => wasm_encoder::Instruction::F32x4Max,
        F32x4PMin => wasm_encoder::Instruction::F32x4PMin,
        F32x4PMax => wasm_encoder::Instruction::F32x4PMax,
        F64x2Ceil => wasm_encoder::Instruction::F64x2Ceil,
        F64x2Floor => wasm_encoder::Instruction::F64x2Floor,
        F64x2Trunc => wasm_encoder::Instruction::F64x2Trunc,
        F64x2Nearest => wasm_encoder::Instruction::F64x2Nearest,
        F64x2Abs => wasm_encoder::Instruction::F64x2Abs,
        F64x2Neg => wasm_encoder::Instruction::F64x2Neg,
        F64x2Sqrt => wasm_encoder::Instruction::F64x2Sqrt,
        F64x2Add => wasm_encoder::Instruction::F64x2Add,
        F64x2Sub => wasm_encoder::Instruction::F64x2Sub,
        F64x2Mul => wasm_encoder::Instruction::F64x2Mul,
        F64x2Div => wasm_encoder::Instruction::F64x2Div,
        F64x2Min => wasm_encoder::Instruction::F64x2Min,
        F64x2Max => wasm_encoder::Instruction::F64x2Max,
        F64x2PMin => wasm_encoder::Instruction::F64x2PMin,
        F64x2PMax => wasm_encoder::Instruction::F64x2PMax,
        I32x4TruncSatF32x4S => wasm_encoder::Instruction::I32x4TruncSatF32x4S,
        I32x4TruncSatF32x4U => wasm_encoder::Instruction::I32x4TruncSatF32x4U,
        F32x4ConvertI32x4S => wasm_encoder::Instruction::F32x4ConvertI32x4S,
        F32x4ConvertI32x4U => wasm_encoder::Instruction::F32x4ConvertI32x4U,
        I32x4TruncSatF64x2SZero => wasm_encoder::Instruction::I32x4TruncSatF64x2SZero,
        I32x4TruncSatF64x2UZero => wasm_encoder::Instruction::I32x4TruncSatF64x2UZero,
        F64x2ConvertLowI32x4S => wasm_encoder::Instruction::F64x2ConvertLowI32x4S,
        F64x2ConvertLowI32x4U => wasm_encoder::Instruction::F64x2ConvertLowI32x4U,
        F32x4DemoteF64x2Zero => wasm_encoder::Instruction::F32x4DemoteF64x2Zero,
        F64x2PromoteLowF32x4 => wasm_encoder::Instruction::F64x2PromoteLowF32x4,
    }
}
