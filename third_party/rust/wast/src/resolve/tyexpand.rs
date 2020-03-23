use crate::ast::*;
use std::collections::HashMap;

#[derive(Default)]
pub struct Expander<'a> {
    pub to_prepend: Vec<ModuleField<'a>>,
    func_types: HashMap<(Vec<ValType<'a>>, Vec<ValType<'a>>), u32>,
    ntypes: u32,
}

impl<'a> Expander<'a> {
    pub fn expand(&mut self, item: &mut ModuleField<'a>) {
        match item {
            ModuleField::Type(t) => self.register_type(t),
            ModuleField::Import(i) => self.expand_import(i),
            ModuleField::Func(f) => self.expand_func(f),
            ModuleField::Global(g) => self.expand_global(g),
            ModuleField::Data(d) => self.expand_data(d),
            ModuleField::Elem(e) => self.expand_elem(e),
            ModuleField::Event(e) => self.expand_event(e),
            _ => {}
        }
    }

    fn register_type(&mut self, ty: &Type<'a>) {
        if let TypeDef::Func(func) = &ty.def {
            let key = self.key(func);
            if !self.func_types.contains_key(&key) {
                self.func_types.insert(key, self.ntypes);
            }
        }
        self.ntypes += 1;
    }

    fn expand_import(&mut self, import: &mut Import<'a>) {
        match &mut import.kind {
            ImportKind::Func(t) | ImportKind::Event(EventType::Exception(t)) => {
                self.expand_type_use(t)
            }
            _ => {}
        }
    }

    fn expand_func(&mut self, func: &mut Func<'a>) {
        self.expand_type_use(&mut func.ty);
        if let FuncKind::Inline { expression, .. } = &mut func.kind {
            self.expand_expression(expression);
        }
    }

    fn expand_global(&mut self, global: &mut Global<'a>) {
        if let GlobalKind::Inline(expr) = &mut global.kind {
            self.expand_expression(expr);
        }
    }

    fn expand_elem(&mut self, elem: &mut Elem<'a>) {
        if let ElemKind::Active { offset, .. } = &mut elem.kind {
            self.expand_expression(offset);
        }
    }

    fn expand_event(&mut self, event: &mut Event<'a>) {
        match &mut event.ty {
            EventType::Exception(ty) => self.expand_type_use(ty),
        }
    }

    fn expand_data(&mut self, data: &mut Data<'a>) {
        if let DataKind::Active { offset, .. } = &mut data.kind {
            self.expand_expression(offset);
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
            | Instruction::Try(bt) => {
                // Only actually expand `TypeUse` with an index which appends a
                // type if it looks like we need one. This way if the
                // multi-value proposal isn't enabled and/or used we won't
                // encode it.
                if bt.ty.func_ty.params.len() == 0 && bt.ty.func_ty.results.len() <= 1 {
                    return;
                }
                self.expand_type_use(&mut bt.ty)
            }
            Instruction::CallIndirect(c) | Instruction::ReturnCallIndirect(c) => {
                self.expand_type_use(&mut c.ty)
            }
            _ => {}
        }
    }

    fn expand_type_use(&mut self, item: &mut TypeUse<'a>) {
        if item.index.is_some() {
            return;
        }
        let key = self.key(&item.func_ty);
        item.index = Some(Index::Num(match self.func_types.get(&key) {
            Some(i) => *i,
            None => self.prepend(key),
        }));
    }

    fn key(&self, ty: &FunctionType<'a>) -> (Vec<ValType<'a>>, Vec<ValType<'a>>) {
        let params = ty.params.iter().map(|p| p.2).collect::<Vec<_>>();
        let results = ty.results.clone();
        (params, results)
    }

    fn prepend(&mut self, key: (Vec<ValType<'a>>, Vec<ValType<'a>>)) -> u32 {
        self.to_prepend.push(ModuleField::Type(Type {
            id: None,
            def: TypeDef::Func(FunctionType {
                params: key.0.iter().map(|t| (None, None, *t)).collect(),
                results: key.1.clone(),
            }),
        }));
        self.func_types.insert(key, self.ntypes);
        self.ntypes += 1;
        return self.ntypes - 1;
    }
}
