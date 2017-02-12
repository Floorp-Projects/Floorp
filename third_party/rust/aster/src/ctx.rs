use syntax::ast;
use syntax::parse::token;

//////////////////////////////////////////////////////////////////////////////

#[derive(Copy)]
pub struct Ctx;

impl Ctx {
    pub fn new() -> Ctx {
        Ctx
    }

    pub fn intern(&self, name: &str) -> ast::Name {
        token::intern(name)
    }
}
