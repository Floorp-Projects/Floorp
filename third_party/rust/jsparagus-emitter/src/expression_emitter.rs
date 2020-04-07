use crate::ast_emitter::AstEmitter;
use crate::emitter::EmitError;

pub struct ExpressionEmitter<F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub expr: F,
}

impl<F> ExpressionEmitter<F>
where
    F: Fn(&mut AstEmitter) -> Result<(), EmitError>,
{
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        //              [stack]

        (self.expr)(emitter)?;
        //              [stack] VAL

        if emitter.options.no_script_rval {
            emitter.emit.pop();
        //          [stack]
        } else {
            emitter.emit.set_rval();
            //          [stack]
        }

        Ok(())
    }
}
