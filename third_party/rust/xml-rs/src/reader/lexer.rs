//! Contains simple lexer for XML documents.
//!
//! This module is for internal use. Use `xml::pull` module to do parsing.

use std::fmt;
use std::collections::VecDeque;
use std::io::Read;
use std::result;
use std::borrow::Cow;

use common::{Position, TextPosition, is_whitespace_char, is_name_char};
use reader::Error;
use util;

/// `Token` represents a single lexeme of an XML document. These lexemes
/// are used to perform actual parsing.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Token {
    /// `<?`
    ProcessingInstructionStart,
    /// `?>`
    ProcessingInstructionEnd,
    /// `<!DOCTYPE
    DoctypeStart,
    /// `<`
    OpeningTagStart,
    /// `</`
    ClosingTagStart,
    /// `>`
    TagEnd,
    /// `/>`
    EmptyTagEnd,
    /// `<!--`
    CommentStart,
    /// `-->`
    CommentEnd,
    /// A chunk of characters, used for errors recovery.
    Chunk(&'static str),
    /// Any non-special character except whitespace.
    Character(char),
    /// Whitespace character.
    Whitespace(char),
    /// `=`
    EqualsSign,
    /// `'`
    SingleQuote,
    /// `"`
    DoubleQuote,
    /// `<![CDATA[`
    CDataStart,
    /// `]]>`
    CDataEnd,
    /// `&`
    ReferenceStart,
    /// `;`
    ReferenceEnd,
}

impl fmt::Display for Token {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Token::Chunk(s)                            => write!(f, "{}", s),
            Token::Character(c) | Token::Whitespace(c) => write!(f, "{}", c),
            other => write!(f, "{}", match other {
                Token::OpeningTagStart            => "<",
                Token::ProcessingInstructionStart => "<?",
                Token::DoctypeStart               => "<!DOCTYPE",
                Token::ClosingTagStart            => "</",
                Token::CommentStart               => "<!--",
                Token::CDataStart                 => "<![CDATA[",
                Token::TagEnd                     => ">",
                Token::EmptyTagEnd                => "/>",
                Token::ProcessingInstructionEnd   => "?>",
                Token::CommentEnd                 => "-->",
                Token::CDataEnd                   => "]]>",
                Token::ReferenceStart             => "&",
                Token::ReferenceEnd               => ";",
                Token::EqualsSign                 => "=",
                Token::SingleQuote                => "'",
                Token::DoubleQuote                => "\"",
                _                          => unreachable!()
            })
        }
    }
}

impl Token {
    pub fn as_static_str(&self) -> Option<&'static str> {
        match *self {
            Token::OpeningTagStart            => Some("<"),
            Token::ProcessingInstructionStart => Some("<?"),
            Token::DoctypeStart               => Some("<!DOCTYPE"),
            Token::ClosingTagStart            => Some("</"),
            Token::CommentStart               => Some("<!--"),
            Token::CDataStart                 => Some("<![CDATA["),
            Token::TagEnd                     => Some(">"),
            Token::EmptyTagEnd                => Some("/>"),
            Token::ProcessingInstructionEnd   => Some("?>"),
            Token::CommentEnd                 => Some("-->"),
            Token::CDataEnd                   => Some("]]>"),
            Token::ReferenceStart             => Some("&"),
            Token::ReferenceEnd               => Some(";"),
            Token::EqualsSign                 => Some("="),
            Token::SingleQuote                => Some("'"),
            Token::DoubleQuote                => Some("\""),
            Token::Chunk(s)                   => Some(s),
            _                                 => None
        }
    }

    // using String.push_str(token.to_string()) is simply way too slow
    pub fn push_to_string(&self, target: &mut String) {
        match self.as_static_str() {
            Some(s) => { target.push_str(s); }
            None => {
                match *self {
                    Token::Character(c) | Token::Whitespace(c) => target.push(c),
                    _ => unreachable!()
                }
            }
        }
    }

    /// Returns `true` if this token contains data that can be interpreted
    /// as a part of the text. Surprisingly, this also means '>' and '=' and '"' and "'" and '-->'.
    #[inline]
    pub fn contains_char_data(&self) -> bool {
        match *self {
            Token::Whitespace(_) | Token::Chunk(_) | Token::Character(_) | Token::CommentEnd |
            Token::TagEnd | Token::EqualsSign | Token::DoubleQuote | Token::SingleQuote => true,
            _ => false
        }
    }

    /// Returns `true` if this token corresponds to a white space character.
    #[inline]
    pub fn is_whitespace(&self) -> bool {
        match *self {
            Token::Whitespace(_) => true,
            _ => false
        }
    }
}

enum State {
    /// Triggered on '<'
    TagStarted,
    /// Triggered on '<!'
    CommentOrCDataOrDoctypeStarted,
    /// Triggered on '<!-'
    CommentStarted,
    /// Triggered on '<!D' up to '<!DOCTYPE'
    DoctypeStarted(DoctypeStartedSubstate),
    /// Triggered on '<![' up to '<![CDATA'
    CDataStarted(CDataStartedSubstate),
    /// Triggered on '?'
    ProcessingInstructionClosing,
    /// Triggered on '/'
    EmptyTagClosing,
    /// Triggered on '-' up to '--'
    CommentClosing(ClosingSubstate),
    /// Triggered on ']' up to ']]'
    CDataClosing(ClosingSubstate),
    /// Default state
    Normal
}

#[derive(Copy, Clone)]
enum ClosingSubstate {
    First, Second
}

#[derive(Copy, Clone)]
enum DoctypeStartedSubstate {
    D, DO, DOC, DOCT, DOCTY, DOCTYP
}

#[derive(Copy, Clone)]
enum CDataStartedSubstate {
    E, C, CD, CDA, CDAT, CDATA
}

/// `Result` represents lexing result. It is either a token or an error message.
pub type Result = result::Result<Option<Token>, Error>;

/// Helps to set up a dispatch table for lexing large unambigous tokens like
/// `<![CDATA[` or `<!DOCTYPE `.
macro_rules! dispatch_on_enum_state(
    ($_self:ident, $s:expr, $c:expr, $is:expr,
     $($st:ident; $stc:expr ; $next_st:ident ; $chunk:expr),+;
     $end_st:ident ; $end_c:expr ; $end_chunk:expr ; $e:expr) => (
        match $s {
            $(
            $st => match $c {
                $stc => $_self.move_to($is($next_st)),
                _  => $_self.handle_error($chunk, $c)
            },
            )+
            $end_st => match $c {
                $end_c => $e,
                _      => $_self.handle_error($end_chunk, $c)
            }
        }
    )
);

/// `Lexer` is a lexer for XML documents, which implements pull API.
///
/// Main method is `next_token` which accepts an `std::io::Read` instance and
/// tries to read the next lexeme from it.
///
/// When `skip_errors` flag is set, invalid lexemes will be returned as `Chunk`s.
/// When it is not set, errors will be reported as `Err` objects with a string message.
/// By default this flag is not set. Use `enable_errors` and `disable_errors` methods
/// to toggle the behavior.
pub struct Lexer {
    pos: TextPosition,
    head_pos: TextPosition,
    char_queue: VecDeque<char>,
    st: State,
    skip_errors: bool,
    inside_comment: bool,
    inside_token: bool,
    eof_handled: bool
}

impl Position for Lexer {
    #[inline]
    /// Returns the position of the last token produced by the lexer
    fn position(&self) -> TextPosition { self.pos }
}

impl Lexer {
    /// Returns a new lexer with default state.
    pub fn new() -> Lexer {
        Lexer {
            pos: TextPosition::new(),
            head_pos: TextPosition::new(),
            char_queue: VecDeque::with_capacity(4),  // TODO: check size
            st: State::Normal,
            skip_errors: false,
            inside_comment: false,
            inside_token: false,
            eof_handled: false
        }
    }

    /// Enables error handling so `next_token` will return `Some(Err(..))`
    /// upon invalid lexeme.
    #[inline]
    pub fn enable_errors(&mut self) { self.skip_errors = false; }

    /// Disables error handling so `next_token` will return `Some(Chunk(..))`
    /// upon invalid lexeme with this lexeme content.
    #[inline]
    pub fn disable_errors(&mut self) { self.skip_errors = true; }

    /// Enables special handling of some lexemes which should be done when we're parsing comment
    /// internals.
    #[inline]
    pub fn inside_comment(&mut self) { self.inside_comment = true; }

    /// Disables the effect of `inside_comment()` method.
    #[inline]
    pub fn outside_comment(&mut self) { self.inside_comment = false; }

    /// Tries to read the next token from the buffer.
    ///
    /// It is possible to pass different instaces of `BufReader` each time
    /// this method is called, but the resulting behavior is undefined in this case.
    ///
    /// Return value:
    /// * `Err(reason) where reason: reader::Error` - when an error occurs;
    /// * `Ok(None)` - upon end of stream is reached;
    /// * `Ok(Some(token)) where token: Token` - in case a complete-token has been read from the stream.
    pub fn next_token<B: Read>(&mut self, b: &mut B) -> Result {
        // Already reached end of buffer
        if self.eof_handled {
            return Ok(None);
        }

        if !self.inside_token {
            self.pos = self.head_pos;
            self.inside_token = true;
        }

        // Check if we have saved a char or two for ourselves
        while let Some(c) = self.char_queue.pop_front() {
            match try!(self.read_next_token(c)) {
                Some(t) => {
                    self.inside_token = false;
                    return Ok(Some(t));
                }
                None => {}  // continue
            }
        }

        loop {
            // TODO: this should handle multiple encodings
            let c = match try!(util::next_char_from(b)) {
                Some(c) => c,   // got next char
                None => break,  // nothing to read left
            };

            match try!(self.read_next_token(c)) {
                Some(t) => {
                    self.inside_token = false;
                    return Ok(Some(t));
                }
                None => {
                    // continue
                }
            }
        }

        // Handle end of stream
        self.eof_handled = true;
        self.pos = self.head_pos;
        match self.st {
            State::TagStarted | State::CommentOrCDataOrDoctypeStarted |
            State::CommentStarted | State::CDataStarted(_)| State::DoctypeStarted(_) |
            State::CommentClosing(ClosingSubstate::Second)  =>
                Err(self.error("Unexpected end of stream")),
            State::ProcessingInstructionClosing =>
                Ok(Some(Token::Character('?'))),
            State::EmptyTagClosing =>
                Ok(Some(Token::Character('/'))),
            State::CommentClosing(ClosingSubstate::First) =>
                Ok(Some(Token::Character('-'))),
            State::CDataClosing(ClosingSubstate::First) =>
                Ok(Some(Token::Character(']'))),
            State::CDataClosing(ClosingSubstate::Second) =>
                Ok(Some(Token::Chunk("]]"))),
            State::Normal =>
                Ok(None)
        }
    }

    #[inline]
    fn error<M: Into<Cow<'static, str>>>(&self, msg: M) -> Error {
        (self, msg).into()
    }

    #[inline]
    fn read_next_token(&mut self, c: char) -> Result {
        let res = self.dispatch_char(c);
        if self.char_queue.is_empty() {
            if c == '\n' {
                self.head_pos.new_line();
            } else {
                self.head_pos.advance(1);
            }
        }
        res
    }

    fn dispatch_char(&mut self, c: char) -> Result {
        match self.st {
            State::Normal                         => self.normal(c),
            State::TagStarted                     => self.tag_opened(c),
            State::CommentOrCDataOrDoctypeStarted => self.comment_or_cdata_or_doctype_started(c),
            State::CommentStarted                 => self.comment_started(c),
            State::CDataStarted(s)                => self.cdata_started(c, s),
            State::DoctypeStarted(s)              => self.doctype_started(c, s),
            State::ProcessingInstructionClosing   => self.processing_instruction_closing(c),
            State::EmptyTagClosing                => self.empty_element_closing(c),
            State::CommentClosing(s)              => self.comment_closing(c, s),
            State::CDataClosing(s)                => self.cdata_closing(c, s)
        }
    }

    #[inline]
    fn move_to(&mut self, st: State) -> Result {
        self.st = st;
        Ok(None)
    }

    #[inline]
    fn move_to_with(&mut self, st: State, token: Token) -> Result {
        self.st = st;
        Ok(Some(token))
    }

    #[inline]
    fn move_to_with_unread(&mut self, st: State, cs: &[char], token: Token) -> Result {
        self.char_queue.extend(cs.iter().cloned());
        self.move_to_with(st, token)
    }

    fn handle_error(&mut self, chunk: &'static str, c: char) -> Result {
        self.char_queue.push_back(c);
        if self.skip_errors || (self.inside_comment && chunk != "--") {  // FIXME: looks hacky
            self.move_to_with(State::Normal, Token::Chunk(chunk))
        } else {
            Err(self.error(format!("Unexpected token '{}' before '{}'", chunk, c)))
        }
    }

    /// Encountered a char
    fn normal(&mut self, c: char) -> Result {
        match c {
            '<'                        => self.move_to(State::TagStarted),
            '>'                        => Ok(Some(Token::TagEnd)),
            '/'                        => self.move_to(State::EmptyTagClosing),
            '='                        => Ok(Some(Token::EqualsSign)),
            '"'                        => Ok(Some(Token::DoubleQuote)),
            '\''                       => Ok(Some(Token::SingleQuote)),
            '?'                        => self.move_to(State::ProcessingInstructionClosing),
            '-'                        => self.move_to(State::CommentClosing(ClosingSubstate::First)),
            ']'                        => self.move_to(State::CDataClosing(ClosingSubstate::First)),
            '&'                        => Ok(Some(Token::ReferenceStart)),
            ';'                        => Ok(Some(Token::ReferenceEnd)),
            _ if is_whitespace_char(c) => Ok(Some(Token::Whitespace(c))),
            _                          => Ok(Some(Token::Character(c)))
        }
    }

    /// Encountered '<'
    fn tag_opened(&mut self, c: char) -> Result {
        match c {
            '?'                        => self.move_to_with(State::Normal, Token::ProcessingInstructionStart),
            '/'                        => self.move_to_with(State::Normal, Token::ClosingTagStart),
            '!'                        => self.move_to(State::CommentOrCDataOrDoctypeStarted),
            _ if is_whitespace_char(c) => self.move_to_with_unread(State::Normal, &[c], Token::OpeningTagStart),
            _ if is_name_char(c)       => self.move_to_with_unread(State::Normal, &[c], Token::OpeningTagStart),
            _                          => self.handle_error("<", c)
        }
    }

    /// Encountered '<!'
    fn comment_or_cdata_or_doctype_started(&mut self, c: char) -> Result {
        match c {
            '-' => self.move_to(State::CommentStarted),
            '[' => self.move_to(State::CDataStarted(CDataStartedSubstate::E)),
            'D' => self.move_to(State::DoctypeStarted(DoctypeStartedSubstate::D)),
            _   => self.handle_error("<!", c)
        }
    }

    /// Encountered '<!-'
    fn comment_started(&mut self, c: char) -> Result {
        match c {
            '-' => self.move_to_with(State::Normal, Token::CommentStart),
            _   => self.handle_error("<!-", c)
        }
    }

    /// Encountered '<!['
    fn cdata_started(&mut self, c: char, s: CDataStartedSubstate) -> Result {
        use self::CDataStartedSubstate::{E, C, CD, CDA, CDAT, CDATA};
        dispatch_on_enum_state!(self, s, c, State::CDataStarted,
            E     ; 'C' ; C     ; "<![",
            C     ; 'D' ; CD    ; "<![C",
            CD    ; 'A' ; CDA   ; "<![CD",
            CDA   ; 'T' ; CDAT  ; "<![CDA",
            CDAT  ; 'A' ; CDATA ; "<![CDAT";
            CDATA ; '[' ; "<![CDATA" ; self.move_to_with(State::Normal, Token::CDataStart)
        )
    }

    /// Encountered '<!D'
    fn doctype_started(&mut self, c: char, s: DoctypeStartedSubstate) -> Result {
        use self::DoctypeStartedSubstate::{D, DO, DOC, DOCT, DOCTY, DOCTYP};
        dispatch_on_enum_state!(self, s, c, State::DoctypeStarted,
            D      ; 'O' ; DO     ; "<!D",
            DO     ; 'C' ; DOC    ; "<!DO",
            DOC    ; 'T' ; DOCT   ; "<!DOC",
            DOCT   ; 'Y' ; DOCTY  ; "<!DOCT",
            DOCTY  ; 'P' ; DOCTYP ; "<!DOCTY";
            DOCTYP ; 'E' ; "<!DOCTYP" ; self.move_to_with(State::Normal, Token::DoctypeStart)
        )
    }

    /// Encountered '?'
    fn processing_instruction_closing(&mut self, c: char) -> Result {
        match c {
            '>' => self.move_to_with(State::Normal, Token::ProcessingInstructionEnd),
            _   => self.move_to_with_unread(State::Normal, &[c], Token::Character('?')),
        }
    }

    /// Encountered '/'
    fn empty_element_closing(&mut self, c: char) -> Result {
        match c {
            '>' => self.move_to_with(State::Normal, Token::EmptyTagEnd),
            _   => self.move_to_with_unread(State::Normal, &[c], Token::Character('/')),
        }
    }

    /// Encountered '-'
    fn comment_closing(&mut self, c: char, s: ClosingSubstate) -> Result {
        match s {
            ClosingSubstate::First => match c {
                '-' => self.move_to(State::CommentClosing(ClosingSubstate::Second)),
                _   => self.move_to_with_unread(State::Normal, &[c], Token::Character('-'))
            },
            ClosingSubstate::Second => match c {
                '>'                      => self.move_to_with(State::Normal, Token::CommentEnd),
                // double dash not followed by a greater-than is a hard error inside comment
                _ if self.inside_comment => self.handle_error("--", c),
                // nothing else except comment closing starts with a double dash, and comment
                // closing can never be after another dash, and also we're outside of a comment,
                // therefore it is safe to push only the last read character to the list of unread
                // characters and pass the double dash directly to the output
                _                        => self.move_to_with_unread(State::Normal, &[c], Token::Chunk("--"))
            }
        }
    }

    /// Encountered ']'
    fn cdata_closing(&mut self, c: char, s: ClosingSubstate) -> Result {
        match s {
            ClosingSubstate::First => match c {
                ']' => self.move_to(State::CDataClosing(ClosingSubstate::Second)),
                _   => self.move_to_with_unread(State::Normal, &[c], Token::Character(']'))
            },
            ClosingSubstate::Second => match c {
                '>' => self.move_to_with(State::Normal, Token::CDataEnd),
                _   => self.move_to_with_unread(State::Normal, &[']', c], Token::Character(']'))
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use common::{Position};
    use std::io::{BufReader, Cursor};

    use super::{Lexer, Token};

    macro_rules! assert_oks(
        (for $lex:ident and $buf:ident ; $($e:expr)+) => ({
            $(
                assert_eq!(Ok(Some($e)), $lex.next_token(&mut $buf));
             )+
        })
    );

    macro_rules! assert_err(
        (for $lex:ident and $buf:ident expect row $r:expr ; $c:expr, $s:expr) => ({
            let err = $lex.next_token(&mut $buf);
            assert!(err.is_err());
            let err = err.unwrap_err();
            assert_eq!($r as u64, err.position().row);
            assert_eq!($c as u64, err.position().column);
            assert_eq!($s, err.msg());
        })
    );

    macro_rules! assert_none(
        (for $lex:ident and $buf:ident) => (
            assert_eq!(Ok(None), $lex.next_token(&mut $buf));
        )
    );

    fn make_lex_and_buf(s: &str) -> (Lexer, BufReader<Cursor<Vec<u8>>>) {
        (Lexer::new(), BufReader::new(Cursor::new(s.to_owned().into_bytes())))
    }

    #[test]
    fn simple_lexer_test() {
        let (mut lex, mut buf) = make_lex_and_buf(
            r#"<a p='q'> x<b z="y">d	</b></a><p/> <?nm ?> <!-- a c --> &nbsp;"#
        );

        assert_oks!(for lex and buf ;
            Token::OpeningTagStart
            Token::Character('a')
            Token::Whitespace(' ')
            Token::Character('p')
            Token::EqualsSign
            Token::SingleQuote
            Token::Character('q')
            Token::SingleQuote
            Token::TagEnd
            Token::Whitespace(' ')
            Token::Character('x')
            Token::OpeningTagStart
            Token::Character('b')
            Token::Whitespace(' ')
            Token::Character('z')
            Token::EqualsSign
            Token::DoubleQuote
            Token::Character('y')
            Token::DoubleQuote
            Token::TagEnd
            Token::Character('d')
            Token::Whitespace('\t')
            Token::ClosingTagStart
            Token::Character('b')
            Token::TagEnd
            Token::ClosingTagStart
            Token::Character('a')
            Token::TagEnd
            Token::OpeningTagStart
            Token::Character('p')
            Token::EmptyTagEnd
            Token::Whitespace(' ')
            Token::ProcessingInstructionStart
            Token::Character('n')
            Token::Character('m')
            Token::Whitespace(' ')
            Token::ProcessingInstructionEnd
            Token::Whitespace(' ')
            Token::CommentStart
            Token::Whitespace(' ')
            Token::Character('a')
            Token::Whitespace(' ')
            Token::Character('c')
            Token::Whitespace(' ')
            Token::CommentEnd
            Token::Whitespace(' ')
            Token::ReferenceStart
            Token::Character('n')
            Token::Character('b')
            Token::Character('s')
            Token::Character('p')
            Token::ReferenceEnd
        );
        assert_none!(for lex and buf);
    }

    #[test]
    fn special_chars_test() {
        let (mut lex, mut buf) = make_lex_and_buf(
            r#"?x!+ // -| ]z]]"#
        );

        assert_oks!(for lex and buf ;
            Token::Character('?')
            Token::Character('x')
            Token::Character('!')
            Token::Character('+')
            Token::Whitespace(' ')
            Token::Character('/')
            Token::Character('/')
            Token::Whitespace(' ')
            Token::Character('-')
            Token::Character('|')
            Token::Whitespace(' ')
            Token::Character(']')
            Token::Character('z')
            Token::Chunk("]]")
        );
        assert_none!(for lex and buf);
    }

    #[test]
    fn cdata_test() {
        let (mut lex, mut buf) = make_lex_and_buf(
            r#"<a><![CDATA[x y ?]]> </a>"#
        );

        assert_oks!(for lex and buf ;
            Token::OpeningTagStart
            Token::Character('a')
            Token::TagEnd
            Token::CDataStart
            Token::Character('x')
            Token::Whitespace(' ')
            Token::Character('y')
            Token::Whitespace(' ')
            Token::Character('?')
            Token::CDataEnd
            Token::Whitespace(' ')
            Token::ClosingTagStart
            Token::Character('a')
            Token::TagEnd
        );
        assert_none!(for lex and buf);
    }

    #[test]
    fn doctype_test() {
        let (mut lex, mut buf) = make_lex_and_buf(
            r#"<a><!DOCTYPE ab xx z> "#
        );
        assert_oks!(for lex and buf ;
            Token::OpeningTagStart
            Token::Character('a')
            Token::TagEnd
            Token::DoctypeStart
            Token::Whitespace(' ')
            Token::Character('a')
            Token::Character('b')
            Token::Whitespace(' ')
            Token::Character('x')
            Token::Character('x')
            Token::Whitespace(' ')
            Token::Character('z')
            Token::TagEnd
            Token::Whitespace(' ')
        );
        assert_none!(for lex and buf)
    }

    #[test]
    fn end_of_stream_handling_ok() {
        macro_rules! eof_check(
            ($data:expr ; $token:expr) => ({
                let (mut lex, mut buf) = make_lex_and_buf($data);
                assert_oks!(for lex and buf ; $token);
                assert_none!(for lex and buf);
            })
        );
        eof_check!("?"  ; Token::Character('?'));
        eof_check!("/"  ; Token::Character('/'));
        eof_check!("-"  ; Token::Character('-'));
        eof_check!("]"  ; Token::Character(']'));
        eof_check!("]]" ; Token::Chunk("]]"));
    }

    #[test]
    fn end_of_stream_handling_error() {
        macro_rules! eof_check(
            ($data:expr; $r:expr, $c:expr) => ({
                let (mut lex, mut buf) = make_lex_and_buf($data);
                assert_err!(for lex and buf expect row $r ; $c, "Unexpected end of stream");
                assert_none!(for lex and buf);
            })
        );
        eof_check!("<"        ; 0, 1);
        eof_check!("<!"       ; 0, 2);
        eof_check!("<!-"      ; 0, 3);
        eof_check!("<!["      ; 0, 3);
        eof_check!("<![C"     ; 0, 4);
        eof_check!("<![CD"    ; 0, 5);
        eof_check!("<![CDA"   ; 0, 6);
        eof_check!("<![CDAT"  ; 0, 7);
        eof_check!("<![CDATA" ; 0, 8);
        eof_check!("--"       ; 0, 2);
    }

    #[test]
    fn error_in_comment_or_cdata_prefix() {
        let (mut lex, mut buf) = make_lex_and_buf("<!x");
        assert_err!(for lex and buf expect row 0 ; 0,
            "Unexpected token '<!' before 'x'"
        );

        let (mut lex, mut buf) = make_lex_and_buf("<!x");
        lex.disable_errors();
        assert_oks!(for lex and buf ;
            Token::Chunk("<!")
            Token::Character('x')
        );
        assert_none!(for lex and buf);
    }

    #[test]
    fn error_in_comment_started() {
        let (mut lex, mut buf) = make_lex_and_buf("<!-\t");
        assert_err!(for lex and buf expect row 0 ; 0,
            "Unexpected token '<!-' before '\t'"
        );

        let (mut lex, mut buf) = make_lex_and_buf("<!-\t");
        lex.disable_errors();
        assert_oks!(for lex and buf ;
            Token::Chunk("<!-")
            Token::Whitespace('\t')
        );
        assert_none!(for lex and buf);
    }

    #[test]
    fn error_in_comment_two_dashes_not_at_end() {
        let (mut lex, mut buf) = make_lex_and_buf("--x");
        lex.inside_comment();
        assert_err!(for lex and buf expect row 0; 0,
            "Unexpected token '--' before 'x'"
        );

        let (mut lex, mut buf) = make_lex_and_buf("--x");
        assert_oks!(for lex and buf ;
            Token::Chunk("--")
            Token::Character('x')
        );
    }

    macro_rules! check_case(
        ($chunk:expr, $app:expr; $data:expr; $r:expr, $c:expr, $s:expr) => ({
            let (mut lex, mut buf) = make_lex_and_buf($data);
            assert_err!(for lex and buf expect row $r ; $c, $s);

            let (mut lex, mut buf) = make_lex_and_buf($data);
            lex.disable_errors();
            assert_oks!(for lex and buf ;
                Token::Chunk($chunk)
                Token::Character($app)
            );
            assert_none!(for lex and buf);
        })
    );

    #[test]
    fn error_in_cdata_started() {
        check_case!("<![",      '['; "<![["      ; 0, 0, "Unexpected token '<![' before '['");
        check_case!("<![C",     '['; "<![C["     ; 0, 0, "Unexpected token '<![C' before '['");
        check_case!("<![CD",    '['; "<![CD["    ; 0, 0, "Unexpected token '<![CD' before '['");
        check_case!("<![CDA",   '['; "<![CDA["   ; 0, 0, "Unexpected token '<![CDA' before '['");
        check_case!("<![CDAT",  '['; "<![CDAT["  ; 0, 0, "Unexpected token '<![CDAT' before '['");
        check_case!("<![CDATA", '|'; "<![CDATA|" ; 0, 0, "Unexpected token '<![CDATA' before '|'");
    }

    #[test]
    fn error_in_doctype_started() {
        check_case!("<!D",      'a'; "<!Da"      ; 0, 0, "Unexpected token '<!D' before 'a'");
        check_case!("<!DO",     'b'; "<!DOb"     ; 0, 0, "Unexpected token '<!DO' before 'b'");
        check_case!("<!DOC",    'c'; "<!DOCc"    ; 0, 0, "Unexpected token '<!DOC' before 'c'");
        check_case!("<!DOCT",   'd'; "<!DOCTd"   ; 0, 0, "Unexpected token '<!DOCT' before 'd'");
        check_case!("<!DOCTY",  'e'; "<!DOCTYe"  ; 0, 0, "Unexpected token '<!DOCTY' before 'e'");
        check_case!("<!DOCTYP", 'f'; "<!DOCTYPf" ; 0, 0, "Unexpected token '<!DOCTYP' before 'f'");
    }



    #[test]
    fn issue_98_cdata_ending_with_right_bracket() {
        let (mut lex, mut buf) = make_lex_and_buf(
            r#"<![CDATA[Foo [Bar]]]>"#
        );

        assert_oks!(for lex and buf ;
            Token::CDataStart
            Token::Character('F')
            Token::Character('o')
            Token::Character('o')
            Token::Whitespace(' ')
            Token::Character('[')
            Token::Character('B')
            Token::Character('a')
            Token::Character('r')
            Token::Character(']')
            Token::CDataEnd
        );
        assert_none!(for lex and buf);
    }
}
