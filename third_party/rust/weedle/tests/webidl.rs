extern crate weedle;

use std::fs;
use std::io::Read;

use weedle::*;

fn read_file(path: &str) -> String {
    let mut file = fs::File::open(path).unwrap();
    let mut file_content = String::new();
    file.read_to_string(&mut file_content).unwrap();
    file_content
}

#[test]
pub fn should_parse_dom_webidl() {
    let content = read_file("./tests/defs/dom.webidl");
    let parsed = weedle::parse(&content).unwrap();

    assert_eq!(parsed.len(), 62);
}

#[test]
fn should_parse_html_webidl() {
    let content = read_file("./tests/defs/html.webidl");
    let parsed = weedle::parse(&content).unwrap();

    assert_eq!(parsed.len(), 325);
}

#[test]
fn should_parse_mediacapture_streams_webidl() {
    let content = read_file("./tests/defs/mediacapture-streams.webidl");
    let parsed = weedle::parse(&content).unwrap();

    assert_eq!(parsed.len(), 37);
}

#[test]
fn interface_constructor() {
    let content = read_file("./tests/defs/interface-constructor.webidl");
    let mut parsed = weedle::parse(&content).unwrap();

    assert_eq!(parsed.len(), 1);

    let definition = parsed.pop().unwrap();

    match definition {
        Definition::Interface(mut interface) => {
            assert!(interface.attributes.is_none());
            assert_eq!(interface.interface, term!(interface));
            assert_eq!(interface.identifier.0, "InterfaceWithConstructor");
            assert_eq!(interface.inheritance, None);

            assert_eq!(interface.members.body.len(), 1);

            let body = interface.members.body.pop().unwrap();

            match body {
                interface::InterfaceMember::Constructor(constructor) => {
                    let mut attributes = constructor.attributes.unwrap().body.list;
                    assert_eq!(attributes.len(), 1);
                    let attribute = attributes.pop().unwrap();

                    match attribute {
                        attribute::ExtendedAttribute::NoArgs(attribute) => {
                            assert_eq!((attribute.0).0, "Throws");
                        }
                        _ => unreachable!(),
                    }

                    let mut args = constructor.args.body.list;
                    assert_eq!(args.len(), 1);
                    let arg = args.pop().unwrap();

                    match arg {
                        argument::Argument::Single(arg) => {
                            assert!(arg.attributes.is_none());
                            assert!(arg.optional.is_none());
                            assert!(arg.type_.attributes.is_none());

                            match arg.type_.type_ {
                                types::Type::Single(types::SingleType::NonAny(
                                    types::NonAnyType::Integer(_),
                                )) => {}
                                _ => unreachable!(),
                            }
                        }
                        _ => unreachable!(),
                    };

                    assert_eq!(constructor.constructor, term::Constructor);
                }
                _ => unreachable!(),
            }
        }
        _ => unreachable!(),
    }
}
