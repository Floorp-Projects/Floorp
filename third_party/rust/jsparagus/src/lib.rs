pub mod ast {
    extern crate jsparagus_ast;
    pub use self::jsparagus_ast::*;
}

pub mod emitter {
    extern crate jsparagus_emitter;
    pub use self::jsparagus_emitter::*;
}

pub mod parser {
    extern crate jsparagus_parser;
    pub use self::jsparagus_parser::*;
}

pub mod scope {
    extern crate jsparagus_scope;
    pub use self::jsparagus_scope::*;
}
