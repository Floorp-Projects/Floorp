use fluent_syntax::ast;
use fluent_syntax::parser::Parser;
use fluent_syntax::parser::ParserError;

rental! {
    mod rentals {
        use super::*;
        #[rental(covariant, debug)]
        pub struct FluentResource {
            string: String,
            ast: ast::Resource<&'string str>,
        }
    }
}

/// A resource containing a list of localization messages.
#[derive(Debug)]
pub struct FluentResource(rentals::FluentResource);

impl FluentResource {
    pub fn try_new(source: String) -> Result<Self, (Self, Vec<ParserError>)> {
        let mut errors = None;
        let res = rentals::FluentResource::new(source, |s| match Parser::new(s).parse() {
            Ok(ast) => ast,
            Err((ast, err)) => {
                errors = Some(err);
                ast
            }
        });

        if let Some(errors) = errors {
            Err((Self(res), errors))
        } else {
            Ok(Self(res))
        }
    }

    pub fn ast(&self) -> &ast::Resource<&str> {
        self.0.all().ast
    }
}
