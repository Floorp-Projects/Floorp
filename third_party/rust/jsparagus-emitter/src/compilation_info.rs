use ast::source_atom_set::SourceAtomSet;

pub struct CompilationInfo<'alloc> {
    pub atoms: SourceAtomSet<'alloc>,
    // NOTE: scope information will be added to this struct.
}

impl<'alloc> CompilationInfo<'alloc> {
    pub fn new(atoms: SourceAtomSet<'alloc>) -> Self {
        Self { atoms }
    }
}
