use std::fs::File;
use std::io::Write;
use std::path::Path;
use std::rc::Rc;

use syntex_syntax::ast;
use syntex_syntax::attr;
use syntex_syntax::codemap::DUMMY_SP;
use syntex_syntax::ext::base::{
    ExtCtxt,
    IdentMacroExpander,
    MultiItemDecorator,
    MultiItemModifier,
    NamedSyntaxExtension,
    Resolver,
    SyntaxExtension,
    TTMacroExpander,
};
use syntex_syntax::ext::expand;
use syntex_syntax::feature_gate;
use syntex_syntax::parse;
use syntex_syntax::print::pprust;
use syntex_syntax::symbol::Symbol;

use super::resolver;
use super::error::Error;

pub type Pass = fn(ast::Crate) -> ast::Crate;

pub struct Registry {
    syntax_exts: Vec<NamedSyntaxExtension>,
    pre_expansion_passes: Vec<Box<Pass>>,
    post_expansion_passes: Vec<Box<Pass>>,
    cfg: Vec<ast::MetaItem>,
    attrs: Vec<ast::Attribute>,
}

impl Registry {
    pub fn new() -> Registry {
        Registry {
            syntax_exts: Vec::new(),
            pre_expansion_passes: Vec::new(),
            post_expansion_passes: Vec::new(),
            cfg: Vec::new(),
            attrs: Vec::new(),
        }
    }

    pub fn add_cfg(&mut self, cfg: &str) {
        let parse_sess = parse::ParseSess::new();
        let meta_item = parse::parse_meta_from_source_str(
            "cfgspec".to_string(),
            cfg.to_string(),
            &parse_sess).unwrap();

        self.cfg.push(meta_item);
    }

    pub fn add_attr(&mut self, attr: &str) {
        let parse_sess = parse::ParseSess::new();
        let meta_item = parse::parse_meta_from_source_str(
            "attrspec".to_string(),
            attr.to_string(),
            &parse_sess).unwrap();

        self.attrs.push(ast::Attribute {
            id: attr::mk_attr_id(),
            style: ast::AttrStyle::Outer,
            value: meta_item,
            is_sugared_doc: false,
            span: DUMMY_SP,
        });
    }

    pub fn add_macro<F>(&mut self, name: &str, extension: F)
        where F: TTMacroExpander + 'static
    {
        let name = Symbol::intern(name);
        let syntax_extension = SyntaxExtension::NormalTT(
            Box::new(extension),
            None,
            false
        );
        self.syntax_exts.push((name, syntax_extension));
    }

    pub fn add_ident_macro<F>(&mut self, name: &str, extension: F)
        where F: IdentMacroExpander + 'static
    {
        let name = Symbol::intern(name);
        let syntax_extension = SyntaxExtension::IdentTT(
            Box::new(extension),
            None,
            false
        );
        self.syntax_exts.push((name, syntax_extension));
    }

    pub fn add_decorator<F>(&mut self, name: &str, extension: F)
        where F: MultiItemDecorator + 'static
    {
        let name = Symbol::intern(name);
        let syntax_extension = SyntaxExtension::MultiDecorator(Box::new(extension));
        self.syntax_exts.push((name, syntax_extension));
    }

    pub fn add_modifier<F>(&mut self, name: &str, extension: F)
        where F: MultiItemModifier + 'static
    {
        let name = Symbol::intern(name);
        let syntax_extension = SyntaxExtension::MultiModifier(Box::new(extension));
        self.syntax_exts.push((name, syntax_extension));
    }

    pub fn add_pre_expansion_pass(&mut self, pass: Pass) {
        self.pre_expansion_passes.push(Box::new(pass))
    }

    pub fn add_post_expansion_pass(&mut self, pass: Pass) {
        self.post_expansion_passes.push(Box::new(pass))
    }

    pub fn expand<S, D>(self, crate_name: &str, src: S, dst: D) -> Result<(), Error>
        where S: AsRef<Path>,
              D: AsRef<Path>,
    {
        let src = src.as_ref();
        let dst = dst.as_ref();

        let sess = parse::ParseSess::new();

        let krate = try!(parse::parse_crate_from_file(
            src,
            &sess));

        if sess.span_diagnostic.has_errors() {
            return Err(Error::Parse);
        }

        let src_name = src.to_str().unwrap().to_string();

        let out = try!(self.expand_crate(
                crate_name,
                &sess,
                src_name,
                krate));

        if sess.span_diagnostic.has_errors() {
            return Err(Error::Expand);
        }

        let mut dst = try!(File::create(dst));
        try!(dst.write_all(&out));
        Ok(())
    }

    /// This method will expand all macros in the source string `src`, and return the results in a
    /// string.
    pub fn expand_str(self,
                      crate_name: &str,
                      src_name: &str,
                      src: &str) -> Result<String, Error> {
        let sess = parse::ParseSess::new();

        let src_name = src_name.to_owned();

        let krate = try!(parse::parse_crate_from_source_str(
            src_name.clone(),
            src.to_owned(),
            &sess));

        let out = try!(self.expand_crate(crate_name, &sess, src_name, krate));

        Ok(String::from_utf8(out).unwrap())
    }

    fn expand_crate(self,
                    crate_name: &str,
                    sess: &parse::ParseSess,
                    src_name: String,
                    mut krate: ast::Crate) -> Result<Vec<u8>, Error> {
        krate.attrs.extend(self.attrs.iter().cloned());

        let features = feature_gate::get_features(
            &sess.span_diagnostic,
            &krate.attrs);

        let krate = self.pre_expansion_passes.iter()
            .fold(krate, |krate, f| (f)(krate));

        let mut ecfg = expand::ExpansionConfig::default(crate_name.to_owned());
        ecfg.features = Some(&features);
        ecfg.keep_macs = true;

        let mut resolver = resolver::Resolver::new(sess);

        for (name, ext) in self.syntax_exts {
            let ident = ast::Ident::with_empty_ctxt(name);
            resolver.add_ext(ident, Rc::new(ext));
        }

        let mut ecx = ExtCtxt::new(&sess, ecfg, &mut resolver);
        let krate = ecx.monotonic_expander().expand_crate(krate);

        let krate = self.post_expansion_passes.iter()
            .fold(krate, |krate, f| (f)(krate));

        let src = sess.codemap()
            .get_filemap(&src_name)
            .unwrap()
            .src
            .as_ref()
            .unwrap()
            .as_bytes()
            .to_vec();

        let mut rdr = &src[..];

        let mut out = Vec::new();
        let annotation = pprust::NoAnn;

        try!(pprust::print_crate(
            sess.codemap(),
            &sess,
            &krate,
            src_name,
            &mut rdr,
            Box::new(&mut out),
            &annotation,
            false));

        Ok(out)
    }
}
