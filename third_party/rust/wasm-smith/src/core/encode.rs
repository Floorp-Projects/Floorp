use super::*;
use std::convert::TryFrom;

impl Module {
    /// Encode this Wasm module into bytes.
    pub fn to_bytes(&self) -> Vec<u8> {
        self.encoded().finish()
    }

    fn encoded(&self) -> wasm_encoder::Module {
        let mut module = wasm_encoder::Module::new();

        self.encode_types(&mut module);
        self.encode_imports(&mut module);
        self.encode_funcs(&mut module);
        self.encode_tables(&mut module);
        self.encode_memories(&mut module);
        self.encode_tags(&mut module);
        self.encode_globals(&mut module);
        self.encode_exports(&mut module);
        self.encode_start(&mut module);
        self.encode_elems(&mut module);
        self.encode_data_count(&mut module);
        self.encode_code(&mut module);
        self.encode_data(&mut module);

        module
    }

    fn encode_types(&self, module: &mut wasm_encoder::Module) {
        if !self.should_encode_types {
            return;
        }

        let mut section = wasm_encoder::TypeSection::new();
        for ty in &self.types {
            match ty {
                Type::Func(ty) => {
                    section.function(ty.params.iter().cloned(), ty.results.iter().cloned());
                }
            }
        }
        module.section(&section);
    }

    fn encode_imports(&self, module: &mut wasm_encoder::Module) {
        if !self.should_encode_imports {
            return;
        }

        let mut section = wasm_encoder::ImportSection::new();
        for im in &self.imports {
            section.import(
                &im.module,
                &im.field,
                translate_entity_type(&im.entity_type),
            );
        }
        module.section(&section);
    }

    fn encode_tags(&self, module: &mut wasm_encoder::Module) {
        if self.num_defined_tags == 0 {
            return;
        }
        let mut tags = wasm_encoder::TagSection::new();
        for tag in self.tags[self.tags.len() - self.num_defined_tags..].iter() {
            tags.tag(wasm_encoder::TagType {
                kind: wasm_encoder::TagKind::Exception,
                func_type_idx: tag.func_type_idx,
            });
        }
        module.section(&tags);
    }

    fn encode_funcs(&self, module: &mut wasm_encoder::Module) {
        if self.num_defined_funcs == 0 {
            return;
        }
        let mut funcs = wasm_encoder::FunctionSection::new();
        for (ty, _) in self.funcs[self.funcs.len() - self.num_defined_funcs..].iter() {
            funcs.function(*ty);
        }
        module.section(&funcs);
    }

    fn encode_tables(&self, module: &mut wasm_encoder::Module) {
        if self.num_defined_tables == 0 {
            return;
        }
        let mut tables = wasm_encoder::TableSection::new();
        for t in self.tables[self.tables.len() - self.num_defined_tables..].iter() {
            tables.table(*t);
        }
        module.section(&tables);
    }

    fn encode_memories(&self, module: &mut wasm_encoder::Module) {
        if self.num_defined_memories == 0 {
            return;
        }
        let mut mems = wasm_encoder::MemorySection::new();
        for m in self.memories[self.memories.len() - self.num_defined_memories..].iter() {
            mems.memory(*m);
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
            match expr {
                GlobalInitExpr::ConstExpr(expr) => globals.global(*ty, expr),
                GlobalInitExpr::FuncRef(func) => globals.global(*ty, &ConstExpr::ref_func(*func)),
            };
        }
        module.section(&globals);
    }

    fn encode_exports(&self, module: &mut wasm_encoder::Module) {
        if self.exports.is_empty() {
            return;
        }
        let mut exports = wasm_encoder::ExportSection::new();
        for (name, kind, idx) in &self.exports {
            exports.export(name, *kind, *idx);
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
            let elements = match &el.items {
                Elements::Expressions(es) => {
                    exps.clear();
                    exps.extend(es.iter().map(|e| {
                        // TODO(nagisa): generate global.get of imported ref globals too.
                        match e {
                            Some(i) => match el.ty {
                                RefType::FUNCREF => wasm_encoder::ConstExpr::ref_func(*i),
                                _ => unreachable!(),
                            },
                            None => wasm_encoder::ConstExpr::ref_null(el.ty.heap_type),
                        }
                    }));
                    wasm_encoder::Elements::Expressions(el.ty, &exps)
                }
                Elements::Functions(fs) => wasm_encoder::Elements::Functions(fs),
            };
            match &el.kind {
                ElementKind::Active { table, offset } => {
                    let offset = match *offset {
                        Offset::Const32(n) => ConstExpr::i32_const(n),
                        Offset::Const64(n) => ConstExpr::i64_const(n),
                        Offset::Global(g) => ConstExpr::global_get(g),
                    };
                    elems.active(*table, &offset, elements);
                }
                ElementKind::Passive => {
                    elems.passive(elements);
                }
                ElementKind::Declared => {
                    elems.declared(elements);
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
            let mut func = wasm_encoder::Function::new(c.locals.iter().map(|l| (1, *l)));
            match &c.instructions {
                Instructions::Generated(instrs) => {
                    for instr in instrs {
                        func.instruction(instr);
                    }
                    func.instruction(&wasm_encoder::Instruction::End);
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
                    let offset = match *offset {
                        Offset::Const32(n) => ConstExpr::i32_const(n),
                        Offset::Const64(n) => ConstExpr::i64_const(n),
                        Offset::Global(g) => ConstExpr::global_get(g),
                    };
                    data.active(*memory_index, &offset, seg.init.iter().copied());
                }
                DataSegmentKind::Passive => {
                    data.passive(seg.init.iter().copied());
                }
            }
        }
        module.section(&data);
    }
}

pub(crate) fn translate_entity_type(ty: &EntityType) -> wasm_encoder::EntityType {
    match ty {
        EntityType::Tag(t) => wasm_encoder::EntityType::Tag(wasm_encoder::TagType {
            kind: wasm_encoder::TagKind::Exception,
            func_type_idx: t.func_type_idx,
        }),
        EntityType::Func(f, _) => wasm_encoder::EntityType::Function(*f),
        EntityType::Table(ty) => (*ty).into(),
        EntityType::Memory(m) => (*m).into(),
        EntityType::Global(g) => (*g).into(),
    }
}
