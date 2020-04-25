use crate::ast_emitter::AstEmitter;
use crate::emitter::EmitError;

pub struct ScriptEmitter<'a, FuncT, FuncF, StmtT, StmtF>
where
    FuncF: Fn(&mut AstEmitter, &FuncT) -> Result<(), EmitError>,
    StmtF: Fn(&mut AstEmitter, &StmtT) -> Result<(), EmitError>,
{
    pub top_level_functions: std::slice::Iter<'a, FuncT>,
    pub top_level_function: FuncF,
    pub statements: std::slice::Iter<'a, StmtT>,
    pub statement: StmtF,
}

impl<'a, FuncT, FuncF, StmtT, StmtF> ScriptEmitter<'a, FuncT, FuncF, StmtT, StmtF>
where
    FuncF: Fn(&mut AstEmitter, &FuncT) -> Result<(), EmitError>,
    StmtF: Fn(&mut AstEmitter, &StmtT) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        let scope_data_map = &emitter.compilation_info.scope_data_map;

        emitter
            .scope_stack
            .enter_global(&mut emitter.emit, scope_data_map);

        for fun in self.top_level_functions {
            (self.top_level_function)(emitter, fun)?;
        }

        for statement in self.statements {
            (self.statement)(emitter, statement)?;
        }

        emitter.emit.ret_rval();

        emitter.scope_stack.leave_global(&mut emitter.emit);

        Ok(())
    }
}
