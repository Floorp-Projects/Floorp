use common::{
    is_name_start_char, is_name_char,
};

use reader::events::XmlEvent;
use reader::lexer::Token;

use super::{Result, PullParser, State, ProcessingInstructionSubstate, DeclarationSubstate};

impl PullParser {
    pub fn inside_processing_instruction(&mut self, t: Token, s: ProcessingInstructionSubstate) -> Option<Result> {
        match s {
            ProcessingInstructionSubstate::PIInsideName => match t {
                Token::Character(c) if !self.buf_has_data() && is_name_start_char(c) ||
                                 self.buf_has_data() && is_name_char(c) => self.append_char_continue(c),

                Token::ProcessingInstructionEnd => {
                    // self.buf contains PI name
                    let name = self.take_buf();

                    // Don't need to check for declaration because it has mandatory attributes
                    // but there is none
                    match &name[..] {
                        // Name is empty, it is an error
                        "" => Some(self_error!(self; "Encountered processing instruction without name")),

                        // Found <?xml-like PI not at the beginning of a document,
                        // it is an error - see section 2.6 of XML 1.1 spec
                        "xml"|"xmL"|"xMl"|"xML"|"Xml"|"XmL"|"XMl"|"XML" =>
                            Some(self_error!(self; "Invalid processing instruction: <?{}", name)),

                        // All is ok, emitting event
                        _ => {
                            self.into_state_emit(
                                State::OutsideTag,
                                Ok(XmlEvent::ProcessingInstruction {
                                    name: name,
                                    data: None
                                })
                            )
                        }
                    }
                }

                Token::Whitespace(_) => {
                    // self.buf contains PI name
                    let name = self.take_buf();

                    match &name[..] {
                        // We have not ever encountered an element and have not parsed XML declaration
                        "xml" if !self.encountered_element && !self.parsed_declaration =>
                            self.into_state_continue(State::InsideDeclaration(DeclarationSubstate::BeforeVersion)),

                        // Found <?xml-like PI after the beginning of a document,
                        // it is an error - see section 2.6 of XML 1.1 spec
                        "xml"|"xmL"|"xMl"|"xML"|"Xml"|"XmL"|"XMl"|"XML"
                            if self.encountered_element || self.parsed_declaration =>
                            Some(self_error!(self; "Invalid processing instruction: <?{}", name)),

                        // All is ok, starting parsing PI data
                        _ => {
                            self.lexer.disable_errors();  // data is arbitrary, so disable errors
                            self.data.name = name;
                            self.into_state_continue(State::InsideProcessingInstruction(ProcessingInstructionSubstate::PIInsideData))
                        }

                    }
                }

                _ => Some(self_error!(self; "Unexpected token: <?{}{}", self.buf, t))
            },

            ProcessingInstructionSubstate::PIInsideData => match t {
                Token::ProcessingInstructionEnd => {
                    self.lexer.enable_errors();
                    let name = self.data.take_name();
                    let data = self.take_buf();
                    self.into_state_emit(
                        State::OutsideTag,
                        Ok(XmlEvent::ProcessingInstruction {
                            name: name,
                            data: Some(data)
                        })
                    )
                },

                // Any other token should be treated as plain characters
                _ => {
                    t.push_to_string(&mut self.buf);
                    None
                }
            },
        }
    }

}
