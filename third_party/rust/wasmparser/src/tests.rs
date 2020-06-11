/* Copyright 2017 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#[cfg(test)]
mod simple_tests {
    use crate::parser::{Parser, ParserInput, ParserState, WasmDecoder};
    use crate::primitives::{Operator, SectionCode};
    use std::fs::File;
    use std::io::prelude::*;
    use std::path::PathBuf;

    fn read_file_data(path: &PathBuf) -> Vec<u8> {
        println!("Parsing {:?}", path);
        let mut data = Vec::new();
        let mut f = File::open(path).ok().unwrap();
        f.read_to_end(&mut data).unwrap();
        data
    }

    macro_rules! expect_state {
        ($state:expr, $expected:pat) => {{
            {
                let state: &ParserState = $state;
                match *state {
                    $expected => (),
                    _ => panic!("Unexpected state during testing: {:?}", state),
                }
            }
        }};
    }

    #[test]
    fn default_read() {
        let data = read_file_data(&PathBuf::from("../../tests/local/simple.wasm"));
        let mut parser = Parser::new(data.as_slice());

        expect_state!(parser.read(), ParserState::BeginWasm { .. });
        expect_state!(parser.read(), ParserState::BeginSection { code: SectionCode::Type, .. });
        expect_state!(parser.read(), ParserState::TypeSectionEntry(_));
        expect_state!(parser.read(), ParserState::EndSection);
        expect_state!(parser.read(), ParserState::BeginSection { code: SectionCode::Function, .. });
        expect_state!(parser.read(), ParserState::FunctionSectionEntry(_));
        expect_state!(parser.read(), ParserState::EndSection);
        expect_state!(parser.read(), ParserState::BeginSection { code: SectionCode::Code, .. });
        expect_state!(parser.read(), ParserState::BeginFunctionBody { .. });
        expect_state!(parser.read(), ParserState::FunctionBodyLocals { .. });
        expect_state!(parser.read(), ParserState::CodeOperator(_));
        expect_state!(parser.read(), ParserState::CodeOperator(Operator::End));
        expect_state!(parser.read(), ParserState::EndFunctionBody);
        expect_state!(parser.read(), ParserState::EndSection);
        expect_state!(parser.read(), ParserState::EndWasm);
    }

    #[test]
    fn default_read_with_input() {
        let data = read_file_data(&PathBuf::from("../../tests/local/simple.wasm"));
        let mut parser = Parser::new(data.as_slice());

        expect_state!(parser.read(), ParserState::BeginWasm { .. });
        expect_state!(parser.read_with_input(ParserInput::Default),
            ParserState::BeginSection { code: SectionCode::Type, .. });
        expect_state!(parser.read(), ParserState::TypeSectionEntry(_));
        expect_state!(parser.read(), ParserState::EndSection);
        expect_state!(parser.read(), ParserState::BeginSection { code: SectionCode::Function, ..});
        expect_state!(
            parser.read_with_input(ParserInput::ReadSectionRawData),
            ParserState::SectionRawData(_)
        );
        expect_state!(parser.read(), ParserState::EndSection);
        expect_state!(parser.read(), ParserState::BeginSection { code: SectionCode::Code, .. });
        expect_state!(parser.read(), ParserState::BeginFunctionBody { .. });
        expect_state!(
            parser.read_with_input(ParserInput::SkipFunctionBody),
            ParserState::EndSection
        );
        expect_state!(parser.read(), ParserState::EndWasm);
    }

    #[test]
    fn skipping() {
        let data = read_file_data(&PathBuf::from("../../tests/local/naming.wasm"));
        let mut parser = Parser::new(data.as_slice());

        expect_state!(parser.read(),
            ParserState::BeginWasm { .. });
        expect_state!(parser.read_with_input(ParserInput::Default),
            ParserState::BeginSection { code: SectionCode::Type, .. });
        expect_state!(parser.read_with_input(ParserInput::SkipSection),
            ParserState::BeginSection { code: SectionCode::Import, ..});
        expect_state!(parser.read_with_input(ParserInput::SkipSection),
            ParserState::BeginSection { code: SectionCode::Function, ..});
        expect_state!(parser.read_with_input(ParserInput::SkipSection),
            ParserState::BeginSection { code: SectionCode::Global, ..});
        expect_state!(parser.read_with_input(ParserInput::SkipSection),
            ParserState::BeginSection { code: SectionCode::Export, ..});
        expect_state!(parser.read_with_input(ParserInput::SkipSection),
            ParserState::BeginSection { code: SectionCode::Element, ..});
        expect_state!(parser.read_with_input(ParserInput::SkipSection),
            ParserState::BeginSection { code: SectionCode::Code, .. });
        expect_state!(parser.read(),
            ParserState::BeginFunctionBody { .. });
        expect_state!(parser.read_with_input(ParserInput::SkipFunctionBody),
            ParserState::BeginFunctionBody { .. });
        expect_state!(parser.read_with_input(ParserInput::SkipFunctionBody),
            ParserState::BeginFunctionBody { .. });
        expect_state!(parser.read_with_input(ParserInput::SkipFunctionBody),
            ParserState::BeginFunctionBody { .. });
        expect_state!(parser.read_with_input(ParserInput::SkipFunctionBody),
            ParserState::BeginFunctionBody { .. });
        expect_state!(parser.read_with_input(ParserInput::SkipFunctionBody),
            ParserState::BeginFunctionBody { .. });
        expect_state!(parser.read_with_input(ParserInput::SkipFunctionBody),
            ParserState::BeginFunctionBody { .. });
        expect_state!(
            parser.read_with_input(ParserInput::SkipFunctionBody),
            ParserState::EndSection
        );
        expect_state!(parser.read(),
            ParserState::BeginSection { code: SectionCode::Custom { .. }, ..});
        expect_state!(parser.read_with_input(ParserInput::SkipSection),
            ParserState::BeginSection { code: SectionCode::Custom { .. }, .. });
        expect_state!(
            parser.read_with_input(ParserInput::SkipSection),
            ParserState::EndWasm
        );
    }
}
