use crate::ast_emitter::AstEmitter;
use crate::emitter::EmitError;

pub struct ScriptEmitter<'a, StmtT, StmtF>
where
    StmtF: Fn(&mut AstEmitter, &StmtT) -> Result<(), EmitError>,
{
    pub statements: std::slice::Iter<'a, StmtT>,
    pub statement: StmtF,
}

impl<'a, StmtT, StmtF> ScriptEmitter<'a, StmtT, StmtF>
where
    StmtF: Fn(&mut AstEmitter, &StmtT) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        emitter
            .scope_stack
            .enter_global(&mut emitter.emit, &emitter.compilation_info.scope_data_map);

        for statement in self.statements {
            (self.statement)(emitter, statement)?;
        }

        emitter.emit.ret_rval();

        emitter.scope_stack.leave_global(&mut emitter.emit);

        Ok(())
    }
}
