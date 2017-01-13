use std::collections::HashMap;
use std::rc::Rc;

use syntex_syntax::ast::{self, Attribute, Name};
use syntex_syntax::ext::base::{
    self,
    Determinacy,
    MultiDecorator,
    MultiModifier,
    SyntaxExtension,
};
use syntex_syntax::ext::expand::Expansion;
use syntex_syntax::ext::hygiene::Mark;
use syntex_syntax::parse::ParseSess;
use syntex_syntax::ptr::P;

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
    fn eliminate_crate_var(&mut self, item: P<ast::Item>) -> P<ast::Item> { item }
    fn is_whitelisted_legacy_custom_derive(&self, _name: Name) -> bool { false }

    fn visit_expansion(&mut self, _invoc: Mark, _expansion: &Expansion) {}
    fn add_ext(&mut self, ident: ast::Ident, ext: Rc<SyntaxExtension>) {
        self.extensions.insert(ident.name, ext);
    }
    fn add_expansions_at_stmt(&mut self, _id: ast::NodeId, _macros: Vec<Mark>) {}

    fn resolve_imports(&mut self) {}
    fn find_attr_invoc(&mut self, attrs: &mut Vec<Attribute>) -> Option<Attribute> {
        for i in 0..attrs.len() {
            if let Some(ext) = self.extensions.get(&attrs[i].value.name) {
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
    fn resolve_macro(&mut self, scope: Mark, path: &ast::Path, force: bool)
                     -> Result<Rc<SyntaxExtension>, Determinacy> {
        let ast::Path { ref segments, span } = *path;
        if segments.iter().any(|segment| segment.parameters.is_some()) {
            let kind =
                if segments.last().unwrap().parameters.is_some() { "macro" } else { "module" };
            let msg = format!("type parameters are not allowed on {}s", kind);
            self.session.span_diagnostic.span_err(path.span, &msg);
            return Err(Determinacy::Determined);
        }

        let path: Vec<_> = segments.iter().map(|seg| seg.identifier).collect();

        if path.len() > 1 {
            // FIXME(syntex): Pass macros with module separators through to the generated source.
            self.session.span_diagnostic.span_err(span, "expected macro name without module separators");
            return Err(Determinacy::Determined);
        }        

        let name = path[0].name;

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
