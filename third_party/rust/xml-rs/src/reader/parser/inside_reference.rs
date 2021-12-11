use std::char;

use common::{is_name_start_char, is_name_char, is_whitespace_str};

use reader::lexer::Token;

use super::{Result, PullParser, State};

impl PullParser {
    pub fn inside_reference(&mut self, t: Token, prev_st: State) -> Option<Result> {
        match t {
            Token::Character(c) if !self.data.ref_data.is_empty() && is_name_char(c) ||
                             self.data.ref_data.is_empty() && (is_name_start_char(c) || c == '#') => {
                self.data.ref_data.push(c);
                None
            }

            Token::ReferenceEnd => {
                // TODO: check for unicode correctness
                let name = self.data.take_ref_data();
                let name_len = name.len();  // compute once
                let c = match &name[..] {
                    "lt"   => Ok('<'.to_string()),
                    "gt"   => Ok('>'.to_string()),
                    "amp"  => Ok('&'.to_string()),
                    "apos" => Ok('\''.to_string()),
                    "quot" => Ok('"'.to_string()),
                    ""     => Err(self_error!(self; "Encountered empty entity")),
                    _ if name_len > 2 && name.starts_with("#x") => {
                        let num_str = &name[2..name_len];
                        if num_str == "0" {
                            Err(self_error!(self; "Null character entity is not allowed"))
                        } else {
                            if self.config.replace_unknown_entity_references {
                                match u32::from_str_radix(num_str, 16).ok().map(|i| char::from_u32(i).unwrap_or('\u{fffd}')) {
                                    Some(c) => Ok(c.to_string()),
                                    None    => Err(self_error!(self; "Invalid hexadecimal character number in an entity: {}", name))
                                }
                            } else {
                                match u32::from_str_radix(num_str, 16).ok().and_then(char::from_u32) {
                                    Some(c) => Ok(c.to_string()),
                                    None    => Err(self_error!(self; "Invalid hexadecimal character number in an entity: {}", name))
                                }
                            }
                        }
                    }
                    _ if name_len > 1 && name.starts_with('#') => {
                        let num_str = &name[1..name_len];
                        if num_str == "0" {
                            Err(self_error!(self; "Null character entity is not allowed"))
                        } else {
                            if self.config.replace_unknown_entity_references {
                                match u32::from_str_radix(num_str, 10).ok().map(|i| char::from_u32(i).unwrap_or('\u{fffd}')) {
                                    Some(c) => Ok(c.to_string()),
                                    None    => Err(self_error!(self; "Invalid decimal character number in an entity: {}", name))
                                }
                            }
                            else {
                                match u32::from_str_radix(num_str, 10).ok().and_then(char::from_u32) {
                                    Some(c) => Ok(c.to_string()),
                                    None    => Err(self_error!(self; "Invalid decimal character number in an entity: {}", name))
                                }
                            }
                        }
                    },
                    _ => {
                        if let Some(v) = self.config.extra_entities.get(&name) {
                            Ok(v.clone())
                        } else {
                            Err(self_error!(self; "Unexpected entity: {}", name))
                        }
                    }
                };
                match c {
                    Ok(c) => {
                        self.buf.push_str(&c);
                        if prev_st == State::OutsideTag && !is_whitespace_str(&c) {
                            self.inside_whitespace = false;
                        }
                        self.into_state_continue(prev_st)
                    }
                    Err(e) => Some(e)
                }
            }

            _ => Some(self_error!(self; "Unexpected token inside an entity: {}", t))
        }
    }
}
