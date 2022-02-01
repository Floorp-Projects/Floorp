use glsl::parser::Parse;
use glsl::syntax;

#[test]
fn left_associativity() {
  for (opstr, opname) in [
    ("+", syntax::BinaryOp::Add),
    ("&&", syntax::BinaryOp::And),
    ("||", syntax::BinaryOp::Or),
  ]
  .iter()
  {
    let r = syntax::TranslationUnit::parse(format!(
      "
      void main() {{
        x = a {op} b {op} c;
      }}
    ",
      op = opstr
    ));

    let expected = syntax::TranslationUnit::from_non_empty_iter(vec![
      syntax::ExternalDeclaration::FunctionDefinition(syntax::FunctionDefinition {
        prototype: syntax::FunctionPrototype {
          ty: syntax::FullySpecifiedType {
            qualifier: None,
            ty: syntax::TypeSpecifier {
              ty: syntax::TypeSpecifierNonArray::Void,
              array_specifier: None,
            },
          },
          name: "main".into(),
          parameters: Vec::new(),
        },
        statement: syntax::CompoundStatement {
          statement_list: vec![syntax::Statement::Simple(Box::new(
            syntax::SimpleStatement::Expression(Some(syntax::Expr::Assignment(
              Box::new(syntax::Expr::Variable("x".into())),
              syntax::AssignmentOp::Equal,
              Box::new(syntax::Expr::Binary(
                opname.clone(),
                Box::new(syntax::Expr::Binary(
                  opname.clone(),
                  Box::new(syntax::Expr::Variable("a".into())),
                  Box::new(syntax::Expr::Variable("b".into())),
                )),
                Box::new(syntax::Expr::Variable("c".into())),
              )),
            ))),
          ))],
        },
      }),
    ])
    .unwrap();

    assert_eq!(r, Ok(expected));
  }
}
