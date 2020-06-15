mod array_emitter;
mod ast_emitter;
mod block_emitter;
mod compilation_info;
mod control_structures;
mod dis;
mod emitter;
mod emitter_scope;
mod expression_emitter;
mod function_declaration_emitter;
mod object_emitter;
mod reference_op_emitter;
mod script_emitter;

extern crate jsparagus_ast as ast;
extern crate jsparagus_scope as scope;
extern crate jsparagus_stencil as stencil;

pub use crate::emitter::{EmitError, EmitOptions};
pub use dis::dis;

use crate::compilation_info::CompilationInfo;

use ast::source_atom_set::SourceAtomSet;
use ast::source_slice_list::SourceSliceList;
use scope::ScopePassResult;
use stencil::result::EmitResult;

pub fn emit<'alloc>(
    ast: &'alloc ast::types::Program<'alloc>,
    options: &EmitOptions,
    atoms: SourceAtomSet<'alloc>,
    slices: SourceSliceList<'alloc>,
) -> Result<EmitResult<'alloc>, EmitError> {
    let ScopePassResult {
        scope_data_map,
        function_declarations,
        function_stencil_indices,
        function_declaration_properties,
        functions,
    } = scope::generate_scope_data(ast);
    let compilation_info = CompilationInfo::new(
        atoms,
        slices,
        scope_data_map,
        function_declarations,
        function_stencil_indices,
        function_declaration_properties,
        functions,
    );
    ast_emitter::emit_program(ast, options, compilation_info)
}

#[cfg(test)]
mod tests {
    extern crate jsparagus_parser as parser;

    use super::{emit, EmitOptions};
    use crate::dis::*;
    use ast::source_atom_set::SourceAtomSet;
    use ast::source_slice_list::SourceSliceList;
    use bumpalo::Bump;
    use parser::{parse_script, ParseOptions};
    use std::cell::RefCell;
    use std::rc::Rc;
    use stencil::opcode::*;

    fn bytecode(source: &str) -> Vec<u8> {
        let alloc = &Bump::new();
        let parse_options = ParseOptions::new();
        let atoms = Rc::new(RefCell::new(SourceAtomSet::new()));
        let slices = Rc::new(RefCell::new(SourceSliceList::new()));
        let parse_result =
            parse_script(alloc, source, &parse_options, atoms.clone(), slices.clone())
                .expect("Failed to parse");
        // println!("{:?}", parse_result);

        let emit_options = EmitOptions::new();

        let result = emit(
            alloc.alloc(ast::types::Program::Script(parse_result.unbox())),
            &emit_options,
            atoms.replace(SourceAtomSet::new_uninitialized()),
            slices.replace(SourceSliceList::new()),
        )
        .expect("Should work!");

        let script_data_index: usize = result
            .top_level_script
            .immutable_script_data
            .expect("Top level script should have ImmutableScriptData")
            .into();
        let script_data = &result.script_data_list[script_data_index];
        let bytecode = &script_data.bytecode;

        println!("{}", dis(&bytecode));
        bytecode.to_vec()
    }

    #[test]
    fn it_works() {
        assert_eq!(
            bytecode("2 + 2"),
            vec![
                Opcode::Int8 as u8,
                2,
                Opcode::Int8 as u8,
                2,
                Opcode::Add as u8,
                Opcode::SetRval as u8,
                Opcode::RetRval as u8,
            ]
        )
    }

    #[test]
    fn dis_call() {
        assert_eq!(
            bytecode("dis()"),
            vec![
                Opcode::GetGName as u8,
                1,
                0,
                0,
                0,
                Opcode::GImplicitThis as u8,
                1,
                0,
                0,
                0,
                Opcode::Call as u8,
                0,
                0,
                Opcode::SetRval as u8,
                Opcode::RetRval as u8,
            ]
        )
    }

    #[test]
    fn literals() {
        assert_eq!(
            bytecode("true"),
            vec![
                Opcode::True as u8,
                Opcode::SetRval as u8,
                Opcode::RetRval as u8,
            ]
        );
        assert_eq!(
            bytecode("false"),
            vec![
                Opcode::False as u8,
                Opcode::SetRval as u8,
                Opcode::RetRval as u8,
            ]
        );
        //assert_eq!(
        //    bytecode("'hello world'"),
        //    vec![
        //        Opcode::String as u8, 0, 0, 0, 0,
        //        Opcode::SetRval as u8,
        //        Opcode::RetRval as u8,
        //    ]
        //);
    }
}
