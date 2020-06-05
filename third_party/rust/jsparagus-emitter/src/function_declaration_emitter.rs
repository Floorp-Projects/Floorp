use crate::ast_emitter::AstEmitter;
use stencil::function::FunctionStencilIndex;
use stencil::gcthings::GCThingIndex;

pub struct LazyFunctionEmitter {
    pub stencil_index: FunctionStencilIndex,
}

impl LazyFunctionEmitter {
    pub fn emit(self, emitter: &mut AstEmitter) -> GCThingIndex {
        emitter.emit.get_function_gcthing_index(self.stencil_index)
    }
}

pub struct FunctionDeclarationEmitter {
    pub fun: GCThingIndex,
}

impl FunctionDeclarationEmitter {
    pub fn emit(self, emitter: &mut AstEmitter) {
        emitter.emit.lambda(self.fun);
        emitter.emit.def_fun();
    }
}
