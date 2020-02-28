mod ast_emitter;
mod dis;
mod emitter;
mod forward_jump_emitter;
mod lower;
pub mod opcode;

extern crate jsparagus_ast as ast;

pub use crate::emitter::{EmitError, EmitOptions, EmitResult};
pub use dis::dis;

pub fn emit(ast: &mut ast::types::Program, options: &EmitOptions) -> Result<EmitResult, EmitError> {
    //lower::run(ast);
    ast_emitter::emit_program(ast, options)
}

#[cfg(test)]
mod tests {
    extern crate jsparagus_parser as parser;

    use super::{emit, EmitOptions};
    use crate::dis::*;
    use crate::opcode::*;
    use bumpalo::Bump;
    use parser::{parse_script, ParseOptions};

    fn bytecode(source: &str) -> Vec<u8> {
        let alloc = &Bump::new();
        let parse_options = ParseOptions::new();
        let parse_result = parse_script(alloc, source, &parse_options).expect("Failed to parse");
        // println!("{:?}", parse_result);

        let emit_options = EmitOptions::new();
        let bc = emit(
            &mut ast::types::Program::Script(parse_result.unbox()),
            &emit_options,
        )
        .expect("Should work!")
        .bytecode;
        println!("{}", dis(&bc));
        bc
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
                0,
                0,
                0,
                0,
                Opcode::GImplicitThis as u8,
                0,
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
