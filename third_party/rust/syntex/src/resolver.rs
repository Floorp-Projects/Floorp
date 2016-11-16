use std::collections::HashMap;
use std::rc::Rc;

use syntex_syntax::ast::{self, Attribute};
use syntex_syntax::ext::base::{
    self,
    Determinacy,
    MultiDecorator,
    MultiModifier,
    SyntaxExtension,
};
use syntex_syntax::ext::expand::Expansion;
use syntex_syntax::ext::hygiene::Mark;
use syntex_syntax::parse::token::intern;
use syntex_syntax::parse::ParseSess;

pub struct Resolver<'a> {
    session: &'a ParseSess,
    extensions: HashMap<ast::Name, Rc<SyntaxExtension>>,
}

impl<'a> Resolver<'a> {
    pub fn new(session: &'a ParseSess) -> Self {
        Resolver {
            session: session,
            extensions: HashMap::new(),
        }
    }
}

impl<'a> base::Resolver for Resolver<'a> {
    fn next_node_id(&mut self) -> ast::NodeId {
        ast::DUMMY_NODE_ID
    }
    fn get_module_scope(&mut self, _id: ast::NodeId) -> Mark { Mark::root() }

    fn visit_expansion(&mut self, _invoc: Mark, _expansion: &Expansion) {}
    fn add_macro(&mut self, _scope: Mark, def: ast::MacroDef, _export: bool) {
        self.session.span_diagnostic.span_bug(
            def.span,
            "add_macro is not supported yet");
    }
    fn add_ext(&mut self, ident: ast::Ident, ext: Rc<SyntaxExtension>) {
        self.extensions.insert(ident.name, ext);
    }
    fn add_expansions_at_stmt(&mut self, _id: ast::NodeId, _macros: Vec<Mark>) {}

    fn find_attr_invoc(&mut self, attrs: &mut Vec<Attribute>) -> Option<Attribute> {
        for i in 0..attrs.len() {
            let name = intern(&attrs[i].name());

            if let Some(ext) = self.extensions.get(&name) {
                match **ext {
                    MultiModifier(..) | MultiDecorator(..) => return Some(attrs.remove(i)),
                    _ => {}
                }
            }
        }
        None
    }
    fn find_extension(&mut self, _scope: Mark, name: ast::Name) -> Option<Rc<SyntaxExtension>> {
        self.extensions.get(&name).map(|ext| ext.clone())
    }
    fn find_mac(&mut self, scope: Mark, mac: &ast::Mac) -> Option<Rc<SyntaxExtension>> {
        let path = &mac.node.path;
        if path.segments.len() > 1 || path.global ||
           !path.segments[0].parameters.is_empty() {
            // NOTE: Pass macros with module separators through to the generated source.
            self.session.span_diagnostic.span_err(path.span,
                                                  "expected macro name without module separators");
            return None;
        }
        let name = path.segments[0].identifier.name;
        self.find_extension(scope, name)
    }
    fn resolve_macro(&mut self, scope: Mark, path: &ast::Path, force: bool)
                     -> Result<Rc<SyntaxExtension>, Determinacy> {
        if path.segments.len() > 1 || path.global ||
            !path.segments[0].parameters.is_empty() {
            // NOTE: Pass macros with module separators through to the generated source.
            self.session.span_diagnostic.span_err(path.span,
                                                    "expected macro name without module separators");
            return Err(Determinacy::Undetermined);
        }
        let name = path.segments[0].identifier.name;
        let span = path.span;

        self.find_extension(scope, name).ok_or_else(|| {
            if force {
                let mut err =
                    self.session.span_diagnostic.struct_span_err(span, &format!("macro undefined: '{}!'", name));
                err.emit();
                Determinacy::Determined
            } else {
                Determinacy::Undetermined
            }
        })
    }
}
