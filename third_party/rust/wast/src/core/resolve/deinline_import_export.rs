use crate::core::*;
use crate::gensym;
use crate::token::{Id, Index, Span};
use std::mem;

pub fn run(fields: &mut Vec<ModuleField>) {
    for mut item in mem::take(fields) {
        match &mut item {
            ModuleField::Func(f) => {
                for name in f.exports.names.drain(..) {
                    fields.push(export(f.span, name, ExportKind::Func, &mut f.id));
                }
                match f.kind {
                    FuncKind::Import(import) => {
                        item = ModuleField::Import(Import {
                            span: f.span,
                            module: import.module,
                            field: import.field,
                            item: ItemSig {
                                span: f.span,
                                id: f.id,
                                name: f.name,
                                kind: ItemKind::Func(f.ty.clone()),
                            },
                        });
                    }
                    FuncKind::Inline { .. } => {}
                }
            }

            ModuleField::Memory(m) => {
                for name in m.exports.names.drain(..) {
                    fields.push(export(m.span, name, ExportKind::Memory, &mut m.id));
                }
                match m.kind {
                    MemoryKind::Import { import, ty } => {
                        item = ModuleField::Import(Import {
                            span: m.span,
                            module: import.module,
                            field: import.field,
                            item: ItemSig {
                                span: m.span,
                                id: m.id,
                                name: None,
                                kind: ItemKind::Memory(ty),
                            },
                        });
                    }
                    // If data is defined inline insert an explicit `data` module
                    // field here instead, switching this to a `Normal` memory.
                    MemoryKind::Inline { is_32, ref data } => {
                        let len = data.iter().map(|l| l.len()).sum::<usize>() as u32;
                        let pages = (len + default_page_size() - 1) / default_page_size();
                        let kind = MemoryKind::Normal(if is_32 {
                            MemoryType::B32 {
                                limits: Limits {
                                    min: pages,
                                    max: Some(pages),
                                },
                                shared: false,
                                page_size_log2: None,
                            }
                        } else {
                            MemoryType::B64 {
                                limits: Limits64 {
                                    min: u64::from(pages),
                                    max: Some(u64::from(pages)),
                                },
                                shared: false,
                                page_size_log2: None,
                            }
                        });
                        let data = match mem::replace(&mut m.kind, kind) {
                            MemoryKind::Inline { data, .. } => data,
                            _ => unreachable!(),
                        };
                        let id = gensym::fill(m.span, &mut m.id);
                        fields.push(ModuleField::Data(Data {
                            span: m.span,
                            id: None,
                            name: None,
                            kind: DataKind::Active {
                                memory: Index::Id(id),
                                offset: Expression {
                                    instrs: Box::new([if is_32 {
                                        Instruction::I32Const(0)
                                    } else {
                                        Instruction::I64Const(0)
                                    }]),
                                    branch_hints: Vec::new(),
                                },
                            },
                            data,
                        }));
                    }

                    MemoryKind::Normal(_) => {}
                }
            }

            ModuleField::Table(t) => {
                for name in t.exports.names.drain(..) {
                    fields.push(export(t.span, name, ExportKind::Table, &mut t.id));
                }
                match &mut t.kind {
                    TableKind::Import { import, ty } => {
                        item = ModuleField::Import(Import {
                            span: t.span,
                            module: import.module,
                            field: import.field,
                            item: ItemSig {
                                span: t.span,
                                id: t.id,
                                name: None,
                                kind: ItemKind::Table(*ty),
                            },
                        });
                    }
                    // If data is defined inline insert an explicit `data` module
                    // field here instead, switching this to a `Normal` memory.
                    TableKind::Inline { payload, elem } => {
                        let len = match payload {
                            ElemPayload::Indices(v) => v.len(),
                            ElemPayload::Exprs { exprs, .. } => exprs.len(),
                        };
                        let kind = TableKind::Normal {
                            ty: TableType {
                                limits: Limits {
                                    min: len as u32,
                                    max: Some(len as u32),
                                },
                                elem: *elem,
                            },
                            init_expr: None,
                        };
                        let payload = match mem::replace(&mut t.kind, kind) {
                            TableKind::Inline { payload, .. } => payload,
                            _ => unreachable!(),
                        };
                        let id = gensym::fill(t.span, &mut t.id);
                        fields.push(ModuleField::Elem(Elem {
                            span: t.span,
                            id: None,
                            name: None,
                            kind: ElemKind::Active {
                                table: Index::Id(id),
                                offset: Expression {
                                    instrs: Box::new([Instruction::I32Const(0)]),
                                    branch_hints: Vec::new(),
                                },
                            },
                            payload,
                        }));
                    }

                    TableKind::Normal { .. } => {}
                }
            }

            ModuleField::Global(g) => {
                for name in g.exports.names.drain(..) {
                    fields.push(export(g.span, name, ExportKind::Global, &mut g.id));
                }
                match g.kind {
                    GlobalKind::Import(import) => {
                        item = ModuleField::Import(Import {
                            span: g.span,
                            module: import.module,
                            field: import.field,
                            item: ItemSig {
                                span: g.span,
                                id: g.id,
                                name: None,
                                kind: ItemKind::Global(g.ty),
                            },
                        });
                    }
                    GlobalKind::Inline { .. } => {}
                }
            }

            ModuleField::Tag(e) => {
                for name in e.exports.names.drain(..) {
                    fields.push(export(e.span, name, ExportKind::Tag, &mut e.id));
                }
                match e.kind {
                    TagKind::Import(import) => {
                        item = ModuleField::Import(Import {
                            span: e.span,
                            module: import.module,
                            field: import.field,
                            item: ItemSig {
                                span: e.span,
                                id: e.id,
                                name: None,
                                kind: ItemKind::Tag(e.ty.clone()),
                            },
                        });
                    }
                    TagKind::Inline { .. } => {}
                }
            }

            ModuleField::Import(_)
            | ModuleField::Type(_)
            | ModuleField::Rec(_)
            | ModuleField::Export(_)
            | ModuleField::Start(_)
            | ModuleField::Elem(_)
            | ModuleField::Data(_)
            | ModuleField::Custom(_) => {}
        }

        fields.push(item);
    }

    fn default_page_size() -> u32 {
        1 << 16
    }
}

fn export<'a>(
    span: Span,
    name: &'a str,
    kind: ExportKind,
    id: &mut Option<Id<'a>>,
) -> ModuleField<'a> {
    let id = gensym::fill(span, id);
    ModuleField::Export(Export {
        span,
        name,
        kind,
        item: Index::Id(id),
    })
}
