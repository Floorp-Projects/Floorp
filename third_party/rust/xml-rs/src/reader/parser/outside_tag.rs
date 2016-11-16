use common::is_whitespace_char;

use reader::events::XmlEvent;
use reader::lexer::Token;

use super::{
    Result, PullParser, State, ClosingTagSubstate, OpeningTagSubstate,
    ProcessingInstructionSubstate, DEFAULT_VERSION, DEFAULT_ENCODING, DEFAULT_STANDALONE
};

impl PullParser {
    pub fn outside_tag(&mut self, t: Token) -> Option<Result> {
        match t {
            Token::ReferenceStart =>
                self.into_state_continue(State::InsideReference(Box::new(State::OutsideTag))),

            Token::Whitespace(_) if self.depth() == 0 => None,  // skip whitespace outside of the root element

            _ if t.contains_char_data() && self.depth() == 0 =>
                Some(self_error!(self; "Unexpected characters outside the root element: {}", t)),

            Token::Whitespace(_) if self.config.trim_whitespace && !self.buf_has_data() => None,

            Token::Whitespace(c) => {
                if !self.buf_has_data() {
                    self.push_pos();
                }
                self.append_char_continue(c)
            }

            _ if t.contains_char_data() => {  // Non-whitespace char data
                if !self.buf_has_data() {
                    self.push_pos();
                }
                self.inside_whitespace = false;
                t.push_to_string(&mut self.buf);
                None
            }

            Token::ReferenceEnd => { // Semi-colon in a text outside an entity
                self.inside_whitespace = false;
                Token::ReferenceEnd.push_to_string(&mut self.buf);
                None
            }

            Token::CommentStart if self.config.coalesce_characters && self.config.ignore_comments => {
                // We need to switch the lexer into a comment mode inside comments
                self.lexer.inside_comment();
                self.into_state_continue(State::InsideComment)
            }

            Token::CDataStart if self.config.coalesce_characters && self.config.cdata_to_characters => {
                if !self.buf_has_data() {
                    self.push_pos();
                }
                // We need to disable lexing errors inside CDATA
                self.lexer.disable_errors();
                self.into_state_continue(State::InsideCData)
            }

            _ => {
                // Encountered some markup event, flush the buffer as characters
                // or a whitespace
                let mut next_event = if self.buf_has_data() {
                    let buf = self.take_buf();
                    if self.inside_whitespace && self.config.trim_whitespace {
                        None
                    } else if self.inside_whitespace && !self.config.whitespace_to_characters {
                        Some(Ok(XmlEvent::Whitespace(buf)))
                    } else if self.config.trim_whitespace {
                        Some(Ok(XmlEvent::Characters(buf.trim_matches(is_whitespace_char).into())))
                    } else {
                        Some(Ok(XmlEvent::Characters(buf)))
                    }
                } else { None };
                self.inside_whitespace = true;  // Reset inside_whitespace flag
                self.push_pos();
                match t {
                    Token::ProcessingInstructionStart =>
                        self.into_state(State::InsideProcessingInstruction(ProcessingInstructionSubstate::PIInsideName), next_event),

                    Token::DoctypeStart if !self.encountered_element => {
                        // We don't have a doctype event so skip this position
                        // FIXME: update when we have a doctype event
                        self.next_pos();
                        self.lexer.disable_errors();
                        self.into_state(State::InsideDoctype, next_event)
                    }

                    Token::OpeningTagStart => {
                        // If declaration was not parsed and we have encountered an element,
                        // emit this declaration as the next event.
                        if !self.parsed_declaration {
                            self.parsed_declaration = true;
                            let sd_event = XmlEvent::StartDocument {
                                version: DEFAULT_VERSION,
                                encoding: DEFAULT_ENCODING.into(),
                                standalone: DEFAULT_STANDALONE
                            };
                            // next_event is always none here because we're outside of
                            // the root element
                            next_event = Some(Ok(sd_event));
                            self.push_pos();
                        }
                        self.encountered_element = true;
                        self.nst.push_empty();
                        self.into_state(State::InsideOpeningTag(OpeningTagSubstate::InsideName), next_event)
                    }

                    Token::ClosingTagStart if self.depth() > 0 =>
                        self.into_state(State::InsideClosingTag(ClosingTagSubstate::CTInsideName), next_event),

                    Token::CommentStart => {
                        // We need to switch the lexer into a comment mode inside comments
                        self.lexer.inside_comment();
                        self.into_state(State::InsideComment, next_event)
                    }

                    Token::CDataStart => {
                        // We need to disable lexing errors inside CDATA
                        self.lexer.disable_errors();
                        self.into_state(State::InsideCData, next_event)
                    }

                    _ => Some(self_error!(self; "Unexpected token: {}", t))
                }
            }
        }
    }
}
