use crate::ast_emitter::AstEmitter;
use crate::emitter::EmitError;
use scope::data::ScopeIndex;

pub struct BlockEmitter<'a, StmtT, StmtF>
where
    StmtF: Fn(&mut AstEmitter, &StmtT) -> Result<(), EmitError>,
{
    pub scope_index: ScopeIndex,
    pub statements: std::slice::Iter<'a, StmtT>,
    pub statement: StmtF,
}

impl<'a, StmtT, StmtF> BlockEmitter<'a, StmtT, StmtF>
where
    StmtF: Fn(&mut AstEmitter, &StmtT) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        emitter.scope_stack.enter_lexical(
            &mut emitter.emit,
            &mut emitter.compilation_info.scope_data_map,
            self.scope_index,
        );

        for statement in self.statements {
            (self.statement)(emitter, statement)?;
        }

        emitter.scope_stack.leave_lexical(&mut emitter.emit);

        Ok(())
    }
}
