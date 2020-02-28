mod scope;

use ast::types::Program;

#[allow(dead_code)]
pub fn run(_ast: &mut Program) {
    unimplemented!();
    //scope::postfix::pass(ast);
}

#[cfg(test)]
mod tests {
    // XXX FIXME - generic AST pass mechanism is disabled
    // use super::*;
    // use bumpalo::Bump;
    // use parser::parse_script;
    // use std::error::Error;
    //
    // #[test]
    // fn it_works() -> Result<(), Box<dyn Error>> {
    //     let allocator = &Bump::new();
    //     let parse_result = parse_script(allocator, "wau")?;
    //     run(&mut ast::Program::Script(parse_result.unbox()));
    //     Ok(())
    // }
}
