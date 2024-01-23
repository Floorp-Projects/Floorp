use crate::core::resolve::Ns;
use crate::core::*;
use crate::names::{resolve_error, Namespace};
use crate::token::{Id, Index};
use crate::Error;
use std::collections::HashMap;

pub fn resolve<'a>(fields: &mut Vec<ModuleField<'a>>) -> Result<Resolver<'a>, Error> {
    let mut resolver = Resolver::default();
    resolver.process(fields)?;
    Ok(resolver)
}

/// Context structure used to perform name resolution.
#[derive(Default)]
pub struct Resolver<'a> {
    // Namespaces within each module. Note that each namespace carries with it
    // information about the signature of the item in that namespace. The
    // signature is later used to synthesize the type of a module and inject
    // type annotations if necessary.
    funcs: Namespace<'a>,
    globals: Namespace<'a>,
    tables: Namespace<'a>,
    memories: Namespace<'a>,
    types: Namespace<'a>,
    tags: Namespace<'a>,
    datas: Namespace<'a>,
    elems: Namespace<'a>,
    fields: HashMap<u32, Namespace<'a>>,
    type_info: Vec<TypeInfo<'a>>,
}

impl<'a> Resolver<'a> {
    fn process(&mut self, fields: &mut Vec<ModuleField<'a>>) -> Result<(), Error> {
        // Number everything in the module, recording what names correspond to
        // what indices.
        for field in fields.iter_mut() {
            self.register(field)?;
        }

        // Then we can replace all our `Index::Id` instances with `Index::Num`
        // in the AST. Note that this also recurses into nested modules.
        for field in fields.iter_mut() {
            self.resolve_field(field)?;
        }
        Ok(())
    }

    fn register_type(&mut self, ty: &Type<'a>) -> Result<(), Error> {
        let type_index = self.types.register(ty.id, "type")?;

        match &ty.def {
            // For GC structure types we need to be sure to populate the
            // field namespace here as well.
            //
            // The field namespace is relative to the struct fields are defined in
            TypeDef::Struct(r#struct) => {
                for (i, field) in r#struct.fields.iter().enumerate() {
                    if let Some(id) = field.id {
                        self.fields
                            .entry(type_index)
                            .or_insert(Namespace::default())
                            .register_specific(id, i as u32, "field")?;
                    }
                }
            }

            TypeDef::Array(_) | TypeDef::Func(_) => {}
        }

        // Record function signatures as we see them to so we can
        // generate errors for mismatches in references such as
        // `call_indirect`.
        match &ty.def {
            TypeDef::Func(f) => {
                let params = f.params.iter().map(|p| p.2).collect();
                let results = f.results.clone();
                self.type_info.push(TypeInfo::Func { params, results });
            }
            _ => self.type_info.push(TypeInfo::Other),
        }

        Ok(())
    }

    fn register(&mut self, item: &ModuleField<'a>) -> Result<(), Error> {
        match item {
            ModuleField::Import(i) => match &i.item.kind {
                ItemKind::Func(_) => self.funcs.register(i.item.id, "func")?,
                ItemKind::Memory(_) => self.memories.register(i.item.id, "memory")?,
                ItemKind::Table(_) => self.tables.register(i.item.id, "table")?,
                ItemKind::Global(_) => self.globals.register(i.item.id, "global")?,
                ItemKind::Tag(_) => self.tags.register(i.item.id, "tag")?,
            },
            ModuleField::Global(i) => self.globals.register(i.id, "global")?,
            ModuleField::Memory(i) => self.memories.register(i.id, "memory")?,
            ModuleField::Func(i) => self.funcs.register(i.id, "func")?,
            ModuleField::Table(i) => self.tables.register(i.id, "table")?,

            ModuleField::Type(i) => {
                return self.register_type(i);
            }
            ModuleField::Rec(i) => {
                for ty in &i.types {
                    self.register_type(ty)?;
                }
                return Ok(());
            }
            ModuleField::Elem(e) => self.elems.register(e.id, "elem")?,
            ModuleField::Data(d) => self.datas.register(d.id, "data")?,
            ModuleField::Tag(t) => self.tags.register(t.id, "tag")?,

            // These fields don't define any items in any index space.
            ModuleField::Export(_) | ModuleField::Start(_) | ModuleField::Custom(_) => {
                return Ok(())
            }
        };

        Ok(())
    }

    fn resolve_type(&self, ty: &mut Type<'a>) -> Result<(), Error> {
        match &mut ty.def {
            TypeDef::Func(func) => func.resolve(self)?,
            TypeDef::Struct(struct_) => {
                for field in &mut struct_.fields {
                    self.resolve_storagetype(&mut field.ty)?;
                }
            }
            TypeDef::Array(array) => self.resolve_storagetype(&mut array.ty)?,
        }
        if let Some(parent) = &mut ty.parent {
            self.resolve(parent, Ns::Type)?;
        }
        Ok(())
    }

    fn resolve_field(&self, field: &mut ModuleField<'a>) -> Result<(), Error> {
        match field {
            ModuleField::Import(i) => {
                self.resolve_item_sig(&mut i.item)?;
                Ok(())
            }

            ModuleField::Type(ty) => self.resolve_type(ty),
            ModuleField::Rec(rec) => {
                for ty in &mut rec.types {
                    self.resolve_type(ty)?;
                }
                Ok(())
            }

            ModuleField::Func(f) => {
                let (idx, inline) = self.resolve_type_use(&mut f.ty)?;
                let n = match idx {
                    Index::Num(n, _) => *n,
                    Index::Id(_) => panic!("expected `Num`"),
                };
                if let FuncKind::Inline { locals, expression } = &mut f.kind {
                    // Resolve (ref T) in locals
                    for local in locals.iter_mut() {
                        self.resolve_valtype(&mut local.ty)?;
                    }

                    // Build a scope with a local namespace for the function
                    // body
                    let mut scope = Namespace::default();

                    // Parameters come first in the scope...
                    if let Some(inline) = &inline {
                        for (id, _, _) in inline.params.iter() {
                            scope.register(*id, "local")?;
                        }
                    } else if let Some(TypeInfo::Func { params, .. }) =
                        self.type_info.get(n as usize)
                    {
                        for _ in 0..params.len() {
                            scope.register(None, "local")?;
                        }
                    }

                    // .. followed by locals themselves
                    for local in locals.iter() {
                        scope.register(local.id, "local")?;
                    }

                    // Initialize the expression resolver with this scope
                    let mut resolver = ExprResolver::new(self, scope);

                    // and then we can resolve the expression!
                    resolver.resolve(expression)?;

                    // specifically save the original `sig`, if it was present,
                    // because that's what we're using for local names.
                    f.ty.inline = inline;
                }
                Ok(())
            }

            ModuleField::Elem(e) => {
                match &mut e.kind {
                    ElemKind::Active { table, offset } => {
                        self.resolve(table, Ns::Table)?;
                        self.resolve_expr(offset)?;
                    }
                    ElemKind::Passive { .. } | ElemKind::Declared { .. } => {}
                }
                match &mut e.payload {
                    ElemPayload::Indices(elems) => {
                        for idx in elems {
                            self.resolve(idx, Ns::Func)?;
                        }
                    }
                    ElemPayload::Exprs { exprs, ty } => {
                        for expr in exprs {
                            self.resolve_expr(expr)?;
                        }
                        self.resolve_heaptype(&mut ty.heap)?;
                    }
                }
                Ok(())
            }

            ModuleField::Data(d) => {
                if let DataKind::Active { memory, offset } = &mut d.kind {
                    self.resolve(memory, Ns::Memory)?;
                    self.resolve_expr(offset)?;
                }
                Ok(())
            }

            ModuleField::Start(i) => {
                self.resolve(i, Ns::Func)?;
                Ok(())
            }

            ModuleField::Export(e) => {
                self.resolve(
                    &mut e.item,
                    match e.kind {
                        ExportKind::Func => Ns::Func,
                        ExportKind::Table => Ns::Table,
                        ExportKind::Memory => Ns::Memory,
                        ExportKind::Global => Ns::Global,
                        ExportKind::Tag => Ns::Tag,
                    },
                )?;
                Ok(())
            }

            ModuleField::Global(g) => {
                self.resolve_valtype(&mut g.ty.ty)?;
                if let GlobalKind::Inline(expr) = &mut g.kind {
                    self.resolve_expr(expr)?;
                }
                Ok(())
            }

            ModuleField::Tag(t) => {
                match &mut t.ty {
                    TagType::Exception(ty) => {
                        self.resolve_type_use(ty)?;
                    }
                }
                Ok(())
            }

            ModuleField::Table(t) => {
                if let TableKind::Normal { ty, init_expr } = &mut t.kind {
                    self.resolve_heaptype(&mut ty.elem.heap)?;
                    if let Some(init_expr) = init_expr {
                        self.resolve_expr(init_expr)?;
                    }
                }
                Ok(())
            }

            ModuleField::Memory(_) | ModuleField::Custom(_) => Ok(()),
        }
    }

    fn resolve_valtype(&self, ty: &mut ValType<'a>) -> Result<(), Error> {
        match ty {
            ValType::Ref(ty) => self.resolve_heaptype(&mut ty.heap)?,
            _ => {}
        }
        Ok(())
    }

    fn resolve_reftype(&self, ty: &mut RefType<'a>) -> Result<(), Error> {
        self.resolve_heaptype(&mut ty.heap)
    }

    fn resolve_heaptype(&self, ty: &mut HeapType<'a>) -> Result<(), Error> {
        match ty {
            HeapType::Concrete(i) => {
                self.resolve(i, Ns::Type)?;
            }
            _ => {}
        }
        Ok(())
    }

    fn resolve_storagetype(&self, ty: &mut StorageType<'a>) -> Result<(), Error> {
        match ty {
            StorageType::Val(ty) => self.resolve_valtype(ty)?,
            _ => {}
        }
        Ok(())
    }

    fn resolve_item_sig(&self, item: &mut ItemSig<'a>) -> Result<(), Error> {
        match &mut item.kind {
            ItemKind::Func(t) | ItemKind::Tag(TagType::Exception(t)) => {
                self.resolve_type_use(t)?;
            }
            ItemKind::Global(t) => self.resolve_valtype(&mut t.ty)?,
            ItemKind::Table(t) => {
                self.resolve_heaptype(&mut t.elem.heap)?;
            }
            ItemKind::Memory(_) => {}
        }
        Ok(())
    }

    fn resolve_type_use<'b, T>(
        &self,
        ty: &'b mut TypeUse<'a, T>,
    ) -> Result<(&'b Index<'a>, Option<T>), Error>
    where
        T: TypeReference<'a>,
    {
        let idx = ty.index.as_mut().unwrap();
        self.resolve(idx, Ns::Type)?;

        // If the type was listed inline *and* it was specified via a type index
        // we need to assert they're the same.
        //
        // Note that we resolve the type first to transform all names to
        // indices to ensure that all the indices line up.
        if let Some(inline) = &mut ty.inline {
            inline.resolve(self)?;
            inline.check_matches(idx, self)?;
        }

        Ok((idx, ty.inline.take()))
    }

    fn resolve_expr(&self, expr: &mut Expression<'a>) -> Result<(), Error> {
        ExprResolver::new(self, Namespace::default()).resolve(expr)
    }

    pub fn resolve(&self, idx: &mut Index<'a>, ns: Ns) -> Result<u32, Error> {
        match ns {
            Ns::Func => self.funcs.resolve(idx, "func"),
            Ns::Table => self.tables.resolve(idx, "table"),
            Ns::Global => self.globals.resolve(idx, "global"),
            Ns::Memory => self.memories.resolve(idx, "memory"),
            Ns::Tag => self.tags.resolve(idx, "tag"),
            Ns::Type => self.types.resolve(idx, "type"),
        }
    }
}

#[derive(Debug, Clone)]
struct ExprBlock<'a> {
    // The label of the block
    label: Option<Id<'a>>,
    // Whether this block pushed a new scope for resolving locals
    pushed_scope: bool,
}

struct ExprResolver<'a, 'b> {
    resolver: &'b Resolver<'a>,
    // Scopes tracks the local namespace and dynamically grows as we enter/exit
    // `let` blocks
    scopes: Vec<Namespace<'a>>,
    blocks: Vec<ExprBlock<'a>>,
}

impl<'a, 'b> ExprResolver<'a, 'b> {
    fn new(resolver: &'b Resolver<'a>, initial_scope: Namespace<'a>) -> ExprResolver<'a, 'b> {
        ExprResolver {
            resolver,
            scopes: vec![initial_scope],
            blocks: Vec::new(),
        }
    }

    fn resolve(&mut self, expr: &mut Expression<'a>) -> Result<(), Error> {
        for instr in expr.instrs.iter_mut() {
            self.resolve_instr(instr)?;
        }
        Ok(())
    }

    fn resolve_block_type(&mut self, bt: &mut BlockType<'a>) -> Result<(), Error> {
        // If the index is specified on this block type then that's the source
        // of resolution and the resolver step here will verify the inline type
        // matches. Note that indexes may come from the source text itself but
        // may also come from being injected as part of the type expansion phase
        // of resolution.
        //
        // If no type is present then that means that the inline type is not
        // present or has 0-1 results. In that case the nested value types are
        // resolved, if they're there, to get encoded later on.
        if bt.ty.index.is_some() {
            self.resolver.resolve_type_use(&mut bt.ty)?;
        } else if let Some(inline) = &mut bt.ty.inline {
            inline.resolve(self.resolver)?;
        }

        Ok(())
    }

    fn resolve_instr(&mut self, instr: &mut Instruction<'a>) -> Result<(), Error> {
        use Instruction::*;

        if let Some(m) = instr.memarg_mut() {
            self.resolver.resolve(&mut m.memory, Ns::Memory)?;
        }

        match instr {
            MemorySize(i) | MemoryGrow(i) | MemoryFill(i) | MemoryDiscard(i) => {
                self.resolver.resolve(&mut i.mem, Ns::Memory)?;
            }
            MemoryInit(i) => {
                self.resolver.datas.resolve(&mut i.data, "data")?;
                self.resolver.resolve(&mut i.mem, Ns::Memory)?;
            }
            MemoryCopy(i) => {
                self.resolver.resolve(&mut i.src, Ns::Memory)?;
                self.resolver.resolve(&mut i.dst, Ns::Memory)?;
            }
            DataDrop(i) => {
                self.resolver.datas.resolve(i, "data")?;
            }

            TableInit(i) => {
                self.resolver.elems.resolve(&mut i.elem, "elem")?;
                self.resolver.resolve(&mut i.table, Ns::Table)?;
            }
            ElemDrop(i) => {
                self.resolver.elems.resolve(i, "elem")?;
            }

            TableCopy(i) => {
                self.resolver.resolve(&mut i.dst, Ns::Table)?;
                self.resolver.resolve(&mut i.src, Ns::Table)?;
            }

            TableFill(i) | TableSet(i) | TableGet(i) | TableSize(i) | TableGrow(i) => {
                self.resolver.resolve(&mut i.dst, Ns::Table)?;
            }

            GlobalSet(i) | GlobalGet(i) => {
                self.resolver.resolve(i, Ns::Global)?;
            }

            LocalSet(i) | LocalGet(i) | LocalTee(i) => {
                assert!(self.scopes.len() > 0);
                // Resolve a local by iterating over scopes from most recent
                // to less recent. This allows locals added by `let` blocks to
                // shadow less recent locals.
                for (depth, scope) in self.scopes.iter().enumerate().rev() {
                    if let Err(e) = scope.resolve(i, "local") {
                        if depth == 0 {
                            // There are no more scopes left, report this as
                            // the result
                            return Err(e);
                        }
                    } else {
                        break;
                    }
                }
                // We must have taken the `break` and resolved the local
                assert!(i.is_resolved());
            }

            Call(i) | RefFunc(i) | ReturnCall(i) => {
                self.resolver.resolve(i, Ns::Func)?;
            }

            CallIndirect(c) | ReturnCallIndirect(c) => {
                self.resolver.resolve(&mut c.table, Ns::Table)?;
                self.resolver.resolve_type_use(&mut c.ty)?;
            }

            CallRef(i) | ReturnCallRef(i) => {
                self.resolver.resolve(i, Ns::Type)?;
            }

            FuncBind(b) => {
                self.resolver.resolve_type_use(&mut b.ty)?;
            }

            Let(t) => {
                // Resolve (ref T) in locals
                for local in t.locals.iter_mut() {
                    self.resolver.resolve_valtype(&mut local.ty)?;
                }

                // Register all locals defined in this let
                let mut scope = Namespace::default();
                for local in t.locals.iter() {
                    scope.register(local.id, "local")?;
                }
                self.scopes.push(scope);
                self.blocks.push(ExprBlock {
                    label: t.block.label,
                    pushed_scope: true,
                });

                self.resolve_block_type(&mut t.block)?;
            }

            Block(bt) | If(bt) | Loop(bt) | Try(bt) => {
                self.blocks.push(ExprBlock {
                    label: bt.label,
                    pushed_scope: false,
                });
                self.resolve_block_type(bt)?;
            }
            TryTable(try_table) => {
                self.blocks.push(ExprBlock {
                    label: try_table.block.label,
                    pushed_scope: false,
                });
                self.resolve_block_type(&mut try_table.block)?;
                for catch in &mut try_table.catches {
                    if let Some(tag) = catch.kind.tag_index_mut() {
                        self.resolver.resolve(tag, Ns::Tag)?;
                    }
                    self.resolve_label(&mut catch.label)?;
                }
            }

            // On `End` instructions we pop a label from the stack, and for both
            // `End` and `Else` instructions if they have labels listed we
            // verify that they match the label at the beginning of the block.
            Else(_) | End(_) => {
                let (matching_block, label) = match &instr {
                    Else(label) => (self.blocks.last().cloned(), label),
                    End(label) => (self.blocks.pop(), label),
                    _ => unreachable!(),
                };
                let matching_block = match matching_block {
                    Some(l) => l,
                    None => return Ok(()),
                };

                // Reset the local scopes to before this block was entered
                if matching_block.pushed_scope {
                    if let End(_) = instr {
                        self.scopes.pop();
                    }
                }

                let label = match label {
                    Some(l) => l,
                    None => return Ok(()),
                };
                if Some(*label) == matching_block.label {
                    return Ok(());
                }
                return Err(Error::new(
                    label.span(),
                    "mismatching labels between end and block".to_string(),
                ));
            }

            Br(i) | BrIf(i) | BrOnNull(i) | BrOnNonNull(i) => {
                self.resolve_label(i)?;
            }

            BrTable(i) => {
                for label in i.labels.iter_mut() {
                    self.resolve_label(label)?;
                }
                self.resolve_label(&mut i.default)?;
            }

            Throw(i) => {
                self.resolver.resolve(i, Ns::Tag)?;
            }
            Rethrow(i) => {
                self.resolve_label(i)?;
            }
            Catch(i) => {
                self.resolver.resolve(i, Ns::Tag)?;
            }
            Delegate(i) => {
                // Since a delegate starts counting one layer out from the
                // current try-delegate block, we pop before we resolve labels.
                self.blocks.pop();
                self.resolve_label(i)?;
            }

            Select(s) => {
                if let Some(list) = &mut s.tys {
                    for ty in list {
                        self.resolver.resolve_valtype(ty)?;
                    }
                }
            }

            RefTest(i) => {
                self.resolver.resolve_reftype(&mut i.r#type)?;
            }
            RefCast(i) => {
                self.resolver.resolve_reftype(&mut i.r#type)?;
            }
            BrOnCast(i) => {
                self.resolve_label(&mut i.label)?;
                self.resolver.resolve_reftype(&mut i.to_type)?;
                self.resolver.resolve_reftype(&mut i.from_type)?;
            }
            BrOnCastFail(i) => {
                self.resolve_label(&mut i.label)?;
                self.resolver.resolve_reftype(&mut i.to_type)?;
                self.resolver.resolve_reftype(&mut i.from_type)?;
            }

            StructNew(i) | StructNewDefault(i) | ArrayNew(i) | ArrayNewDefault(i) | ArrayGet(i)
            | ArrayGetS(i) | ArrayGetU(i) | ArraySet(i) => {
                self.resolver.resolve(i, Ns::Type)?;
            }

            StructSet(s) | StructGet(s) | StructGetS(s) | StructGetU(s) => {
                let type_index = self.resolver.resolve(&mut s.r#struct, Ns::Type)?;
                if let Index::Id(field_id) = s.field {
                    self.resolver
                        .fields
                        .get(&type_index)
                        .ok_or(Error::new(field_id.span(), format!("accessing a named field `{}` in a struct without named fields, type index {}", field_id.name(), type_index)))?
                        .resolve(&mut s.field, "field")?;
                }
            }

            ArrayNewFixed(a) => {
                self.resolver.resolve(&mut a.array, Ns::Type)?;
            }
            ArrayNewData(a) => {
                self.resolver.resolve(&mut a.array, Ns::Type)?;
                self.resolver.datas.resolve(&mut a.data_idx, "data")?;
            }
            ArrayNewElem(a) => {
                self.resolver.resolve(&mut a.array, Ns::Type)?;
                self.resolver.elems.resolve(&mut a.elem_idx, "elem")?;
            }
            ArrayFill(a) => {
                self.resolver.resolve(&mut a.array, Ns::Type)?;
            }
            ArrayCopy(a) => {
                self.resolver.resolve(&mut a.dest_array, Ns::Type)?;
                self.resolver.resolve(&mut a.src_array, Ns::Type)?;
            }
            ArrayInitData(a) => {
                self.resolver.resolve(&mut a.array, Ns::Type)?;
                self.resolver.datas.resolve(&mut a.segment, "data")?;
            }
            ArrayInitElem(a) => {
                self.resolver.resolve(&mut a.array, Ns::Type)?;
                self.resolver.elems.resolve(&mut a.segment, "elem")?;
            }

            RefNull(ty) => self.resolver.resolve_heaptype(ty)?,

            _ => {}
        }
        Ok(())
    }

    fn resolve_label(&self, label: &mut Index<'a>) -> Result<(), Error> {
        let id = match label {
            Index::Num(..) => return Ok(()),
            Index::Id(id) => *id,
        };
        let idx = self
            .blocks
            .iter()
            .rev()
            .enumerate()
            .filter_map(|(i, b)| b.label.map(|l| (i, l)))
            .find(|(_, l)| *l == id);
        match idx {
            Some((idx, _)) => {
                *label = Index::Num(idx as u32, id.span());
                Ok(())
            }
            None => Err(resolve_error(id, "label")),
        }
    }
}

enum TypeInfo<'a> {
    Func {
        params: Box<[ValType<'a>]>,
        results: Box<[ValType<'a>]>,
    },
    Other,
}

trait TypeReference<'a> {
    fn check_matches(&mut self, idx: &Index<'a>, cx: &Resolver<'a>) -> Result<(), Error>;
    fn resolve(&mut self, cx: &Resolver<'a>) -> Result<(), Error>;
}

impl<'a> TypeReference<'a> for FunctionType<'a> {
    fn check_matches(&mut self, idx: &Index<'a>, cx: &Resolver<'a>) -> Result<(), Error> {
        let n = match idx {
            Index::Num(n, _) => *n,
            Index::Id(_) => panic!("expected `Num`"),
        };
        let (params, results) = match cx.type_info.get(n as usize) {
            Some(TypeInfo::Func { params, results }) => (params, results),
            _ => return Ok(()),
        };

        // Here we need to check that the inline type listed (ourselves) matches
        // what was listed in the module itself (the `params` and `results`
        // above). The listed values in `types` are not resolved yet, although
        // we should be resolved. In any case we do name resolution
        // opportunistically here to see if the values are equal.

        let types_not_equal = |a: &ValType, b: &ValType| {
            let mut a = a.clone();
            let mut b = b.clone();
            drop(cx.resolve_valtype(&mut a));
            drop(cx.resolve_valtype(&mut b));
            a != b
        };

        let not_equal = params.len() != self.params.len()
            || results.len() != self.results.len()
            || params
                .iter()
                .zip(self.params.iter())
                .any(|(a, (_, _, b))| types_not_equal(a, b))
            || results
                .iter()
                .zip(self.results.iter())
                .any(|(a, b)| types_not_equal(a, b));
        if not_equal {
            return Err(Error::new(
                idx.span(),
                format!("inline function type doesn't match type reference"),
            ));
        }

        Ok(())
    }

    fn resolve(&mut self, cx: &Resolver<'a>) -> Result<(), Error> {
        // Resolve the (ref T) value types in the final function type
        for param in self.params.iter_mut() {
            cx.resolve_valtype(&mut param.2)?;
        }
        for result in self.results.iter_mut() {
            cx.resolve_valtype(result)?;
        }
        Ok(())
    }
}
