//! Utilies for running in a build script.

use atty;
use file_text::FileText;
use grammar::parse_tree as pt;
use grammar::repr as r;
use lalrpop_util::ParseError;
use lexer::intern_token;
use lr1;
use message::builder::InlineBuilder;
use message::{Content, Message};
use normalize;
use parser;
use rust::RustWrite;
use session::{ColorConfig, Session};
use sha2::{Digest, Sha256};
use term;
use tls::Tls;
use tok;

use std::fs;
use std::io::{self, BufRead, Write};
use std::path::{Path, PathBuf};
use std::process::exit;
use std::rc::Rc;

mod action;
mod fake_term;

use self::fake_term::FakeTerminal;

const LALRPOP_VERSION_HEADER: &'static str = concat!(
    "// auto-generated: \"",
    env!("CARGO_PKG_NAME"),
    " ",
    env!("CARGO_PKG_VERSION"),
    "\""
);

fn hash_file(file: &Path) -> io::Result<String> {
    let mut file = try!(fs::File::open(&file));
    let mut sha_256 = Sha256::new();
    try!(io::copy(&mut file, &mut sha_256));

    let mut hash_str = "// sha256: ".to_owned();
    for byte in sha_256.result() {
        hash_str.push_str(&format!("{:x}", byte));
    }
    Ok(hash_str)
}

pub fn process_dir<P: AsRef<Path>>(session: Rc<Session>, root_dir: P) -> io::Result<()> {
    let lalrpop_files = try!(lalrpop_files(root_dir));
    for lalrpop_file in lalrpop_files {
        try!(process_file(session.clone(), lalrpop_file));
    }
    Ok(())
}

pub fn process_file<P: AsRef<Path>>(session: Rc<Session>, lalrpop_file: P) -> io::Result<()> {
    let lalrpop_file = lalrpop_file.as_ref();
    let rs_file = try!(resolve_rs_file(&session, lalrpop_file));
    let report_file = try!(resolve_report_file(&session, lalrpop_file));
    process_file_into(session, lalrpop_file, &rs_file, &report_file)
}

fn resolve_rs_file(session: &Session, lalrpop_file: &Path) -> io::Result<PathBuf> {
    gen_resolve_file(session, lalrpop_file, "rs")
}

fn resolve_report_file(session: &Session, lalrpop_file: &Path) -> io::Result<PathBuf> {
    gen_resolve_file(session, lalrpop_file, "report")
}

fn gen_resolve_file(session: &Session, lalrpop_file: &Path, ext: &str) -> io::Result<PathBuf> {
    let in_dir = if let Some(ref d) = session.in_dir {
        d.as_path()
    } else {
        Path::new(".")
    };
    let out_dir = if let Some(ref d) = session.out_dir {
        d.as_path()
    } else {
        in_dir
    };

    // If the lalrpop file is not in in_dir, the result is that the
    // .rs file is created in the same directory as the lalrpop file
    // for compatibility reasons
    Ok(out_dir
        .join(lalrpop_file.strip_prefix(&in_dir).unwrap_or(lalrpop_file))
        .with_extension(ext))
}

fn process_file_into(
    session: Rc<Session>,
    lalrpop_file: &Path,
    rs_file: &Path,
    report_file: &Path,
) -> io::Result<()> {
    if session.force_build || try!(needs_rebuild(&lalrpop_file, &rs_file)) {
        log!(
            session,
            Informative,
            "processing file `{}`",
            lalrpop_file.to_string_lossy()
        );
        if let Some(parent) = rs_file.parent() {
            try!(fs::create_dir_all(parent));
        }
        try!(make_read_only(&rs_file, false));
        try!(remove_old_file(&rs_file));

        // Load the LALRPOP source text for this file:
        let file_text = Rc::new(try!(FileText::from_path(lalrpop_file.to_path_buf())));

        // Store the session and file-text in TLS -- this is not
        // intended to be used in this high-level code, but it gives
        // easy access to this information pervasively in the
        // low-level LR(1) and grammar normalization code. This is
        // particularly useful for error-reporting.
        let _tls = Tls::install(session.clone(), file_text.clone());

        // Do the LALRPOP processing itself and write the resulting
        // buffer into a file. We use a buffer so that if LR(1)
        // generation fails at some point, we don't leave a partial
        // file behind.
        {
            let grammar = try!(parse_and_normalize_grammar(&session, &file_text));
            let buffer = try!(emit_recursive_ascent(&session, &grammar, &report_file));
            let mut output_file = try!(fs::File::create(&rs_file));
            try!(writeln!(output_file, "{}", LALRPOP_VERSION_HEADER));
            try!(writeln!(output_file, "{}", try!(hash_file(&lalrpop_file))));
            try!(output_file.write_all(&buffer));
        }

        try!(make_read_only(&rs_file, true));
    }
    Ok(())
}

fn remove_old_file(rs_file: &Path) -> io::Result<()> {
    match fs::remove_file(rs_file) {
        Ok(()) => Ok(()),
        Err(e) => {
            // Unix reports NotFound, Windows PermissionDenied!
            match e.kind() {
                io::ErrorKind::NotFound | io::ErrorKind::PermissionDenied => Ok(()),
                _ => Err(e),
            }
        }
    }
}

fn needs_rebuild(lalrpop_file: &Path, rs_file: &Path) -> io::Result<bool> {
    match fs::File::open(&rs_file) {
        Ok(rs_file) => {
            let mut version_str = String::new();
            let mut hash_str = String::new();

            let mut f = io::BufReader::new(rs_file);

            try!(f.read_line(&mut version_str));
            try!(f.read_line(&mut hash_str));

            Ok(hash_str.trim() != try!(hash_file(&lalrpop_file))
                || version_str.trim() != LALRPOP_VERSION_HEADER)
        }
        Err(e) => match e.kind() {
            io::ErrorKind::NotFound => Ok(true),
            _ => Err(e),
        },
    }
}

fn make_read_only(rs_file: &Path, ro: bool) -> io::Result<()> {
    if rs_file.is_file() {
        let rs_metadata = try!(fs::metadata(&rs_file));
        let mut rs_permissions = rs_metadata.permissions();
        rs_permissions.set_readonly(ro);
        fs::set_permissions(&rs_file, rs_permissions)
    } else {
        Ok(())
    }
}

fn lalrpop_files<P: AsRef<Path>>(root_dir: P) -> io::Result<Vec<PathBuf>> {
    let mut result = vec![];
    for entry in try!(fs::read_dir(root_dir)) {
        let entry = try!(entry);
        let file_type = try!(entry.file_type());

        let path = entry.path();

        if file_type.is_dir() {
            result.extend(try!(lalrpop_files(&path)));
        }

        if file_type.is_file()
            && path.extension().is_some()
            && path.extension().unwrap() == "lalrpop"
        {
            result.push(path);
        }
    }
    Ok(result)
}

fn parse_and_normalize_grammar(session: &Session, file_text: &FileText) -> io::Result<r::Grammar> {
    let grammar = match parser::parse_grammar(file_text.text()) {
        Ok(grammar) => grammar,

        Err(ParseError::InvalidToken { location }) => {
            let ch = file_text.text()[location..].chars().next().unwrap();
            report_error(
                &file_text,
                pt::Span(location, location),
                &format!("invalid character `{}`", ch),
            );
        }

        Err(ParseError::UnrecognizedToken {
            token: None,
            expected: _,
        }) => {
            let len = file_text.text().len();
            report_error(
                &file_text,
                pt::Span(len, len),
                &format!("unexpected end of file"),
            );
        }

        Err(ParseError::UnrecognizedToken {
            token: Some((lo, _, hi)),
            expected,
        }) => {
            let _ = expected; // didn't implement this yet :)
            let text = &file_text.text()[lo..hi];
            report_error(
                &file_text,
                pt::Span(lo, hi),
                &format!("unexpected token: `{}`", text),
            );
        }

        Err(ParseError::ExtraToken { token: (lo, _, hi) }) => {
            let text = &file_text.text()[lo..hi];
            report_error(
                &file_text,
                pt::Span(lo, hi),
                &format!("extra token at end of input: `{}`", text),
            );
        }

        Err(ParseError::User { error }) => {
            let string = match error.code {
                tok::ErrorCode::UnrecognizedToken => "unrecognized token",
                tok::ErrorCode::UnterminatedEscape => "unterminated escape; missing '`'?",
                tok::ErrorCode::UnrecognizedEscape => {
                    "unrecognized escape; only \\n, \\r, \\t, \\\" and \\\\ are recognized"
                }
                tok::ErrorCode::UnterminatedStringLiteral => {
                    "unterminated string literal; missing `\"`?"
                }
                tok::ErrorCode::UnterminatedCharacterLiteral => {
                    "unterminated character literal; missing `'`?"
                }
                tok::ErrorCode::UnterminatedAttribute => "unterminated #! attribute; missing `]`?",
                tok::ErrorCode::ExpectedStringLiteral => "expected string literal; missing `\"`?",
                tok::ErrorCode::UnterminatedCode => {
                    "unterminated code block; perhaps a missing `;`, `)`, `]` or `}`?"
                }
            };

            report_error(
                &file_text,
                pt::Span(error.location, error.location + 1),
                string,
            )
        }
    };

    match normalize::normalize(session, grammar) {
        Ok(grammar) => Ok(grammar),
        Err(error) => report_error(&file_text, error.span, &error.message),
    }
}

fn report_error(file_text: &FileText, span: pt::Span, message: &str) -> ! {
    println!("{} error: {}", file_text.span_str(span), message);

    let out = io::stderr();
    let mut out = out.lock();
    file_text.highlight(span, &mut out).unwrap();

    exit(1);
}

fn report_messages(messages: Vec<Message>) -> term::Result<()> {
    let builder = InlineBuilder::new().begin_paragraphs();
    let builder = messages
        .into_iter()
        .fold(builder, |b, m| b.push(Box::new(m)));
    let content = builder.end().end();
    report_content(&*content)
}

fn report_content(content: &Content) -> term::Result<()> {
    // FIXME -- can we query the size of the terminal somehow?
    let canvas = content.emit_to_canvas(80);

    let try_colors = match Tls::session().color_config {
        ColorConfig::Yes => true,
        ColorConfig::No => false,
        ColorConfig::IfTty => atty::is(atty::Stream::Stdout),
    };

    if try_colors {
        if let Some(mut stdout) = term::stdout() {
            return canvas.write_to(&mut *stdout);
        }
    }

    let stdout = io::stdout();
    let mut stdout = FakeTerminal::new(stdout.lock());
    canvas.write_to(&mut stdout)
}

fn emit_module_attributes<W: Write>(
    grammar: &r::Grammar,
    rust: &mut RustWrite<W>,
) -> io::Result<()> {
    rust.write_module_attributes(grammar)
}

fn emit_uses<W: Write>(grammar: &r::Grammar, rust: &mut RustWrite<W>) -> io::Result<()> {
    rust.write_uses("", grammar)
}

fn emit_recursive_ascent(
    session: &Session,
    grammar: &r::Grammar,
    report_file: &Path,
) -> io::Result<Vec<u8>> {
    let mut rust = RustWrite::new(vec![]);

    // We generate a module structure like this:
    //
    // ```
    // mod <output-file> {
    //     // For each public symbol:
    //     pub fn parse_XYZ();
    //     mod __XYZ { ... }
    //
    //     // For each bit of action code:
    //     <action-code>
    // }
    // ```
    //
    // Note that the action code goes in the outer module.  This is
    // intentional because it means that the foo.lalrpop file serves
    // as a module in the rust hierarchy, so if the action code
    // includes things like `super::` it will resolve in the natural
    // way.

    try!(emit_module_attributes(grammar, &mut rust));
    try!(emit_uses(grammar, &mut rust));

    if grammar.start_nonterminals.is_empty() {
        println!("Error: no public symbols declared in grammar");
        exit(1);
    }

    for (user_nt, start_nt) in &grammar.start_nonterminals {
        // We generate these, so there should always be exactly 1
        // production. Otherwise the LR(1) algorithm doesn't know
        // where to stop!
        assert_eq!(grammar.productions_for(start_nt).len(), 1);

        log!(
            session,
            Verbose,
            "Building states for public nonterminal `{}`",
            user_nt
        );

        let _lr1_tls = lr1::Lr1Tls::install(grammar.terminals.clone());

        let lr1result = lr1::build_states(&grammar, start_nt.clone());
        if session.emit_report {
            let mut output_report_file = try!(fs::File::create(&report_file));
            try!(lr1::generate_report(&mut output_report_file, &lr1result));
        }

        let states = match lr1result {
            Ok(states) => states,
            Err(error) => {
                let messages = lr1::report_error(&grammar, &error);
                let _ = report_messages(messages);
                exit(1) // FIXME -- propagate up instead of calling `exit`
            }
        };

        match grammar.algorithm.codegen {
            r::LrCodeGeneration::RecursiveAscent => try!(lr1::codegen::ascent::compile(
                &grammar,
                user_nt.clone(),
                start_nt.clone(),
                &states,
                "super",
                &mut rust,
            )),
            r::LrCodeGeneration::TableDriven => try!(lr1::codegen::parse_table::compile(
                &grammar,
                user_nt.clone(),
                start_nt.clone(),
                &states,
                "super",
                &mut rust,
            )),

            r::LrCodeGeneration::TestAll => try!(lr1::codegen::test_all::compile(
                &grammar,
                user_nt.clone(),
                start_nt.clone(),
                &states,
                &mut rust,
            )),
        }

        rust!(
            rust,
            "{}use self::{}parse{}::{}Parser;",
            grammar.nonterminals[&user_nt].visibility,
            grammar.prefix,
            start_nt,
            user_nt
        );
    }

    if let Some(ref intern_token) = grammar.intern_token {
        try!(intern_token::compile(&grammar, intern_token, &mut rust));
        rust!(rust, "pub use self::{}intern_token::Token;", grammar.prefix);
    }

    try!(action::emit_action_code(grammar, &mut rust));

    try!(emit_to_triple_trait(grammar, &mut rust));

    Ok(rust.into_inner())
}

fn emit_to_triple_trait<W: Write>(grammar: &r::Grammar, rust: &mut RustWrite<W>) -> io::Result<()> {
    #![allow(non_snake_case)]

    let L = grammar.types.terminal_loc_type();
    let T = grammar.types.terminal_token_type();
    let E = grammar.types.error_type();

    let mut user_type_parameters = String::new();
    for type_parameter in &grammar.type_parameters {
        user_type_parameters.push_str(&format!("{}, ", type_parameter));
    }

    rust!(rust, "");
    rust!(
        rust,
        "pub trait {}ToTriple<{}> {{",
        grammar.prefix,
        user_type_parameters,
    );
    rust!(rust, "type Error;");
    rust!(
        rust,
        "fn to_triple(value: Self) -> Result<({},{},{}),Self::Error>;",
        L,
        T,
        L,
    );
    rust!(rust, "}}");

    rust!(rust, "");
    if grammar.types.opt_terminal_loc_type().is_some() {
        rust!(
            rust,
            "impl<{}> {}ToTriple<{}> for ({}, {}, {}) {{",
            user_type_parameters,
            grammar.prefix,
            user_type_parameters,
            L,
            T,
            L,
        );
        rust!(rust, "type Error = {};", E);
        rust!(
            rust,
            "fn to_triple(value: Self) -> Result<({},{},{}),{}> {{",
            L,
            T,
            L,
            E,
        );
        rust!(rust, "Ok(value)");
        rust!(rust, "}}");
        rust!(rust, "}}");

        rust!(
            rust,
            "impl<{}> {}ToTriple<{}> for Result<({}, {}, {}),{}> {{",
            user_type_parameters,
            grammar.prefix,
            user_type_parameters,
            L,
            T,
            L,
            E,
        );
        rust!(rust, "type Error = {};", E);
        rust!(
            rust,
            "fn to_triple(value: Self) -> Result<({},{},{}),{}> {{",
            L,
            T,
            L,
            E,
        );
        rust!(rust, "value");
        rust!(rust, "}}");
        rust!(rust, "}}");
    } else {
        rust!(
            rust,
            "impl<{}> {}ToTriple<{}> for {} {{",
            user_type_parameters,
            grammar.prefix,
            user_type_parameters,
            T,
        );
        rust!(rust, "type Error = {};", E);
        rust!(
            rust,
            "fn to_triple(value: Self) -> Result<((),{},()),{}> {{",
            T,
            E,
        );
        rust!(rust, "Ok(((), value, ()))");
        rust!(rust, "}}");
        rust!(rust, "}}");

        rust!(
            rust,
            "impl<{}> {}ToTriple<{}> for Result<({}),{}> {{",
            user_type_parameters,
            grammar.prefix,
            user_type_parameters,
            T,
            E,
        );
        rust!(rust, "type Error = {};", E);
        rust!(
            rust,
            "fn to_triple(value: Self) -> Result<((),{},()),{}> {{",
            T,
            E,
        );
        rust!(rust, "value.map(|v| ((), v, ()))");
        rust!(rust, "}}");
        rust!(rust, "}}");
    }

    Ok(())
}
