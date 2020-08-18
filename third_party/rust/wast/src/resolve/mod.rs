use crate::ast::*;
use crate::Error;

mod expand;
mod names;
mod tyexpand;

pub fn resolve<'a>(module: &mut Module<'a>) -> Result<Names<'a>, Error> {
    let fields = match &mut module.kind {
        ModuleKind::Text(fields) => fields,
        _ => return Ok(Default::default()),
    };

    // First up, let's de-inline import/export annotations since this'll
    // restructure the module and affect how we count indices in future passes
    // since function definitions turn into imports.
    //
    // The first pass switches all inline imports to explicit `Import` items.
    // This pass also counts all `Import` items per-type to start building up
    // the index space so we know the corresponding index for each item.
    //
    // In a second pass we then remove all inline `export` annotations, again
    // counting indices as we go along to ensure we always know the index for
    // what we're exporting.
    //
    // The final step is then taking all of the injected `export` fields and
    // actually pushing them onto our list of fields.
    let mut expander = expand::Expander::default();
    expander.process(fields, expand::Expander::deinline_import);
    expander.process(fields, expand::Expander::deinline_export);

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

    // For the second pass we resolve all inline type annotations. This will, in
    // the order that we see them, append to the list of types. Note that types
    // are indexed so we're careful to always insert new types just before the
    // field that we're looking at.
    //
    // It's not strictly required that we `move_types_first` here but it gets
    // our indexing to exactly match wabt's which our test suite is asserting.
    let mut cur = 0;
    let mut expander = tyexpand::Expander::default();
    move_types_first(fields);
    while cur < fields.len() {
        expander.expand(&mut fields[cur]);
        for new in expander.to_prepend.drain(..) {
            fields.insert(cur, new);
            cur += 1;
        }
        cur += 1;
    }

    // Perform name resolution over all `Index` items to resolve them all to
    // indices instead of symbolic names.
    //
    // For this operation we do need to make sure that imports are sorted first
    // because otherwise we'll be calculating indices in the wrong order.
    move_imports_first(fields);
    let mut resolver = names::Resolver::default();
    for field in fields.iter_mut() {
        resolver.register(field)?;
    }
    for field in fields.iter_mut() {
        resolver.resolve(field)?;
    }
    Ok(Names { resolver })
}

fn move_imports_first(fields: &mut [ModuleField<'_>]) {
    fields.sort_by_key(|f| match f {
        ModuleField::Import(_) => false,
        _ => true,
    });
}

fn move_types_first(fields: &mut [ModuleField<'_>]) {
    fields.sort_by_key(|f| match f {
        ModuleField::Type(_) => false,
        _ => true,
    });
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
        self.resolver.resolve_idx(idx, names::Ns::Func)
    }

    /// Resolves `idx` within the memory namespace.
    ///
    /// If `idx` is a `Num`, it is ignored, but if it's an `Id` then it will be
    /// looked up in the memory namespace and converted to a `Num`. If the
    /// `Id` is not defined then an error will be returned.
    pub fn resolve_memory(&self, idx: &mut Index<'a>) -> Result<(), Error> {
        self.resolver.resolve_idx(idx, names::Ns::Memory)
    }

    /// Resolves `idx` within the table namespace.
    ///
    /// If `idx` is a `Num`, it is ignored, but if it's an `Id` then it will be
    /// looked up in the table namespace and converted to a `Num`. If the
    /// `Id` is not defined then an error will be returned.
    pub fn resolve_table(&self, idx: &mut Index<'a>) -> Result<(), Error> {
        self.resolver.resolve_idx(idx, names::Ns::Table)
    }

    /// Resolves `idx` within the global namespace.
    ///
    /// If `idx` is a `Num`, it is ignored, but if it's an `Id` then it will be
    /// looked up in the global namespace and converted to a `Num`. If the
    /// `Id` is not defined then an error will be returned.
    pub fn resolve_global(&self, idx: &mut Index<'a>) -> Result<(), Error> {
        self.resolver.resolve_idx(idx, names::Ns::Global)
    }
}
