extern crate toml;
extern crate serde;

use serde::ser::Serialize;

const NO_PRETTY: &'static str = "\
[example]
array = [\"item 1\", \"item 2\"]
empty = []
oneline = \"this has no newlines.\"
text = \"\\nthis is the first line\\nthis is the second line\\n\"
";

#[test]
fn no_pretty() {
    let toml = NO_PRETTY;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    value.serialize(&mut toml::Serializer::new(&mut result)).unwrap();
    println!("EXPECTED:\n{}", toml);
    println!("\nRESULT:\n{}", result);
    assert_eq!(toml, &result);
}

#[test]
fn disable_pretty() {
    let toml = NO_PRETTY;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    {
        let mut serializer = toml::Serializer::pretty(&mut result);
        serializer.pretty_string(false);
        serializer.pretty_array(false);
        value.serialize(&mut serializer).unwrap();
    }
    println!("EXPECTED:\n{}", toml);
    println!("\nRESULT:\n{}", result);
    assert_eq!(toml, &result);
}

const PRETTY_STD: &'static str = "\
[example]
array = [
    'item 1',
    'item 2',
]
empty = []
one = ['one']
oneline = 'this has no newlines.'
text = '''
this is the first line
this is the second line
'''
";

#[test]
fn pretty_std() {
    let toml = PRETTY_STD;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    value.serialize(&mut toml::Serializer::pretty(&mut result)).unwrap();
    println!("EXPECTED:\n{}", toml);
    println!("\nRESULT:\n{}", result);
    assert_eq!(toml, &result);
}


const PRETTY_INDENT_2: &'static str = "\
[example]
array = [
  'item 1',
  'item 2',
]
empty = []
one = ['one']
oneline = 'this has no newlines.'
text = '''
this is the first line
this is the second line
'''
three = [
  'one',
  'two',
  'three',
]
";

#[test]
fn pretty_indent_2() {
    let toml = PRETTY_INDENT_2;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    {
        let mut serializer = toml::Serializer::pretty(&mut result);
        serializer.pretty_array_indent(2);
        value.serialize(&mut serializer).unwrap();
    }
    println!(">> Result:\n{}", result);
    assert_eq!(toml, &result);
}

const PRETTY_INDENT_2_OTHER: &'static str = "\
[example]
array = [
  \"item 1\",
  \"item 2\",
]
empty = []
oneline = \"this has no newlines.\"
text = \"\\nthis is the first line\\nthis is the second line\\n\"
";


#[test]
/// Test pretty indent when gotten the other way
fn pretty_indent_2_other() {
    let toml = PRETTY_INDENT_2_OTHER;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    {
        let mut serializer = toml::Serializer::new(&mut result);
        serializer.pretty_array_indent(2);
        value.serialize(&mut serializer).unwrap();
    }
    assert_eq!(toml, &result);
}


const PRETTY_ARRAY_NO_COMMA: &'static str = "\
[example]
array = [
    \"item 1\",
    \"item 2\"
]
empty = []
oneline = \"this has no newlines.\"
text = \"\\nthis is the first line\\nthis is the second line\\n\"
";
#[test]
/// Test pretty indent when gotten the other way
fn pretty_indent_array_no_comma() {
    let toml = PRETTY_ARRAY_NO_COMMA;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    {
        let mut serializer = toml::Serializer::new(&mut result);
        serializer.pretty_array_trailing_comma(false);
        value.serialize(&mut serializer).unwrap();
    }
    assert_eq!(toml, &result);
}


const PRETTY_NO_STRING: &'static str = "\
[example]
array = [
    \"item 1\",
    \"item 2\",
]
empty = []
oneline = \"this has no newlines.\"
text = \"\\nthis is the first line\\nthis is the second line\\n\"
";
#[test]
/// Test pretty indent when gotten the other way
fn pretty_no_string() {
    let toml = PRETTY_NO_STRING;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    {
        let mut serializer = toml::Serializer::pretty(&mut result);
        serializer.pretty_string(false);
        value.serialize(&mut serializer).unwrap();
    }
    assert_eq!(toml, &result);
}

const PRETTY_TRICKY: &'static str = r##"[example]
f = "\f"
glass = '''
Nothing too unusual, except that I can eat glass in:
- Greek: Μπορώ να φάω σπασμένα γυαλιά χωρίς να πάθω τίποτα. 
- Polish: Mogę jeść szkło, i mi nie szkodzi. 
- Hindi: मैं काँच खा सकता हूँ, मुझे उस से कोई पीडा नहीं होती. 
- Japanese: 私はガラスを食べられます。それは私を傷つけません。 
'''
r = "\r"
r_newline = """
\r
"""
single = '''this is a single line but has '' cuz it's tricky'''
single_tricky = "single line with ''' in it"
tabs = '''
this is pretty standard
	except for some 	tabs right here
'''
text = """
this is the first line.
This has a ''' in it and \"\"\" cuz it's tricky yo
Also ' and \" because why not
this is the fourth line
"""
"##;

#[test]
fn pretty_tricky() {
    let toml = PRETTY_TRICKY;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    value.serialize(&mut toml::Serializer::pretty(&mut result)).unwrap();
    println!("EXPECTED:\n{}", toml);
    println!("\nRESULT:\n{}", result);
    assert_eq!(toml, &result);
}

const PRETTY_TABLE_ARRAY: &'static str = r##"[[array]]
key = 'foo'

[[array]]
key = 'bar'

[abc]
doc = 'this is a table'

[example]
single = 'this is a single line string'
"##;

#[test]
fn pretty_table_array() {
    let toml = PRETTY_TABLE_ARRAY;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    value.serialize(&mut toml::Serializer::pretty(&mut result)).unwrap();
    println!("EXPECTED:\n{}", toml);
    println!("\nRESULT:\n{}", result);
    assert_eq!(toml, &result);
}

const TABLE_ARRAY: &'static str = r##"[[array]]
key = "foo"

[[array]]
key = "bar"

[abc]
doc = "this is a table"

[example]
single = "this is a single line string"
"##;

#[test]
fn table_array() {
    let toml = TABLE_ARRAY;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    value.serialize(&mut toml::Serializer::new(&mut result)).unwrap();
    println!("EXPECTED:\n{}", toml);
    println!("\nRESULT:\n{}", result);
    assert_eq!(toml, &result);
}

const PRETTY_TRICKY_NON_LITERAL: &'static str = r##"[example]
f = "\f"
glass = """
Nothing too unusual, except that I can eat glass in:
- Greek: Μπορώ να φάω σπασμένα γυαλιά χωρίς να πάθω τίποτα. 
- Polish: Mogę jeść szkło, i mi nie szkodzi. 
- Hindi: मैं काँच खा सकता हूँ, मुझे उस से कोई पीडा नहीं होती. 
- Japanese: 私はガラスを食べられます。それは私を傷つけません。 
"""
plain = """
This has a couple of lines
Because it likes to.
"""
r = "\r"
r_newline = """
\r
"""
single = "this is a single line but has '' cuz it's tricky"
single_tricky = "single line with ''' in it"
tabs = """
this is pretty standard
\texcept for some \ttabs right here
"""
text = """
this is the first line.
This has a ''' in it and \"\"\" cuz it's tricky yo
Also ' and \" because why not
this is the fourth line
"""
"##;

#[test]
fn pretty_tricky_non_literal() {
    let toml = PRETTY_TRICKY_NON_LITERAL;
    let value: toml::Value = toml::from_str(toml).unwrap();
    let mut result = String::with_capacity(128);
    {
        let mut serializer = toml::Serializer::pretty(&mut result);
        serializer.pretty_string_literal(false);
        value.serialize(&mut serializer).unwrap();
    }
    println!("EXPECTED:\n{}", toml);
    println!("\nRESULT:\n{}", result);
    assert_eq!(toml, &result);
}
