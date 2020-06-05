use crate::ast_emitter::AstEmitter;
use crate::emitter::EmitError;
use stencil::scope::ScopeIndex;

pub struct BlockEmitter<'a, FuncT, FuncF, StmtT, StmtF>
where
    FuncF: Fn(&mut AstEmitter, &FuncT) -> Result<(), EmitError>,
    StmtF: Fn(&mut AstEmitter, &StmtT) -> Result<(), EmitError>,
{
    pub scope_index: ScopeIndex,
    pub functions: std::slice::Iter<'a, FuncT>,
    pub function: FuncF,
    pub statements: std::slice::Iter<'a, StmtT>,
    pub statement: StmtF,
}

impl<'a, FuncT, FuncF, StmtT, StmtF> BlockEmitter<'a, FuncT, FuncF, StmtT, StmtF>
where
    FuncF: Fn(&mut AstEmitter, &FuncT) -> Result<(), EmitError>,
    StmtF: Fn(&mut AstEmitter, &StmtT) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        emitter.scope_stack.enter_lexical(
            &mut emitter.emit,
            &mut emitter.compilation_info.scope_data_map,
            self.scope_index,
        );

        for fun in self.functions {
            (self.function)(emitter, fun)?;
        }

        for statement in self.statements {
            (self.statement)(emitter, statement)?;
        }

        emitter.scope_stack.leave_lexical(&mut emitter.emit);

        Ok(())
    }
}
