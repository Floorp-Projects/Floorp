use crate::ast_emitter::AstEmitter;
use crate::emitter::EmitError;
use crate::reference_op_emitter::{AssignmentEmitter, DeclarationEmitter, NameReferenceEmitter};
use ast::source_atom_set::SourceAtomSetIndex;
use stencil::gcthings::GCThingIndex;
use stencil::script::ScriptStencilIndex;

pub struct LazyFunctionEmitter {
    pub stencil_index: ScriptStencilIndex,
}

impl LazyFunctionEmitter {
    pub fn emit(self, emitter: &mut AstEmitter) -> GCThingIndex {
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
        .emit(emitter)
    }
}

pub struct AnnexBFunctionDeclarationEmitter {
    pub name: SourceAtomSetIndex,
    pub fun_index: GCThingIndex,
}

impl AnnexBFunctionDeclarationEmitter {
    pub fn emit(self, emitter: &mut AstEmitter) -> Result<(), EmitError> {
        AssignmentEmitter {
            lhs: |emitter| {
                Ok(NameReferenceEmitter { name: self.name }.emit_for_assignment(emitter))
            },
            rhs: |emitter| {
                emitter.emit.lambda(self.fun_index);
                Ok(())
            },
        }
        .emit(emitter)
    }
}
