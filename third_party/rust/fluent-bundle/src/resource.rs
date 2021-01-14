use fluent_syntax::ast;
use fluent_syntax::parser::Parser;
use fluent_syntax::parser::ParserError;
use ouroboros::self_referencing;

#[self_referencing]
#[derive(Debug)]
pub struct InnerFluentResource {
    string: String,
    #[borrows(string)]
    ast: ast::Resource<&'this str>,
}

/// A resource containing a list of localization messages.
#[derive(Debug)]
pub struct FluentResource(InnerFluentResource);

impl FluentResource {
    pub fn try_new(source: String) -> Result<Self, (Self, Vec<ParserError>)> {
        let mut errors = None;

        let res = InnerFluentResourceBuilder {
            string: source,
            ast_builder: |string: &str| match Parser::new(string).parse() {
                Ok(ast) => ast,
                Err((ast, err)) => {
                    errors = Some(err);
                    ast
                }
            },
        }
        .build();

        if let Some(errors) = errors {
            Err((Self(res), errors))
        } else {
            Ok(Self(res))
        }
    }

    pub fn ast(&self) -> &ast::Resource<&str> {
        self.0.borrow_ast()
    }
}
