pub mod full {
    use ast::{arena, types::*, visit::Pass};
    use bumpalo::Bump;
    use std::mem::replace;

    #[allow(dead_code)]
    pub fn pass<'alloc>(allocator: &'alloc Bump, ast: &mut Program<'alloc>) {
        ScopePass {
            allocator,
            top_scope: Scope {
                declarations: arena::Vec::new_in(allocator),
            },
        }
        .visit_program(ast);
    }

    struct Scope<'alloc> {
        declarations: arena::Vec<'alloc, &'alloc str>,
    }

    struct ScopePass<'alloc> {
        allocator: &'alloc bumpalo::Bump,
        top_scope: Scope<'alloc>,
    }

    impl<'alloc> Pass<'alloc> for ScopePass<'alloc> {
        fn visit_binding_identifier(&mut self, ast: &mut BindingIdentifier<'alloc>) {
            self.visit_identifier(&mut ast.name);
        }

        fn visit_block(&mut self, ast: &mut Block<'alloc>) {
            let old_scope = replace(
                &mut self.top_scope,
                Scope {
                    declarations: arena::Vec::new_in(self.allocator),
                },
            );
            for item in &mut ast.statements {
                self.visit_statement(item);
            }
            let this_scope = replace(&mut self.top_scope, old_scope);
            ast.declarations = Some(this_scope.declarations);
        }
    }
}

pub mod postfix {
    //use ast::visit::{PostfixPass, PostfixPassMonoid, PostfixPassVisitor};
    use ast::arena;
    //use ast::types::*;

    // #[allow(dead_code)]
    // pub fn pass(ast: &mut Program) {
    //     PostfixPassVisitor::new(ScopePass {}).visit_program(ast);
    // }

    #[allow(dead_code)]
    #[derive(Debug)]
    struct ScopeInfo<'alloc> {
        declarations: arena::Vec<'alloc, arena::String<'alloc>>,
        references: arena::Vec<'alloc, arena::String<'alloc>>,
    }

    // impl<'alloc> PostfixPassMonoid for ScopeInfo<'alloc> {
    //     fn append(&mut self, mut other: Self) {
    //         self.declarations.append(&mut other.declarations);
    //         self.references.append(&mut other.references);
    //     }
    // }
    //
    // struct ScopePass<'alloc> {
    //     lifetime_marker: std::marker::PhantomData<&'alloc Script<'alloc>>,
    // }
    //
    // impl<'alloc> PostfixPass<'alloc> for ScopePass<'alloc> {
    //     type Value = ScopeInfo<'alloc>;
    //
    //     fn visit_identifier(&self, name: &mut arena::String<'alloc>) -> ScopeInfo {
    //         ScopeInfo {
    //             declarations: Vec::new(),
    //             references: vec![name.to_string()],
    //         }
    //     }
    //
    //     fn visit_variable_declarator(
    //         &self,
    //         mut binding: ScopeInfo,
    //         init: Option<ScopeInfo>,
    //     ) -> Self::Value {
    //         // TODO: This is a little gross. We assume that the eventual Identifier contained
    //         // within VariableDeclarator->BindingIdentifier->Identifier is the only binding, and no
    //         // references can occur (e.g. can destructuring exprs contain ident references?)
    //         let mut result = ScopeInfo {
    //             declarations: binding.references,
    //             references: Vec::new(),
    //         };
    //         result.declarations.append(&mut binding.declarations);
    //         if let Some(item) = init {
    //             result.append(item);
    //         }
    //         result
    //     }
    //
    //     fn visit_block(
    //         &self,
    //         mut statements: arena::Vec<'alloc, Self::Value>,
    //         declarations: &mut Option<arena::Vec<'alloc, String>>,
    //     ) -> ScopeInfo {
    //         let mut declared_here = Vec::new();
    //         let mut free_vars = Vec::new();
    //         let statements = statements
    //             .drain(..)
    //             .fold(ScopeInfo::default(), |mut sum, item| {
    //                 sum.append(item);
    //                 sum
    //             });
    //         for reference in statements.references {
    //             if statements.declarations.contains(&reference) {
    //                 declared_here.push(reference);
    //             } else {
    //                 free_vars.push(reference);
    //             }
    //         }
    //         *declarations = Some(declared_here);
    //         ScopeInfo {
    //             declarations: Vec::new(),
    //             references: free_vars,
    //         }
    //     }
    // }
}
