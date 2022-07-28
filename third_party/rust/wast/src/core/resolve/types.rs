use crate::core::*;
use crate::gensym;
use crate::token::{Index, Span};
use std::collections::HashMap;

pub fn expand<'a>(fields: &mut Vec<ModuleField<'a>>) {
    let mut expander = Expander::default();
    expander.process(fields);
}

#[derive(Default)]
pub(crate) struct Expander<'a> {
    // Maps used to "intern" types. These maps are populated as type annotations
    // are seen and inline type annotations use previously defined ones if
    // there's a match.
    func_type_to_idx: HashMap<FuncKey<'a>, Index<'a>>,

    /// Fields, during processing, which should be prepended to the
    /// currently-being-processed field. This should always be empty after
    /// processing is complete.
    to_prepend: Vec<ModuleField<'a>>,
}

impl<'a> Expander<'a> {
    fn process(&mut self, fields: &mut Vec<ModuleField<'a>>) {
        // Next we expand "header" fields which are those like types and
        // imports. In this context "header" is defined by the previous
        // `process_imports_early` annotation.
        let mut cur = 0;
        while cur < fields.len() {
            self.expand_header(&mut fields[cur]);
            for item in self.to_prepend.drain(..) {
                fields.insert(cur, item);
                cur += 1;
            }
            cur += 1;
        }

        // Next after we've done that we expand remaining fields. Note that
        // after this we actually append instead of prepend. This is because
        // injected types are intended to come at the end of the type section
        // and types will be sorted before all other items processed here in the
        // final module anyway.
        for field in fields.iter_mut() {
            self.expand(field);
        }
        fields.append(&mut self.to_prepend);
    }

    fn expand_header(&mut self, item: &mut ModuleField<'a>) {
        match item {
            ModuleField::Type(ty) => {
                let id = gensym::fill(ty.span, &mut ty.id);
                match &mut ty.def {
                    TypeDef::Func(f) => {
                        f.key().insert(self, Index::Id(id));
                    }
                    TypeDef::Array(_) | TypeDef::Struct(_) => {}
                }
            }
            _ => {}
        }
    }

    fn expand(&mut self, item: &mut ModuleField<'a>) {
        match item {
            // This is pre-expanded above
            ModuleField::Type(_) => {}
            ModuleField::Rec(_) => {}

            ModuleField::Import(i) => {
                self.expand_item_sig(&mut i.item);
            }
            ModuleField::Func(f) => {
                self.expand_type_use(&mut f.ty);
                if let FuncKind::Inline { expression, .. } = &mut f.kind {
                    self.expand_expression(expression);
                }
            }
            ModuleField::Global(g) => {
                if let GlobalKind::Inline(expr) = &mut g.kind {
                    self.expand_expression(expr);
                }
            }
            ModuleField::Data(d) => {
                if let DataKind::Active { offset, .. } = &mut d.kind {
                    self.expand_expression(offset);
                }
            }
            ModuleField::Elem(e) => {
                if let ElemKind::Active { offset, .. } = &mut e.kind {
                    self.expand_expression(offset);
                }
                if let ElemPayload::Exprs { exprs, .. } = &mut e.payload {
                    for expr in exprs {
                        self.expand_expression(expr);
                    }
                }
            }
            ModuleField::Tag(t) => match &mut t.ty {
                TagType::Exception(ty) => {
                    self.expand_type_use(ty);
                }
            },

            ModuleField::Table(_)
            | ModuleField::Memory(_)
            | ModuleField::Start(_)
            | ModuleField::Export(_)
            | ModuleField::Custom(_) => {}
        }
    }

    fn expand_item_sig(&mut self, item: &mut ItemSig<'a>) {
        match &mut item.kind {
            ItemKind::Func(t) | ItemKind::Tag(TagType::Exception(t)) => {
                self.expand_type_use(t);
            }
            ItemKind::Global(_) | ItemKind::Table(_) | ItemKind::Memory(_) => {}
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
            | Instruction::Let(LetType { block: bt, .. })
            | Instruction::Try(bt) => {
                // No expansion necessary, a type reference is already here.
                // We'll verify that it's the same as the inline type, if any,
                // later.
                if bt.ty.index.is_some() {
                    return;
                }

                match &bt.ty.inline {
                    // Only actually expand `TypeUse` with an index which appends a
                    // type if it looks like we need one. This way if the
                    // multi-value proposal isn't enabled and/or used we won't
                    // encode it.
                    Some(inline) => {
                        if inline.params.len() == 0 && inline.results.len() <= 1 {
                            return;
                        }
                    }

                    // If we didn't have either an index or an inline type
                    // listed then assume our block has no inputs/outputs, so
                    // fill in the inline type here.
                    //
                    // Do not fall through to expanding the `TypeUse` because
                    // this doesn't force an empty function type to go into the
                    // type section.
                    None => {
                        bt.ty.inline = Some(FunctionType::default());
                        return;
                    }
                }
                self.expand_type_use(&mut bt.ty);
            }
            Instruction::FuncBind(b) => {
                self.expand_type_use(&mut b.ty);
            }
            Instruction::CallIndirect(c) | Instruction::ReturnCallIndirect(c) => {
                self.expand_type_use(&mut c.ty);
            }
            _ => {}
        }
    }

    fn expand_type_use<T>(&mut self, item: &mut TypeUse<'a, T>) -> Index<'a>
    where
        T: TypeReference<'a>,
    {
        if let Some(idx) = &item.index {
            return *idx;
        }
        let key = match item.inline.as_mut() {
            Some(ty) => {
                ty.expand(self);
                ty.key()
            }
            None => T::default().key(),
        };
        let span = Span::from_offset(0); // FIXME(#613): don't manufacture
        let idx = self.key_to_idx(span, key);
        item.index = Some(idx);
        idx
    }

    fn key_to_idx(&mut self, span: Span, key: impl TypeKey<'a>) -> Index<'a> {
        // First see if this `key` already exists in the type definitions we've
        // seen so far...
        if let Some(idx) = key.lookup(self) {
            return idx;
        }

        // ... and failing that we insert a new type definition.
        let id = gensym::gen(span);
        self.to_prepend.push(ModuleField::Type(Type {
            span,
            id: Some(id),
            name: None,
            def: key.to_def(span),
            parent: None,
        }));
        let idx = Index::Id(id);
        key.insert(self, idx);
        idx
    }
}

pub(crate) trait TypeReference<'a>: Default {
    type Key: TypeKey<'a>;
    fn key(&self) -> Self::Key;
    fn expand(&mut self, cx: &mut Expander<'a>);
}

pub(crate) trait TypeKey<'a> {
    fn lookup(&self, cx: &Expander<'a>) -> Option<Index<'a>>;
    fn to_def(&self, span: Span) -> TypeDef<'a>;
    fn insert(&self, cx: &mut Expander<'a>, id: Index<'a>);
}

pub(crate) type FuncKey<'a> = (Box<[ValType<'a>]>, Box<[ValType<'a>]>);

impl<'a> TypeReference<'a> for FunctionType<'a> {
    type Key = FuncKey<'a>;

    fn key(&self) -> Self::Key {
        let params = self.params.iter().map(|p| p.2).collect();
        let results = self.results.clone();
        (params, results)
    }

    fn expand(&mut self, _cx: &mut Expander<'a>) {}
}

impl<'a> TypeKey<'a> for FuncKey<'a> {
    fn lookup(&self, cx: &Expander<'a>) -> Option<Index<'a>> {
        cx.func_type_to_idx.get(self).cloned()
    }

    fn to_def(&self, _span: Span) -> TypeDef<'a> {
        TypeDef::Func(FunctionType {
            params: self.0.iter().map(|t| (None, None, *t)).collect(),
            results: self.1.clone(),
        })
    }

    fn insert(&self, cx: &mut Expander<'a>, idx: Index<'a>) {
        cx.func_type_to_idx.entry(self.clone()).or_insert(idx);
    }
}
