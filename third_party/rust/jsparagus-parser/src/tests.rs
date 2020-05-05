use std::iter;

use crate::lexer::Lexer;
use crate::parser::Parser;
use crate::{parse_script, ParseOptions};
use ast::source_atom_set::SourceAtomSet;
use ast::source_slice_list::SourceSliceList;
use ast::{arena, source_location::SourceLocation, types::*};
use bumpalo::{self, Bump};
use generated_parser::{self, AstBuilder, ParseError, Result, TerminalId};
use std::cell::RefCell;
use std::rc::Rc;

#[cfg(all(feature = "unstable", test))]
mod benchmarks {
    extern crate test;

    use std::fs::File;
    use std::io::Read;
    use test::Bencher;

    use crate::lexer::Lexer;
    use crate::parse_script;

    #[bench]
    fn bench_parse_grammar(b: &mut Bencher) {
        let mut buffer = fs::read_to_string("../vue.js").expect("reading test file");
        b.iter(|| {
            let lexer = Lexer::new(buffer.chars());
            parse_script(lexer).unwrap();
        });
    }
}

trait IntoChunks<'a> {
    type Chunks: Iterator<Item = &'a str>;
    fn into_chunks(self) -> Self::Chunks;
}

impl<'a> IntoChunks<'a> for &'a str {
    type Chunks = iter::Once<&'a str>;
    fn into_chunks(self) -> Self::Chunks {
        iter::once(self)
    }
}

impl<'a> IntoChunks<'a> for &'a Vec<&'a str> {
    type Chunks = iter::Cloned<std::slice::Iter<'a, &'a str>>;
    fn into_chunks(self) -> Self::Chunks {
        self.iter().cloned()
    }
}

// Glue all the chunks together.  XXX TODO Once the lexer supports chunks,
// we'll reimplement this to feed the code to the lexer one chunk at a time.
fn chunks_to_string<'a, T: IntoChunks<'a>>(code: T) -> String {
    let mut buf = String::new();
    for chunk in code.into_chunks() {
        buf.push_str(chunk);
    }
    buf
}

fn try_parse<'alloc, 'source, Source>(
    allocator: &'alloc Bump,
    code: Source,
) -> Result<'alloc, arena::Box<'alloc, Script<'alloc>>>
where
    Source: IntoChunks<'source>,
{
    let buf = arena::alloc_str(allocator, &chunks_to_string(code));
    let options = ParseOptions::new();
    let atoms = Rc::new(RefCell::new(SourceAtomSet::new()));
    let slices = Rc::new(RefCell::new(SourceSliceList::new()));
    parse_script(allocator, &buf, &options, atoms, slices)
}

fn assert_parses<'alloc, T: IntoChunks<'alloc>>(code: T) {
    let allocator = &Bump::new();
    try_parse(allocator, code).unwrap();
}

fn assert_error<'alloc, T: IntoChunks<'alloc>>(code: T) {
    let allocator = &Bump::new();
    assert!(match try_parse(allocator, code).map_err(|e| *e) {
        Err(ParseError::NotImplemented(_)) => panic!("expected error, got NotImplemented"),
        Err(_) => true,
        Ok(ast) => panic!("assertion failed: SUCCESS error: {:?}", ast),
    });
}

fn assert_syntax_error<'alloc, T: IntoChunks<'alloc>>(code: T) {
    let allocator = &Bump::new();
    assert!(match try_parse(allocator, code).map_err(|e| *e) {
        Err(ParseError::SyntaxError(_)) => true,
        Err(other) => panic!("unexpected error: {:?}", other),
        Ok(ast) => panic!("assertion failed: SUCCESS error: {:?}", ast),
    });
}

fn assert_not_implemented<'alloc, T: IntoChunks<'alloc>>(code: T) {
    let allocator = &Bump::new();
    assert!(match try_parse(allocator, code).map_err(|e| *e) {
        Err(ParseError::NotImplemented(_)) => true,
        Err(other) => panic!("unexpected error: {:?}", other),
        Ok(ast) => panic!("assertion failed: SUCCESS error: {:?}", ast),
    });
}

fn assert_illegal_character<'alloc, T: IntoChunks<'alloc>>(code: T) {
    let allocator = &Bump::new();
    assert!(match try_parse(allocator, code).map_err(|e| *e) {
        Err(ParseError::IllegalCharacter(_)) => true,
        Err(other) => panic!("unexpected error: {:?}", other),
        Ok(ast) => panic!("assertion failed: SUCCESS error: {:?}", ast),
    });
}

fn assert_error_eq<'alloc, T: IntoChunks<'alloc>>(code: T, expected: ParseError) {
    let allocator = &Bump::new();
    let result = try_parse(allocator, code);
    assert!(result.is_err());
    assert_eq!(*result.unwrap_err(), expected);
}

fn assert_incomplete<'alloc, T: IntoChunks<'alloc>>(code: T) {
    let allocator = &Bump::new();
    let result = try_parse(allocator, code);
    assert!(result.is_err());
    assert_eq!(*result.unwrap_err(), ParseError::UnexpectedEnd);
}

// Assert that `left` and `right`, when parsed as ES Modules, consist of the
// same sequence of tokens (although possibly at different offsets).
fn assert_same_tokens<'alloc>(left: &str, right: &str) {
    let allocator = &Bump::new();
    let left_atoms = Rc::new(RefCell::new(SourceAtomSet::new()));
    let left_slices = Rc::new(RefCell::new(SourceSliceList::new()));
    let right_atoms = Rc::new(RefCell::new(SourceAtomSet::new()));
    let right_slices = Rc::new(RefCell::new(SourceSliceList::new()));
    let mut left_lexer = Lexer::new(
        allocator,
        left.chars(),
        left_atoms.clone(),
        left_slices.clone(),
    );
    let mut right_lexer = Lexer::new(
        allocator,
        right.chars(),
        right_atoms.clone(),
        right_slices.clone(),
    );

    let mut left_parser = Parser::new(
        AstBuilder::new(allocator, left_atoms, left_slices),
        generated_parser::START_STATE_MODULE,
    );
    let mut right_parser = Parser::new(
        AstBuilder::new(allocator, right_atoms, right_slices),
        generated_parser::START_STATE_MODULE,
    );

    loop {
        let left_token = left_lexer
            .next(&left_parser)
            .expect("error parsing left string");
        let right_token = right_lexer
            .next(&right_parser)
            .expect("error parsing right string");
        assert_eq!(
            left_token.terminal_id, right_token.terminal_id,
            "at offset {} in {:?} / {} in {:?}",
            left_token.loc.start, left, right_token.loc.start, right,
        );
        assert_eq!(
            left_token.value, right_token.value,
            "at offsets {} / {}",
            left_token.loc.start, right_token.loc.start
        );

        if left_token.terminal_id == TerminalId::End {
            break;
        }
        left_parser.write_token(&left_token).unwrap();
        right_parser.write_token(&right_token).unwrap();
    }
    left_parser.close(left_lexer.offset()).unwrap();
    right_parser.close(left_lexer.offset()).unwrap();
}

fn assert_can_close_after<'alloc, T: IntoChunks<'alloc>>(code: T) {
    let allocator = &Bump::new();
    let buf = chunks_to_string(code);
    let atoms = Rc::new(RefCell::new(SourceAtomSet::new()));
    let slices = Rc::new(RefCell::new(SourceSliceList::new()));
    let mut lexer = Lexer::new(allocator, buf.chars(), atoms.clone(), slices.clone());
    let mut parser = Parser::new(
        AstBuilder::new(allocator, atoms, slices),
        generated_parser::START_STATE_SCRIPT,
    );
    loop {
        let t = lexer.next(&parser).expect("lexer error");
        if t.terminal_id == TerminalId::End {
            break;
        }
        parser.write_token(&t).unwrap();
    }
    assert!(parser.can_close());
}

fn assert_same_number(code: &str, expected: f64) {
    let allocator = &Bump::new();
    let script = try_parse(allocator, code).unwrap().unbox();
    match &script.statements[0] {
        Statement::ExpressionStatement(expression) => match &**expression {
            Expression::LiteralNumericExpression(num) => {
                assert_eq!(num.value, expected, "{}", code);
            }
            _ => panic!("expected LiteralNumericExpression"),
        },
        _ => panic!("expected ExpressionStatement"),
    }
}

#[test]
fn test_asi_at_end() {
    assert_parses("3 + 4");
    assert_syntax_error("3 4");
    assert_incomplete("3 +");
    assert_incomplete("{");
    assert_incomplete("{;");
}

#[test]
fn test_asi_at_block_end() {
    assert_parses("{ doCrimes() }");
    assert_parses("function f() { ok }");
}

#[test]
fn test_asi_after_line_terminator() {
    assert_parses(
        "switch (value) {
             case 1: break
             case 2: console.log('2');
         }",
    );
    assert_syntax_error("switch (value) { case 1: break case 2: console.log('2'); }");

    // "[T]he presence or absence of single-line comments does not affect the
    // process of automatic semicolon insertion[...]."
    // <https://tc39.es/ecma262/#sec-comments>
    assert_parses("x = 1 // line break here\ny = 2");
    assert_parses("x = 1 // line break here\r\ny = 2");
    assert_parses("x = 1 /* no line break in here */ //\ny = 2");
    assert_parses("x = 1<!-- line break here\ny = 2");

    assert_syntax_error("x = 1 /* no line break in here */ y = 2");
    assert_parses("x = 1 /* line break \n there */y = 2");
}

#[test]
fn test_asi_suppressed() {
    // The specification says ASI does not happen in the production
    // EmptyStatement : `;`.
    // TODO - assert_syntax_error("if (true)");
    assert_syntax_error("{ for (;;) }");

    // ASI does not happen in for(;;) loops.
    assert_syntax_error("for ( \n ; ) {}");
    assert_syntax_error("for ( ; \n ) {}");
    assert_syntax_error("for ( \n \n ) {}");
    assert_syntax_error("for (var i = 0 \n i < 9;   i++) {}");
    assert_syntax_error("for (var i = 0;   i < 9 \n i++) {}");
    assert_syntax_error("for (i = 0     \n i < 9;   i++) {}");
    assert_syntax_error("for (i = 0;       i < 9 \n i++) {}");
    assert_syntax_error("for (const i = 0 \n i < 9;   i++) {}");

    // ASI is suppressed in the production ClassElement[Yield, Await] : `;`
    // to prevent an infinite loop of ASI. lol
    assert_syntax_error("class Fail { \n +1; }");
}

#[test]
fn test_if_else() {
    assert_parses("if (x) f();");
    assert_incomplete("if (x)");
    assert_parses("if (x) f(); else g();");
    assert_incomplete("if (x) f(); else");
    assert_parses("if (x) if (y) g(); else h();");
    assert_parses("if (x) if (y) g(); else h(); else j();");
}

#[test]
fn test_lexer_decimal() {
    assert_parses("0.");
    assert_parses(".5");
    assert_syntax_error(".");
}

#[test]
fn test_numbers() {
    assert_same_number("0", 0.0);
    assert_same_number("1", 1.0);
    assert_same_number("10", 10.0);

    assert_error_eq("0a", ParseError::IllegalCharacter('a'));
    assert_error_eq("1a", ParseError::IllegalCharacter('a'));

    assert_error_eq("1.0a", ParseError::IllegalCharacter('a'));
    assert_error_eq(".0a", ParseError::IllegalCharacter('a'));
    assert_error_eq("1.a", ParseError::IllegalCharacter('a'));

    assert_same_number("1.0", 1.0);
    assert_same_number("1.", 1.0);
    assert_same_number("0.", 0.0);

    assert_same_number("1.0e0", 1.0);
    assert_same_number("1.e0", 1.0);
    assert_same_number(".0e0", 0.0);

    assert_same_number("1.0e+0", 1.0);
    assert_same_number("1.e+0", 1.0);
    assert_same_number(".0e+0", 0.0);

    assert_same_number("1.0e-0", 1.0);
    assert_same_number("1.e-0", 1.0);
    assert_same_number(".0e-0", 0.0);

    assert_error_eq("1.0e", ParseError::UnexpectedEnd);
    assert_error_eq("1.e", ParseError::UnexpectedEnd);
    assert_error_eq(".0e", ParseError::UnexpectedEnd);

    assert_error_eq("1.0e+", ParseError::UnexpectedEnd);
    assert_error_eq("1.0e-", ParseError::UnexpectedEnd);
    assert_error_eq(".0e+", ParseError::UnexpectedEnd);
    assert_error_eq(".0e-", ParseError::UnexpectedEnd);

    assert_same_number("1.0E0", 1.0);
    assert_same_number("1.E0", 1.0);
    assert_same_number(".0E0", 0.0);

    assert_same_number("1.0E+0", 1.0);
    assert_same_number("1.E+0", 1.0);
    assert_same_number(".0E+0", 0.0);

    assert_same_number("1.0E-0", 1.0);
    assert_same_number("1.E-0", 1.0);
    assert_same_number(".0E-0", 0.0);

    assert_error_eq("1.0E", ParseError::UnexpectedEnd);
    assert_error_eq("1.E", ParseError::UnexpectedEnd);
    assert_error_eq(".0E", ParseError::UnexpectedEnd);

    assert_error_eq("1.0E+", ParseError::UnexpectedEnd);
    assert_error_eq("1.0E-", ParseError::UnexpectedEnd);
    assert_error_eq(".0E+", ParseError::UnexpectedEnd);
    assert_error_eq(".0E-", ParseError::UnexpectedEnd);

    assert_same_number(".0", 0.0);
    assert_parses("");

    assert_same_number("0b0", 0.0);

    assert_same_number("0b1", 1.0);
    assert_same_number("0B01", 1.0);
    assert_error_eq("0b", ParseError::UnexpectedEnd);
    assert_error_eq("0b ", ParseError::IllegalCharacter(' '));
    assert_error_eq("0b2", ParseError::IllegalCharacter('2'));

    assert_same_number("0o0", 0.0);
    assert_same_number("0o7", 7.0);
    assert_same_number("0O01234567", 0o01234567 as f64);
    assert_error_eq("0o", ParseError::UnexpectedEnd);
    assert_error_eq("0o ", ParseError::IllegalCharacter(' '));
    assert_error_eq("0o8", ParseError::IllegalCharacter('8'));

    assert_same_number("0x0", 0.0);
    assert_same_number("0xf", 15.0);
    assert_not_implemented("0X0123456789abcdef");
    assert_not_implemented("0X0123456789ABCDEF");
    assert_error_eq("0x", ParseError::UnexpectedEnd);
    assert_error_eq("0x ", ParseError::IllegalCharacter(' '));
    assert_error_eq("0xg", ParseError::IllegalCharacter('g'));

    assert_parses("1..x");

    assert_same_number("1_1", 11.0);
    assert_same_number("0b1_1", 3.0);
    assert_same_number("0o1_1", 9.0);
    assert_same_number("0x1_1", 17.0);

    assert_same_number("1_1.1_1", 11.11);
    assert_same_number("1_1.1_1e+1_1", 11.11e11);

    assert_error_eq("1_", ParseError::UnexpectedEnd);
    assert_error_eq("1._1", ParseError::IllegalCharacter('_'));
    assert_error_eq("1.1_", ParseError::UnexpectedEnd);
    assert_error_eq("1.1e1_", ParseError::UnexpectedEnd);
    assert_error_eq("1.1e_1", ParseError::IllegalCharacter('_'));
}

#[test]
fn test_numbers_large() {
    assert_same_number("4294967295", 4294967295.0);
    assert_same_number("4294967296", 4294967296.0);
    assert_same_number("4294967297", 4294967297.0);

    assert_same_number("9007199254740991", 9007199254740991.0);
    assert_same_number("9007199254740992", 9007199254740992.0);
    assert_same_number("9007199254740993", 9007199254740992.0);

    assert_same_number("18446744073709553664", 18446744073709552000.0);
    assert_same_number("18446744073709553665", 18446744073709556000.0);

    assert_same_number("0b11111111111111111111111111111111", 4294967295.0);
    assert_same_number("0b100000000000000000000000000000000", 4294967296.0);
    assert_same_number("0b100000000000000000000000000000001", 4294967297.0);

    assert_same_number(
        "0b11111111111111111111111111111111111111111111111111111",
        9007199254740991.0,
    );
    assert_not_implemented("0b100000000000000000000000000000000000000000000000000000");

    assert_same_number("0o77777777777777777", 2251799813685247.0);
    assert_not_implemented("0o100000000000000000");

    assert_same_number("0xfffffffffffff", 4503599627370495.0);
    assert_not_implemented("0x10000000000000");

    assert_not_implemented("4.9406564584124654417656879286822e-324");
}

#[test]
fn test_bigint() {
    assert_not_implemented("0n");
    /*
        assert_parses("0n");
        assert_parses("1n");
        assert_parses("10n");

        assert_error_eq("0na", ParseError::IllegalCharacter('a'));
        assert_error_eq("1na", ParseError::IllegalCharacter('a'));

        assert_error_eq("1.0n", ParseError::IllegalCharacter('n'));
        assert_error_eq(".0n", ParseError::IllegalCharacter('n'));
        assert_error_eq("1.n", ParseError::IllegalCharacter('n'));

        assert_error_eq("1e0n", ParseError::IllegalCharacter('n'));
        assert_error_eq("1e+0n", ParseError::IllegalCharacter('n'));
        assert_error_eq("1e-0n", ParseError::IllegalCharacter('n'));
        assert_error_eq("1E0n", ParseError::IllegalCharacter('n'));
        assert_error_eq("1E+0n", ParseError::IllegalCharacter('n'));
        assert_error_eq("1E-0n", ParseError::IllegalCharacter('n'));

        assert_parses("0b0n");

        assert_parses("0b1n");
        assert_parses("0B01n");
        assert_error_eq("0bn", ParseError::IllegalCharacter('n'));

        assert_parses("0o0n");
        assert_parses("0o7n");
        assert_parses("0O01234567n");
        assert_error_eq("0on", ParseError::IllegalCharacter('n'));

        assert_parses("0x0n");
        assert_parses("0xfn");
        assert_parses("0X0123456789abcdefn");
        assert_parses("0X0123456789ABCDEFn");
        assert_error_eq("0xn", ParseError::IllegalCharacter('n'));

        assert_parses("1_1n");
        assert_parses("0b1_1n");
        assert_parses("0o1_1n");
        assert_parses("0x1_1n");

        assert_error_eq("1_1.1_1n", ParseError::IllegalCharacter('n'));
        assert_error_eq("1_1.1_1e1_1n", ParseError::IllegalCharacter('n'));

        assert_error_eq("1_n", ParseError::IllegalCharacter('n'));
        assert_error_eq("1.1_n", ParseError::IllegalCharacter('n'));
        assert_error_eq("1.1e1_n", ParseError::IllegalCharacter('n'));
    */
}

#[test]
fn test_arrow() {
    assert_parses("x => x");
    assert_parses("f = x => x;");
    assert_parses("(x, y) => [y, x]");
    assert_parses("f = (x, y) => {}");
    assert_syntax_error("(x, y) => {x: x, y: y}");
}

#[test]
fn test_illegal_character() {
    assert_illegal_character("\0");
    assert_illegal_character("—x;");
    assert_illegal_character("const ONE_THIRD = 1 ÷ 3;");
}

#[test]
fn test_identifier() {
    // U+00B7 MIDDLE DOT is an IdentifierPart.
    assert_parses("_·_ = {_·_:'·_·'};");

    // <ZWJ> and <ZWNJ> match IdentifierPart but not IdentifierStart.
    assert_parses("var x\u{200c};"); // <ZWNJ>
    assert_parses("_\u{200d}();"); // <ZWJ>
    assert_parses("_\u{200d}__();"); // <ZWJ>
    assert_parses("_\u{200d}\u{200c}();"); // <ZWJ>
    assert_illegal_character("var \u{200c};"); // <ZWNJ>
    assert_illegal_character("x = \u{200d};"); // <ZWJ>

    // Other_ID_Start for backward compat.
    assert_parses("\u{309B}();");
    assert_parses("\u{309C}();");
    assert_parses("_\u{309B}();");
    assert_parses("_\u{309C}();");

    // Non-BMP.
    assert_parses("\u{10000}();");
    assert_parses("_\u{10000}();");
    assert_illegal_character("\u{1000c}();");
    assert_illegal_character("_\u{1000c}();");
}

#[test]
fn test_regexp() {
    assert_parses(r"/\w/");
    assert_parses("/[A-Z]/");
    assert_parses("/[//]/");
    assert_parses("/a*a/");
    assert_parses("/**//x*/");
    assert_same_tokens("/**//x*/", "/x*/");
    assert_parses("{} /x/");
    assert_parses("of / 2");
}

#[test]
fn test_html_comments() {
    assert_same_tokens("x<!--y;", "x");
    assert_same_tokens("x<!-y;", "x < ! - y ;");
    assert_same_tokens("x<!y", "x < ! y");

    assert_same_tokens("--> hello world\nok", "ok");
    assert_same_tokens("/* ignore */ --> also ignore\nok", "ok");
    assert_same_tokens("/* ignore *//**/--> also ignore\nok", "ok");
    assert_same_tokens("x-->y\nz", "x -- > y\nz");
}

#[test]
fn test_incomplete_comments() {
    assert_error("/*");
    assert_error("/* hello world");
    assert_error("/* hello world *");

    assert_parses(&vec!["/* hello\n", " world */"]);
    assert_parses(&vec!["// oawfeoiawj", "ioawefoawjie"]);
    assert_parses(&vec!["// oawfeoiawj", "ioawefoawjie\n ok();"]);
    assert_parses(&vec!["// oawfeoiawj", "ioawefoawjie", "jiowaeawojefiw"]);
    assert_parses(&vec![
        "// oawfeoiawj",
        "ioawefoawjie",
        "jiowaeawojefiw\n ok();",
    ]);
}

#[test]
fn test_strings() {
    assert_parses("f(\"\",\"\")");
    assert_parses("f(\"\")");
    assert_parses("(\"\")");
    assert_parses("f('','')");
    assert_parses("f('')");
    assert_parses("('')");
}

#[test]
fn test_awkward_chunks() {
    assert_parses(&vec!["const", "ructor.length = 1;"]);
    assert_parses(&vec!["const", " x = 1;"]);

    // Try feeding one character at a time to the parser.
    let chars: Vec<&str> = "function f() { ok(); }".split("").collect();
    assert_parses(&chars);

    // XXX TODO
    //assertEqual(
    //    self.parse(&vec!["/xyzzy/", "g;"]),
    //    ('Script',
    //     ('ScriptBody',
    //      ('StatementList 0',
    //       ('ExpressionStatement',
    //        ('PrimaryExpression 10', '/xyzzy/g'))))));

    let allocator = &Bump::new();
    let actual = try_parse(allocator, &vec!["x/", "=2;"]).unwrap();
    let atoms = Rc::new(RefCell::new(SourceAtomSet::new()));
    let expected = Script {
        directives: arena::Vec::new_in(allocator),
        statements: bumpalo::vec![
            in allocator;
            Statement::ExpressionStatement(arena::alloc(
                allocator,
                Expression::CompoundAssignmentExpression {
                    operator: CompoundAssignmentOperator::Div {
                        loc: SourceLocation::new(1, 3),
                    },
                    binding: SimpleAssignmentTarget::AssignmentTargetIdentifier(
                        AssignmentTargetIdentifier {
                            name: Identifier {
                                value: atoms.borrow_mut().insert("x"),
                                loc: SourceLocation::new(0, 1),
                            },
                            loc: SourceLocation::new(0, 1),
                        },
                    ),
                    expression: arena::alloc(
                        allocator,
                        Expression::LiteralNumericExpression(NumericLiteral {
                            value: 2.0,
                            loc: SourceLocation::new(3, 4),
                        }),
                    ),
                    loc: SourceLocation::new(0, 4),
                },
            ))
        ],
        loc: SourceLocation::new(0, 4),
    };
    assert_eq!(format!("{:?}", actual), format!("{:?}", expected));
}

#[test]
fn test_can_close() {
    let empty: Vec<&str> = vec![];
    assert_can_close_after(&empty);
    assert_can_close_after("");
    assert_can_close_after("2 + 2;\n");
    assert_can_close_after("// seems ok\n");
}

#[test]
fn test_regex() {
    assert_parses("/x/");
    assert_parses("x = /x/");
    assert_parses("x = /x/g");

    // FIXME: Unexpected flag
    // assert_parses("x = /x/wow_flags_can_be_$$anything$$");
    assert_not_implemented("x = /x/wow_flags_can_be_$$anything$$");

    // TODO: Should the lexer running out of input throw an incomplete error, or a lexer error?
    assert_error_eq("/x", ParseError::UnterminatedRegExp);
    assert_incomplete("x = //"); // comment
    assert_error_eq("x = /*/", ParseError::UnterminatedMultiLineComment); /*/ comment */
    assert_error_eq("x =/= 2", ParseError::UnterminatedRegExp);
    assert_parses("x /= 2");
    assert_parses("x = /[]/");
    assert_parses("x = /[^x]/");
    assert_parses("x = /+=351*/");
    assert_parses("x = /^\\s*function (\\w+)/;");
    assert_parses("const regexp = /this is fine: [/] dont @ me/;");
}

#[test]
fn test_arrow_parameters() {
    assert_error_eq(
        "({a:a, ...b, c:c}) => {}",
        ParseError::ObjectPatternWithNonFinalRest,
    );
    assert_error_eq(
        "(a, [...zero, one]) => {}",
        ParseError::ArrayPatternWithNonFinalRest,
    );
    assert_error_eq(
        "(a, {items: [...zero, one]}) => {}",
        ParseError::ArrayPatternWithNonFinalRest,
    );
}

#[test]
fn test_invalid_assignment_targets() {
    assert_syntax_error("2 + 2 = x;");
    assert_error_eq("(2 + 2) = x;", ParseError::InvalidAssignmentTarget);
    assert_error_eq("++-x;", ParseError::InvalidAssignmentTarget);
    assert_error_eq("(x && y)--;", ParseError::InvalidAssignmentTarget);
}

#[test]
fn test_can_close_with_asi() {
    assert_can_close_after("2 + 2\n");
}

#[test]
fn test_conditional_keywords() {
    // property names
    assert_parses("const obj = {if: 3, function: 4};");
    assert_parses("const obj = {true: 1, false: 0, null: NaN};");
    assert_parses("assert(obj.if == 3);");
    assert_parses("assert(obj.true + obj.false + obj.null == NaN);");

    // method names
    assert_parses(
        "
        class C {
            if() {}
            function() {}
        }
        ",
    );

    // FIXME: let (multitoken lookahead):
    assert_not_implemented("let a = 1;");
    /*
    // let as identifier
    assert_parses("var let = [new Date];");
    // let as keyword, then identifier
    assert_parses("let v = let;");
    // `let .` -> ExpressionStatement
    assert_parses("let.length;");
    // `let [` -> LexicalDeclaration
    assert_syntax_error("let[0].getYear();");
     */

    assert_parses(
        "
        var of = [1, 2, 3];
        for (of of of) console.log(of);  // logs 1, 2, 3
        ",
    );

    // Not implemented:
    // assert_parses("var of, let, private, target;");

    assert_parses("class X { get y() {} }");

    // Not implemented:
    // assert_parses("async: { break async; }");

    assert_parses("var get = { get get() {}, set get(v) {}, set: 3 };");

    // Not implemented (requires hack; grammar is not LR(1)):
    // assert_parses("for (async of => {};;) {}");
    // assert_parses("for (async of []) {}");
}

#[test]
fn test_async_arrows() {
    // FIXME: async (multiple lookahead)
    assert_not_implemented("const a = async a => 1;");
    /*
    assert_parses("let f = async arg => body;");
    assert_parses("f = async (a1, a2) => {};");
    assert_parses("f = async (a1 = b + c, ...a2) => {};");

    assert_error_eq("f = async (a, b + c) => {};", ParseError::InvalidParameter);
    assert_error_eq(
        "f = async (...a1, a2) => {};",
        ParseError::ArrowParametersWithNonFinalRest,
    );
    assert_error_eq("obj.async() => {}", ParseError::ArrowHeadInvalid);
    */

    assert_error_eq("foo(a, b) => {}", ParseError::ArrowHeadInvalid);
}

#[test]
fn test_binary() {
    assert_parses("1 == 2");
    assert_parses("1 != 2");
    assert_parses("1 === 2");
    assert_parses("1 !== 2");
    assert_parses("1 < 2");
    assert_parses("1 <= 2");
    assert_parses("1 > 2");
    assert_parses("1 >= 2");
    assert_parses("1 in 2");
    assert_parses("1 instanceof 2");
    assert_parses("1 << 2");
    assert_parses("1 >> 2");
    assert_parses("1 >>> 2");
    assert_parses("1 + 2");
    assert_parses("1 - 2");
    assert_parses("1 * 2");
    assert_parses("1 / 2");
    assert_parses("1 % 2");
    assert_parses("1 ** 2");
    assert_parses("1 , 2");
    assert_parses("1 || 2");
    assert_parses("1 && 2");
    assert_parses("1 | 2");
    assert_parses("1 ^ 2");
    assert_parses("1 & 2");
}

#[test]
fn test_coalesce() {
    assert_parses("const f = options.prop ?? 0;");
    assert_syntax_error("if (options.prop ?? 0 || options.prop > 1000) {}");
}

#[test]
fn test_no_line_terminator_here() {
    // Parse `code` as a Script and compute some function of the resulting AST.
    fn parse_then<F, R>(code: &str, f: F) -> R
    where
        F: FnOnce(&Script) -> R,
    {
        let allocator = &Bump::new();
        match try_parse(allocator, code) {
            Err(err) => {
                panic!("Failed to parse code {:?}: {}", code, err);
            }
            Ok(script) => f(&*script),
        }
    }

    // Parse `code` as a Script and return the number of top-level
    // StatementListItems.
    fn count_items(code: &str) -> usize {
        parse_then(code, |script| script.statements.len())
    }

    // Without a newline, labelled `break` in loop.  But a line break changes
    // the meaning -- then it's a plain `break` statement, followed by
    // ExpressionStatement `LOOP;`
    assert_eq!(count_items("LOOP: while (true) break LOOP;"), 1);
    assert_eq!(count_items("LOOP: while (true) break \n LOOP;"), 2);

    // The same, but for `continue`.
    assert_eq!(count_items("LOOP: while (true) continue LOOP;"), 1);
    assert_eq!(count_items("LOOP: while (true) continue \n LOOP;"), 2);

    // Parse `code` as a Script, expected to contain a single function
    // declaration, and return the number of statements in the function body.
    fn count_statements_in_function(code: &str) -> usize {
        parse_then(code, |script| {
            assert_eq!(
                script.statements.len(),
                1,
                "expected function declaration, got {:?}",
                script
            );
            match &script.statements[0] {
                Statement::FunctionDeclaration(func) => func.body.statements.len(),
                _ => panic!("expected function declaration, got {:?}", script),
            }
        })
    }

    assert_eq!(
        count_statements_in_function("function f() { return x; }"),
        1
    );
    assert_eq!(
        count_statements_in_function("function f() { return\n x; }"),
        2
    );

    assert_parses("x++");
    assert_incomplete("x\n++");

    assert_parses("throw fit;");
    assert_syntax_error("throw\nfit;");

    // Alternative ways of spelling LineTerminator
    assert_syntax_error("throw//\nfit;");
    assert_syntax_error("throw/*\n*/fit;");
    assert_syntax_error("throw\rfit;");
    assert_syntax_error("throw\r\nfit;");
}
