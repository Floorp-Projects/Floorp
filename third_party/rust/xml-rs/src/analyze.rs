extern crate xml;

use std::cmp;
use std::env;
use std::io::{self, Read, Write, BufReader};
use std::fs::File;
use std::collections::HashSet;

use xml::ParserConfig;
use xml::reader::XmlEvent;

macro_rules! abort {
    ($code:expr) => {::std::process::exit($code)};
    ($code:expr, $($args:tt)+) => {{
        writeln!(&mut ::std::io::stderr(), $($args)+).unwrap();
        ::std::process::exit($code);
    }}
}

fn main() {
    let mut file;
    let mut stdin;
    let source: &mut Read = match env::args().nth(1) {
        Some(file_name) => {
            file = File::open(file_name)
                .unwrap_or_else(|e| abort!(1, "Cannot open input file: {}", e));
            &mut file
        }
        None => {
            stdin = io::stdin();
            &mut stdin
        }
    };

    let reader = ParserConfig::new()
        .whitespace_to_characters(true)
        .ignore_comments(false)
        .create_reader(BufReader::new(source));

    let mut processing_instructions = 0;
    let mut elements = 0;
    let mut character_blocks = 0;
    let mut cdata_blocks = 0;
    let mut characters = 0;
    let mut comment_blocks = 0;
    let mut comment_characters = 0;
    let mut namespaces = HashSet::new();
    let mut depth = 0;
    let mut max_depth = 0;

    for e in reader {
        match e {
            Ok(e) => match e {
                XmlEvent::StartDocument { version, encoding, standalone } =>
                    println!(
                        "XML document version {}, encoded in {}, {}standalone",
                        version, encoding, if standalone.unwrap_or(false) { "" } else { "not " }
                    ),
                XmlEvent::EndDocument => println!("Document finished"),
                XmlEvent::ProcessingInstruction { .. } => processing_instructions += 1,
                XmlEvent::Whitespace(_) => {}  // can't happen due to configuration
                XmlEvent::Characters(s) => {
                    character_blocks += 1;
                    characters += s.len();
                }
                XmlEvent::CData(s) => {
                    cdata_blocks += 1;
                    characters += s.len();
                }
                XmlEvent::Comment(s) => {
                    comment_blocks += 1;
                    comment_characters += s.len();
                }
                XmlEvent::StartElement { namespace, .. } => {
                    depth += 1;
                    max_depth = cmp::max(max_depth, depth);
                    elements += 1;
                    namespaces.extend(namespace.0.into_iter().map(|(_, ns_uri)| ns_uri));
                }
                XmlEvent::EndElement { .. } => {
                    depth -= 1;
                }
            },
            Err(e) => abort!(1, "Error parsing XML document: {}", e)
        }
    }
    namespaces.remove(xml::namespace::NS_EMPTY_URI);
    namespaces.remove(xml::namespace::NS_XMLNS_URI);
    namespaces.remove(xml::namespace::NS_XML_URI);

    println!("Elements: {}, maximum depth: {}", elements, max_depth);
    println!("Namespaces (excluding built-in): {}", namespaces.len());
    println!("Characters: {}, characters blocks: {}, CDATA blocks: {}",
             characters, character_blocks, cdata_blocks);
    println!("Comment blocks: {}, comment characters: {}", comment_blocks, comment_characters);
    println!("Processing instructions (excluding built-in): {}", processing_instructions);
}
