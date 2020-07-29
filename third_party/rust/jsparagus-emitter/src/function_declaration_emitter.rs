use crate::ast_emitter::AstEmitter;
use crate::emitter::EmitError;
use crate::reference_op_emitter::{
    AssignmentEmitter, DeclarationEmitter, GetNameEmitter, NameReferenceEmitter,
};
use ast::source_atom_set::SourceAtomSetIndex;
use stencil::gcthings::GCThingIndex;
use stencil::script::ScriptStencilIndex;

pub struct LazyFunctionEmitter {
    pub stencil_index: ScriptStencilIndex,
}

impl LazyFunctionEmitter {
    pub fn emit(self, emitter: &mut AstEmitter) -> GCThingIndex {
        emitter
            .compilation_info
            .functions
            .get_mut(self.stencil_index)
            .set_function_emitted();
        emitter.emit.get_function_gcthing_index(self.stencil_index)
    }
}

pub struct TopLevelFunctionDeclarationEmitter {
    pub fun_index: GCThingIndex,
}

impl TopLevelFunctionDeclarationEmitter {
    pub fn emit(self, emitter: &mut AstEmitter) {
        emitter.emit.lambda(self.fun_index);
        emitter.emit.def_fun();
    }
}

pub struct LexicalFunctionDeclarationEmitter {
    pub name: SourceAtomSetIndex,
    pub fun_index: GCThingIndex,
}

impl LexicalFunctionDeclarationEmitter {
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        DeclarationEmitter {
            lhs: |emitter| {
                Ok(NameReferenceEmitter { name: self.name }.emit_for_declaration(emitter))
            },
            rhs: |emitter| {
                emitter.emit.lambda(self.fun_index);
                Ok(())
            },
        }
        .emit(emitter)?;

        emitter.emit.pop();

        Ok(())
    }
}

pub struct AnnexBFunctionDeclarationEmitter {
    pub name: SourceAtomSetIndex,
    pub fun_index: GCThingIndex,
}

impl AnnexBFunctionDeclarationEmitter {
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        LexicalFunctionDeclarationEmitter {
            name: self.name,
            fun_index: self.fun_index,
        }
        .emit(emitter)?;

        AssignmentEmitter {
            lhs: |emitter| {
                Ok(NameReferenceEmitter { name: self.name }.emit_for_var_assignment(emitter))
            },
            rhs: |emitter| {
                GetNameEmitter { name: self.name }.emit(emitter);
                Ok(())
            },
        }
        .emit(emitter)?;

        emitter.emit.pop();

        Ok(())
    }
}
