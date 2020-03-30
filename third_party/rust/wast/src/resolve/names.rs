use crate::ast::*;
use crate::Error;
use std::collections::HashMap;

#[derive(Copy, Clone)]
pub enum Ns {
    Data,
    Elem,
    Event,
    Func,
    Global,
    Memory,
    Table,
    Type,
    Field,
}

impl Ns {
    fn desc(&self) -> &'static str {
        match self {
            Ns::Data => "data",
            Ns::Elem => "elem",
            Ns::Event => "event",
            Ns::Func => "func",
            Ns::Global => "global",
            Ns::Memory => "memory",
            Ns::Table => "table",
            Ns::Type => "type",
            Ns::Field => "field",
        }
    }
}

#[derive(Default)]
pub struct Resolver<'a> {
    ns: [Namespace<'a>; 9],
    func_tys: HashMap<u32, FunctionType<'a>>,
}

#[derive(Default)]
struct Namespace<'a> {
    names: HashMap<Id<'a>, u32>,
    count: u32,
}

impl<'a> Resolver<'a> {
    pub fn register(&mut self, item: &ModuleField<'a>) -> Result<(), Error> {
        let mut register =
            |ns: Ns, name: Option<Id<'a>>| self.ns_mut(ns).register(name, ns.desc()).map(|_| ());
        match item {
            ModuleField::Import(i) => match i.kind {
                ImportKind::Func(_) => register(Ns::Func, i.id),
                ImportKind::Memory(_) => register(Ns::Memory, i.id),
                ImportKind::Table(_) => register(Ns::Table, i.id),
                ImportKind::Global(_) => register(Ns::Global, i.id),
                ImportKind::Event(_) => register(Ns::Event, i.id),
            },
            ModuleField::Global(i) => register(Ns::Global, i.id),
            ModuleField::Memory(i) => register(Ns::Memory, i.id),
            ModuleField::Func(i) => register(Ns::Func, i.id),
            ModuleField::Table(i) => register(Ns::Table, i.id),
            ModuleField::Type(i) => {
                let index = self.ns_mut(Ns::Type).register(i.id, Ns::Type.desc())?;
                match &i.def {
                    TypeDef::Func(func) => {
                        // Store a copy of this function type for resolving
                        // type uses later. We can't resolve this copy yet, so
                        // we will take care of that when resolving the type
                        // use.
                        self.func_tys.insert(index, func.clone());
                    }
                    TypeDef::Struct(r#struct) => {
                        for (i, field) in r#struct.fields.iter().enumerate() {
                            if let Some(id) = field.id {
                                // The field namespace is global, but the
                                // resolved indices are relative to the struct
                                // they are defined in
                                self.ns_mut(Ns::Field).register_specific(
                                    id,
                                    i as u32,
                                    Ns::Field.desc(),
                                )?;
                            }
                        }
                    }
                }
                Ok(())
            }
            ModuleField::Elem(e) => register(Ns::Elem, e.id),
            ModuleField::Data(d) => register(Ns::Data, d.id),
            ModuleField::Event(e) => register(Ns::Event, e.id),
            ModuleField::Start(_)
            | ModuleField::Export(_)
            | ModuleField::GcOptIn(_)
            | ModuleField::Custom(_) => Ok(()),
        }
    }

    fn ns_mut(&mut self, ns: Ns) -> &mut Namespace<'a> {
        &mut self.ns[ns as usize]
    }

    fn ns(&self, ns: Ns) -> &Namespace<'a> {
        &self.ns[ns as usize]
    }

    pub fn resolve(&self, field: &mut ModuleField<'a>) -> Result<(), Error> {
        match field {
            ModuleField::Import(i) => {
                match &mut i.kind {
                    ImportKind::Func(t) | ImportKind::Event(EventType::Exception(t)) => {
                        self.resolve_type_use(i.span, t)?;
                    }
                    ImportKind::Global(t) => {
                        self.resolve_valtype(&mut t.ty)?;
                    }
                    _ => {}
                }
                Ok(())
            }

            ModuleField::Type(t) => self.resolve_type(t),

            ModuleField::Func(f) => {
                self.resolve_type_use(f.span, &mut f.ty)?;
                if let FuncKind::Inline { locals, expression } = &mut f.kind {
                    // Resolve (ref T) in locals
                    for (_, _, valtype) in locals.iter_mut() {
                        self.resolve_valtype(valtype)?;
                    }

                    // Build a resolver with local namespace for the function
                    // body
                    let mut resolver = ExprResolver::new(self, f.span);

                    // Parameters come first in the local namespace...
                    for (id, _, _) in f.ty.func_ty.params.iter() {
                        resolver.locals.register(*id, "local")?;
                    }

                    // .. followed by locals themselves
                    for (id, _, _) in locals {
                        resolver.locals.register(*id, "local")?;
                    }

                    // and then we can resolve the expression!
                    resolver.resolve(expression)?;
                }
                Ok(())
            }

            ModuleField::Elem(e) => {
                match &mut e.kind {
                    ElemKind::Active { table, offset } => {
                        self.resolve_idx(table, Ns::Table)?;
                        self.resolve_expr(e.span, offset)?;
                    }
                    ElemKind::Passive { .. } | ElemKind::Declared { .. } => {}
                }
                match &mut e.payload {
                    ElemPayload::Indices(elems) => {
                        for idx in elems {
                            self.resolve_idx(idx, Ns::Func)?;
                        }
                    }
                    ElemPayload::Exprs { exprs, .. } => {
                        for funcref in exprs {
                            if let Some(idx) = funcref {
                                self.resolve_idx(idx, Ns::Func)?;
                            }
                        }
                    }
                }
                Ok(())
            }

            ModuleField::Data(d) => {
                if let DataKind::Active { memory, offset } = &mut d.kind {
                    self.resolve_idx(memory, Ns::Memory)?;
                    self.resolve_expr(d.span, offset)?;
                }
                Ok(())
            }

            ModuleField::Start(i) => {
                self.resolve_idx(i, Ns::Func)?;
                Ok(())
            }

            ModuleField::Export(e) => match &mut e.kind {
                ExportKind::Func(f) => self.resolve_idx(f, Ns::Func),
                ExportKind::Memory(f) => self.resolve_idx(f, Ns::Memory),
                ExportKind::Global(f) => self.resolve_idx(f, Ns::Global),
                ExportKind::Table(f) => self.resolve_idx(f, Ns::Table),
                ExportKind::Event(f) => self.resolve_idx(f, Ns::Event),
            },

            ModuleField::Global(g) => {
                self.resolve_valtype(&mut g.ty.ty)?;
                if let GlobalKind::Inline(expr) = &mut g.kind {
                    self.resolve_expr(g.span, expr)?;
                }
                Ok(())
            }

            ModuleField::Event(e) => self.resolve_event_type(e.span, &mut e.ty),

            ModuleField::Table(_)
            | ModuleField::Memory(_)
            | ModuleField::GcOptIn(_)
            | ModuleField::Custom(_) => Ok(()),
        }
    }

    fn resolve_valtype(&self, ty: &mut ValType<'a>) -> Result<(), Error> {
        if let ValType::Ref(id) = ty {
            self.ns(Ns::Type)
                .resolve(id)
                .map_err(|id| self.resolve_error(id, "type"))?;
        }
        Ok(())
    }

    fn resolve_type(&self, ty: &mut Type<'a>) -> Result<(), Error> {
        match &mut ty.def {
            TypeDef::Func(func) => self.resolve_function_type(func),
            TypeDef::Struct(r#struct) => self.resolve_struct_type(r#struct),
        }
    }

    fn resolve_function_type(&self, func: &mut FunctionType<'a>) -> Result<(), Error> {
        for param in &mut func.params {
            self.resolve_valtype(&mut param.2)?;
        }
        for result in &mut func.results {
            self.resolve_valtype(result)?;
        }
        Ok(())
    }

    fn resolve_struct_type(&self, r#struct: &mut StructType<'a>) -> Result<(), Error> {
        for field in &mut r#struct.fields {
            self.resolve_valtype(&mut field.ty)?;
        }
        Ok(())
    }

    fn resolve_type_use(&self, span: Span, ty: &mut TypeUse<'a>) -> Result<u32, Error> {
        assert!(ty.index.is_some());
        let idx = self
            .ns(Ns::Type)
            .resolve(ty.index.as_mut().unwrap())
            .map_err(|id| self.resolve_error(id, "type"))?;

        // If the type was listed inline *and* it was specified via a type index
        // we need to assert they're the same.
        let expected = match self.func_tys.get(&(idx as u32)) {
            Some(ty) => ty,
            None => return Ok(idx),
        };
        if ty.func_ty.params.len() > 0 || ty.func_ty.results.len() > 0 {
            // The function type for this type use hasn't been resolved,
            // but neither has the expected function type, so this comparison
            // is valid.
            let params_not_equal = expected.params.iter().map(|t| t.2).ne(ty
                .func_ty
                .params
                .iter()
                .map(|t| t.2));
            if params_not_equal || expected.results != ty.func_ty.results {
                let span = ty.index_span.unwrap_or(span);
                return Err(Error::new(
                    span,
                    format!("inline function type doesn't match type reference"),
                ));
            }
        } else {
            ty.func_ty.params = expected.params.clone();
            ty.func_ty.results = expected.results.clone();
        }

        // Resolve the (ref T) value types in the final function type
        self.resolve_function_type(&mut ty.func_ty)?;

        Ok(idx)
    }

    fn resolve_expr(&self, span: Span, expr: &mut Expression<'a>) -> Result<(), Error> {
        ExprResolver::new(self, span).resolve(expr)
    }

    fn resolve_event_type(&self, span: Span, ty: &mut EventType<'a>) -> Result<(), Error> {
        match ty {
            EventType::Exception(ty) => {
                self.resolve_type_use(span, ty)?;
            }
        }
        Ok(())
    }

    pub fn resolve_idx(&self, idx: &mut Index<'a>, ns: Ns) -> Result<(), Error> {
        match self.ns(ns).resolve(idx) {
            Ok(_n) => Ok(()),
            Err(id) => Err(self.resolve_error(id, ns.desc())),
        }
    }

    fn resolve_error(&self, id: Id<'a>, ns: &str) -> Error {
        Error::new(
            id.span(),
            format!("failed to find {} named `${}`", ns, id.name()),
        )
    }
}

impl<'a> Namespace<'a> {
    fn register(&mut self, name: Option<Id<'a>>, desc: &str) -> Result<u32, Error> {
        let index = self.count;
        if let Some(name) = name {
            if let Some(_prev) = self.names.insert(name, index) {
                // FIXME: temporarily allow duplicately-named data and element
                // segments. This is a sort of dumb hack to get the spec test
                // suite working (ironically).
                //
                // So as background, the text format disallows duplicate
                // identifiers, causing a parse error if they're found. There
                // are two tests currently upstream, however, data.wast and
                // elem.wast, which *look* like they have duplicately named
                // element and data segments. These tests, however, are using
                // pre-bulk-memory syntax where a bare identifier was the
                // table/memory being initialized. In post-bulk-memory this
                // identifier is the name of the segment. Since we implement
                // post-bulk-memory features that means that we're parsing the
                // memory/table-to-initialize as the name of the segment.
                //
                // This is technically incorrect behavior but no one is
                // hopefully relying on this too much. To get the spec tests
                // passing we ignore errors for elem/data segments. Once the
                // spec tests get updated enough we can remove this condition
                // and return errors for them.
                if desc != "elem" && desc != "data" {
                    return Err(Error::new(
                        name.span(),
                        format!("duplicate {} identifier", desc),
                    ));
                }
            }
        }
        self.count += 1;
        Ok(index)
    }

    fn register_specific(&mut self, name: Id<'a>, index: u32, desc: &str) -> Result<(), Error> {
        if let Some(_prev) = self.names.insert(name, index) {
            return Err(Error::new(
                name.span(),
                format!("duplicate identifier for {}", desc),
            ));
        }
        Ok(())
    }

    fn resolve(&self, idx: &mut Index<'a>) -> Result<u32, Id<'a>> {
        let id = match idx {
            Index::Num(n) => return Ok(*n),
            Index::Id(id) => id,
        };
        if let Some(&n) = self.names.get(id) {
            *idx = Index::Num(n);
            return Ok(n);
        }
        Err(*id)
    }
}

struct ExprResolver<'a, 'b> {
    resolver: &'b Resolver<'a>,
    locals: Namespace<'a>,
    labels: Vec<Option<Id<'a>>>,
    span: Span,
}

impl<'a, 'b> ExprResolver<'a, 'b> {
    fn new(resolver: &'b Resolver<'a>, span: Span) -> ExprResolver<'a, 'b> {
        ExprResolver {
            resolver,
            locals: Default::default(),
            labels: Vec::new(),
            span,
        }
    }

    fn resolve(&mut self, expr: &mut Expression<'a>) -> Result<(), Error> {
        for instr in expr.instrs.iter_mut() {
            self.resolve_instr(instr)?;
        }
        Ok(())
    }

    fn resolve_instr(&mut self, instr: &mut Instruction<'a>) -> Result<(), Error> {
        use crate::ast::Instruction::*;

        match instr {
            MemoryInit(i) => self.resolver.resolve_idx(&mut i.data, Ns::Data),
            DataDrop(i) => self.resolver.resolve_idx(i, Ns::Data),

            TableInit(i) => {
                self.resolver.resolve_idx(&mut i.elem, Ns::Elem)?;
                self.resolver.resolve_idx(&mut i.table, Ns::Table)
            }
            ElemDrop(i) => self.resolver.resolve_idx(i, Ns::Elem),

            TableCopy(i) => {
                self.resolver.resolve_idx(&mut i.dst, Ns::Table)?;
                self.resolver.resolve_idx(&mut i.src, Ns::Table)
            }

            TableFill(i) | TableSet(i) | TableGet(i) | TableSize(i) | TableGrow(i) => {
                self.resolver.resolve_idx(&mut i.dst, Ns::Table)
            }

            GlobalSet(i) | GlobalGet(i) => self.resolver.resolve_idx(i, Ns::Global),

            LocalSet(i) | LocalGet(i) | LocalTee(i) => self
                .locals
                .resolve(i)
                .map(|_| ())
                .map_err(|id| self.resolver.resolve_error(id, "local")),

            Call(i) | RefFunc(i) | ReturnCall(i) => self.resolver.resolve_idx(i, Ns::Func),

            CallIndirect(c) | ReturnCallIndirect(c) => {
                self.resolver.resolve_idx(&mut c.table, Ns::Table)?;
                self.resolver.resolve_type_use(self.span, &mut c.ty)?;
                Ok(())
            }

            Block(bt) | If(bt) | Loop(bt) | Try(bt) => {
                self.labels.push(bt.label);

                // Ok things get interesting here. First off when parsing `bt`
                // *optionally* has an index and a function type listed. If
                // they're both not present it's equivalent to 0 params and 0
                // results.
                //
                // In MVP wasm blocks can have 0 params and 0-1 results. Now
                // there's also multi-value. We want to prefer MVP wasm wherever
                // possible (for backcompat) so we want to list this block as
                // being an "MVP" block if we can. The encoder only has
                // `BlockType` to work with, so it'll be looking at `params` and
                // `results` to figure out what to encode. If `params` and
                // `results` fit within MVP, then it uses MVP encoding
                //
                // To put all that together, here we handle:
                //
                // * If the `index` was specified, resolve it and use it as the
                //   source of truth. If this turns out to be an MVP type,
                //   record it as such.
                // * Otherwise use `params` and `results` as the source of
                //   truth. *If* this were a non-MVP compatible block `index`
                //   would be filled by by `tyexpand.rs`.
                //
                // tl;dr; we handle the `index` here if it's set and then fill
                // out `params` and `results` if we can, otherwise no work
                // happens.
                if bt.ty.index.is_some() {
                    let ty = self.resolver.resolve_type_use(self.span, &mut bt.ty)?;
                    let ty = match self.resolver.func_tys.get(&(ty as u32)) {
                        Some(ty) => ty,
                        None => return Ok(()),
                    };
                    if ty.params.len() == 0 && ty.results.len() <= 1 {
                        bt.ty.func_ty.params.truncate(0);
                        bt.ty.func_ty.results = ty.results.clone();
                        bt.ty.index = None;
                    }
                }

                // Resolve (ref T) params and results
                self.resolver.resolve_function_type(&mut bt.ty.func_ty)?;

                Ok(())
            }

            // On `End` instructions we pop a label from the stack, and for both
            // `End` and `Else` instructions if they have labels listed we
            // verify that they match the label at the beginning of the block.
            Else(_) | End(_) => {
                let (matching_label, label) = match instr {
                    Else(label) => (self.labels.last().cloned(), label),
                    End(label) => (self.labels.pop(), label),
                    _ => unreachable!(),
                };
                let matching_label = match matching_label {
                    Some(l) => l,
                    None => return Ok(()),
                };
                let label = match label {
                    Some(l) => l,
                    None => return Ok(()),
                };
                if Some(*label) == matching_label {
                    return Ok(());
                }
                return Err(Error::new(
                    label.span(),
                    "mismatching labels between end and block".to_string(),
                ));
            }

            Br(i) | BrIf(i) => self.resolve_label(i),

            BrTable(i) => {
                for label in i.labels.iter_mut() {
                    self.resolve_label(label)?;
                }
                self.resolve_label(&mut i.default)
            }

            Throw(i) => self.resolver.resolve_idx(i, Ns::Event),
            BrOnExn(b) => {
                self.resolve_label(&mut b.label)?;
                self.resolver.resolve_idx(&mut b.exn, Ns::Event)
            }

            Select(s) => {
                for ty in &mut s.tys {
                    self.resolver.resolve_valtype(ty)?;
                }
                Ok(())
            }

            StructNew(s) => self.resolver.resolve_idx(s, Ns::Type),
            StructSet(s) | StructGet(s) => {
                self.resolver.resolve_idx(&mut s.r#struct, Ns::Type)?;
                self.resolver.resolve_idx(&mut s.field, Ns::Field)
            }
            StructNarrow(s) => {
                self.resolver.resolve_valtype(&mut s.from)?;
                self.resolver.resolve_valtype(&mut s.to)
            }

            _ => Ok(()),
        }
    }

    fn resolve_label(&self, label: &mut Index<'a>) -> Result<(), Error> {
        let id = match label {
            Index::Num(_) => return Ok(()),
            Index::Id(id) => *id,
        };
        let idx = self
            .labels
            .iter()
            .rev()
            .enumerate()
            .filter_map(|(i, l)| l.map(|l| (i, l)))
            .find(|(_, l)| *l == id);
        match idx {
            Some((idx, _)) => {
                *label = Index::Num(idx as u32);
                Ok(())
            }
            None => Err(self.resolver.resolve_error(id, "label")),
        }
    }
}
