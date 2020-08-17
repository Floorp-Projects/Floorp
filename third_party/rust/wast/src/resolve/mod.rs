use crate::ast::*;
use crate::Error;

mod deinline_import_export;
mod expand;
mod gensym;
mod names;

#[derive(PartialEq, Eq, Hash, Copy, Clone, Debug)]
pub enum Ns {
    Func,
    Table,
    Global,
    Memory,
    Module,
    Instance,
    Event,
    Type,
}

impl Ns {
    fn from_item(item: &ItemSig<'_>) -> Ns {
        match item.kind {
            ItemKind::Func(_) => Ns::Func,
            ItemKind::Table(_) => Ns::Table,
            ItemKind::Memory(_) => Ns::Memory,
            ItemKind::Global(_) => Ns::Global,
            ItemKind::Instance(_) => Ns::Instance,
            ItemKind::Module(_) => Ns::Module,
            ItemKind::Event(_) => Ns::Event,
        }
    }

    fn from_export<'a>(kind: &ExportKind<'a>) -> (Index<'a>, Ns) {
        match *kind {
            ExportKind::Func(f) => (f, Ns::Func),
            ExportKind::Table(f) => (f, Ns::Table),
            ExportKind::Global(f) => (f, Ns::Global),
            ExportKind::Memory(f) => (f, Ns::Memory),
            ExportKind::Instance(f) => (f, Ns::Instance),
            ExportKind::Module(f) => (f, Ns::Module),
            ExportKind::Event(f) => (f, Ns::Event),
            ExportKind::Type(f) => (f, Ns::Type),
        }
    }

    fn from_export_mut<'a, 'b>(kind: &'b mut ExportKind<'a>) -> (&'b mut Index<'a>, Ns) {
        match kind {
            ExportKind::Func(f) => (f, Ns::Func),
            ExportKind::Table(f) => (f, Ns::Table),
            ExportKind::Global(f) => (f, Ns::Global),
            ExportKind::Memory(f) => (f, Ns::Memory),
            ExportKind::Instance(f) => (f, Ns::Instance),
            ExportKind::Module(f) => (f, Ns::Module),
            ExportKind::Event(f) => (f, Ns::Event),
            ExportKind::Type(f) => (f, Ns::Type),
        }
    }

    fn to_export_kind<'a>(&self, index: Index<'a>) -> ExportKind<'a> {
        match self {
            Ns::Func => ExportKind::Func(index),
            Ns::Table => ExportKind::Table(index),
            Ns::Global => ExportKind::Global(index),
            Ns::Memory => ExportKind::Memory(index),
            Ns::Module => ExportKind::Module(index),
            Ns::Instance => ExportKind::Instance(index),
            Ns::Event => ExportKind::Event(index),
            Ns::Type => ExportKind::Type(index),
        }
    }

    fn desc(&self) -> &'static str {
        match self {
            Ns::Func => "func",
            Ns::Table => "table",
            Ns::Global => "global",
            Ns::Memory => "memory",
            Ns::Module => "module",
            Ns::Instance => "instance",
            Ns::Event => "event",
            Ns::Type => "type",
        }
    }
}

pub fn resolve<'a>(module: &mut Module<'a>) -> Result<Names<'a>, Error> {
    let fields = match &mut module.kind {
        ModuleKind::Text(fields) => fields,
        _ => return Ok(Default::default()),
    };

    // Ensure that each resolution of a module is deterministic in the names
    // that it generates by resetting our thread-local symbol generator.
    gensym::reset();

    // First up, de-inline import/export annotations.
    //
    // This ensures we only have to deal with inline definitions and to
    // calculate exports we only have to look for a particular kind of module
    // field.
    deinline_import_export::run(fields);

    // With a canonical form of imports make sure that imports are all listed
    // first.
    for i in 1..fields.len() {
        let span = match &fields[i] {
            ModuleField::Import(i) => i.span,
            _ => continue,
        };
        let name = match &fields[i - 1] {
            ModuleField::Memory(_) => "memory",
            ModuleField::Func(_) => "function",
            ModuleField::Table(_) => "table",
            ModuleField::Global(_) => "global",
            _ => continue,
        };
        return Err(Error::new(span, format!("import after {}", name)));
    }

    // Next inject any `alias` declarations and expand `(export $foo)`
    // annotations. These are all part of the module-linking proposal and we try
    // to resolve their injection here to keep index spaces stable for later.
    expand::run(fields)?;

    // Perform name resolution over all `Index` items to resolve them all to
    // indices instead of symbolic names.
    let resolver = names::resolve(fields)?;
    Ok(Names { resolver })
}

/// Representation of the results of name resolution for a module.
///
/// This structure is returned from the
/// [`Module::resolve`](crate::Module::resolve) function and can be used to
/// resolve your own name arguments if you have any.
#[derive(Default)]
pub struct Names<'a> {
    resolver: names::Resolver<'a>,
}

impl<'a> Names<'a> {
    /// Resolves `idx` within the function namespace.
    ///
    /// If `idx` is a `Num`, it is ignored, but if it's an `Id` then it will be
    /// looked up in the function namespace and converted to a `Num`. If the
    /// `Id` is not defined then an error will be returned.
    pub fn resolve_func(&self, idx: &mut Index<'a>) -> Result<(), Error> {
        self.resolver.module().resolve(idx, Ns::Func)?;
        Ok(())
    }

    /// Resolves `idx` within the memory namespace.
    ///
    /// If `idx` is a `Num`, it is ignored, but if it's an `Id` then it will be
    /// looked up in the memory namespace and converted to a `Num`. If the
    /// `Id` is not defined then an error will be returned.
    pub fn resolve_memory(&self, idx: &mut Index<'a>) -> Result<(), Error> {
        self.resolver.module().resolve(idx, Ns::Memory)?;
        Ok(())
    }

    /// Resolves `idx` within the table namespace.
    ///
    /// If `idx` is a `Num`, it is ignored, but if it's an `Id` then it will be
    /// looked up in the table namespace and converted to a `Num`. If the
    /// `Id` is not defined then an error will be returned.
    pub fn resolve_table(&self, idx: &mut Index<'a>) -> Result<(), Error> {
        self.resolver.module().resolve(idx, Ns::Table)?;
        Ok(())
    }

    /// Resolves `idx` within the global namespace.
    ///
    /// If `idx` is a `Num`, it is ignored, but if it's an `Id` then it will be
    /// looked up in the global namespace and converted to a `Num`. If the
    /// `Id` is not defined then an error will be returned.
    pub fn resolve_global(&self, idx: &mut Index<'a>) -> Result<(), Error> {
        self.resolver.module().resolve(idx, Ns::Global)?;
        Ok(())
    }
}
