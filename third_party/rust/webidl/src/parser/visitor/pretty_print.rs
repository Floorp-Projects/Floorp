use std::f64;

use super::ImmutableVisitor;
use parser::ast::*;

/// A visitor that can be used to convert an AST back into source code.
///
/// # Example
///
/// ```
/// use webidl::ast::*;
/// use webidl::visitor::*;
///
/// let ast = vec![Definition::Interface(Interface::NonPartial(NonPartialInterface {
///                 extended_attributes: vec![
///                     Box::new(ExtendedAttribute::NoArguments(
///                         Other::Identifier("Attribute".to_string())))],
///                 inherits: None,
///                 members: vec![InterfaceMember::Attribute(Attribute::Regular(RegularAttribute {
///                              extended_attributes: vec![],
///                              inherits: false,
///                              name: "attr".to_string(),
///                              read_only: true,
///                              type_: Box::new(Type {
///                                  extended_attributes: vec![],
///                                  kind: TypeKind::SignedLong,
///                                  nullable: true
///                              })
///                          }))],
///                 name: "Node".to_string()
///           }))];
/// let mut visitor = PrettyPrintVisitor::new();
/// visitor.visit(&ast);
/// assert_eq!(visitor.get_output(),
///            "[Attribute]\ninterface Node {\n    readonly attribute long? attr;\n};\n\n");
/// ```
#[derive(Clone, Debug, Default, Eq, Hash, PartialEq)]
pub struct PrettyPrintVisitor {
    output: String,
}

impl PrettyPrintVisitor {
    pub fn new() -> Self {
        PrettyPrintVisitor {
            output: String::new(),
        }
    }

    pub fn get_output(&self) -> &String {
        &self.output
    }

    fn stringify_arguments(&mut self, arguments: &[Argument]) {
        self.output.push('(');

        for (index, argument) in arguments.iter().enumerate() {
            if index != 0 {
                self.output.push_str(", ");
            }

            self.visit_argument(argument);
        }

        self.output.push(')');
    }

    fn stringify_extended_attributes(&mut self, extended_attributes: &[Box<ExtendedAttribute>]) {
        if extended_attributes.is_empty() {
            return;
        }

        self.output.push('[');

        for (index, extended_attribute) in extended_attributes.iter().enumerate() {
            if index != 0 {
                self.output.push_str(", ");
            }

            self.visit_extended_attribute(extended_attribute);
        }

        self.output.push(']');
    }

    fn stringify_namespace_members(&mut self, namespace_members: &[NamespaceMember]) {
        let mut iterator = namespace_members.iter().peekable();

        while let Some(member) = iterator.next() {
            self.visit_namespace_member(member);

            if let Some(next_member) = iterator.peek() {
                match *member {
                    NamespaceMember::Attribute(_) => match **next_member {
                        NamespaceMember::Attribute(_) => (),
                        _ => self.output.push('\n'),
                    },
                    NamespaceMember::Operation(_) => match **next_member {
                        NamespaceMember::Operation(_) => (),
                        _ => self.output.push('\n'),
                    },
                }
            }
        }
    }

    #[allow(unknown_lints)]
    #[allow(float_cmp)]
    fn stringify_float_literal(&mut self, float_literal: f64) {
        if float_literal.is_nan() {
            self.output.push_str("NaN");
        } else if float_literal == f64::INFINITY {
            self.output.push_str("Infinity");
        } else if float_literal == f64::NEG_INFINITY {
            self.output.push_str("-Infinity");
        } else {
            self.output.push_str(&*float_literal.to_string());

            if float_literal.floor() == float_literal {
                self.output.push_str(".0");
            }
        }
    }

    fn stringify_interface_members(&mut self, interface_members: &[InterfaceMember]) {
        let mut iterator = interface_members.iter().peekable();

        while let Some(member) = iterator.next() {
            self.visit_interface_member(member);

            if let Some(next_member) = iterator.peek() {
                match *member {
                    InterfaceMember::Attribute(_) => match **next_member {
                        InterfaceMember::Attribute(_) => (),
                        _ => self.output.push('\n'),
                    },
                    InterfaceMember::Const(_) => match **next_member {
                        InterfaceMember::Const(_) => (),
                        _ => self.output.push('\n'),
                    },
                    InterfaceMember::Iterable(_) => match **next_member {
                        InterfaceMember::Iterable(_) => (),
                        _ => self.output.push('\n'),
                    },
                    InterfaceMember::Maplike(_) => match **next_member {
                        InterfaceMember::Maplike(_) => (),
                        _ => self.output.push('\n'),
                    },
                    InterfaceMember::Operation(_) => match **next_member {
                        InterfaceMember::Operation(_) => (),
                        _ => self.output.push('\n'),
                    },
                    InterfaceMember::Setlike(_) => match **next_member {
                        InterfaceMember::Setlike(_) => (),
                        _ => self.output.push('\n'),
                    },
                }
            }
        }
    }

    fn stringify_mixin_members(&mut self, mixin_members: &[MixinMember]) {
        let mut iterator = mixin_members.iter().peekable();

        while let Some(member) = iterator.next() {
            self.visit_mixin_member(member);

            if let Some(next_member) = iterator.peek() {
                match *member {
                    MixinMember::Attribute(_) => match **next_member {
                        MixinMember::Attribute(_) => (),
                        _ => self.output.push('\n'),
                    },
                    MixinMember::Const(_) => match **next_member {
                        MixinMember::Const(_) => (),
                        _ => self.output.push('\n'),
                    },
                    MixinMember::Operation(_) => match **next_member {
                        MixinMember::Operation(_) => (),
                        _ => self.output.push('\n'),
                    },
                }
            }
        }
    }
}

impl<'ast> ImmutableVisitor<'ast> for PrettyPrintVisitor {
    fn visit(&mut self, definitions: &'ast [Definition]) {
        let mut iterator = definitions.iter().peekable();

        while let Some(definition) = iterator.next() {
            self.visit_definition(definition);

            if let Some(next_definition) = iterator.peek() {
                match *definition {
                    Definition::Callback(_) => match **next_definition {
                        Definition::Callback(_) => (),
                        _ => self.output.push('\n'),
                    },
                    Definition::Implements(_) => match **next_definition {
                        Definition::Implements(_) => (),
                        _ => self.output.push('\n'),
                    },
                    Definition::Includes(_) => match **next_definition {
                        Definition::Includes(_) => (),
                        _ => self.output.push('\n'),
                    },
                    Definition::Typedef(_) => match **next_definition {
                        Definition::Typedef(_) => (),
                        _ => self.output.push('\n'),
                    },
                    _ => (),
                }
            }
        }
    }

    fn visit_argument(&mut self, argument: &'ast Argument) {
        self.stringify_extended_attributes(&argument.extended_attributes);

        if !argument.extended_attributes.is_empty() {
            self.output.push(' ');
        }

        if argument.optional && !argument.variadic {
            self.output.push_str("optional ");
        }

        self.visit_type(&argument.type_);

        if argument.variadic {
            self.output.push_str("...");
        }

        self.output.push(' ');
        self.visit_identifier(&argument.name);

        if let Some(ref default_value) = argument.default {
            self.output.push_str(" = ");
            self.visit_default_value(default_value);
        }
    }

    fn visit_argument_list_extended_attribute(&mut self, ex: &'ast ArgumentListExtendedAttribute) {
        self.visit_identifier(&ex.name);
        self.stringify_arguments(&ex.arguments);
    }

    fn visit_callback(&mut self, callback: &'ast Callback) {
        if !callback.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&callback.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("callback ");
        self.visit_identifier(&callback.name);
        self.output.push_str(" = ");
        self.visit_return_type(&callback.return_type);
        self.output.push(' ');
        self.stringify_arguments(&callback.arguments);
        self.output.push_str(";\n");
    }

    fn visit_callback_interface(&mut self, callback_interface: &'ast CallbackInterface) {
        if !callback_interface.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&callback_interface.extended_attributes);
            self.output.push_str("\n");
        }

        self.output.push_str("callback interface ");
        self.visit_identifier(&callback_interface.name);

        if let Some(ref inherit) = callback_interface.inherits {
            self.output.push_str(" : ");
            self.output.push_str(inherit);
        }

        self.output.push_str(" {\n");
        self.stringify_interface_members(&callback_interface.members);
        self.output.push_str("};\n\n");
    }

    fn visit_const(&mut self, const_: &'ast Const) {
        if !const_.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&const_.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    const ");
        self.visit_const_type(&const_.type_);

        if const_.nullable {
            self.output.push('?');
        }

        self.output.push(' ');
        self.visit_identifier(&const_.name);
        self.output.push_str(" = ");
        self.visit_const_value(&const_.value);
        self.output.push_str(";\n");
    }

    fn visit_const_type(&mut self, const_type: &'ast ConstType) {
        match *const_type {
            ConstType::Boolean => self.output.push_str("boolean"),
            ConstType::Byte => self.output.push_str("byte"),
            ConstType::Identifier(ref identifier) => self.visit_identifier(identifier),
            ConstType::Octet => self.output.push_str("octet"),
            ConstType::RestrictedDouble => self.output.push_str("double"),
            ConstType::RestrictedFloat => self.output.push_str("float"),
            ConstType::SignedLong => self.output.push_str("long"),
            ConstType::SignedLongLong => self.output.push_str("long long"),
            ConstType::SignedShort => self.output.push_str("short"),
            ConstType::UnrestrictedDouble => self.output.push_str("unrestricted double"),
            ConstType::UnrestrictedFloat => self.output.push_str("unrestricted float"),
            ConstType::UnsignedLong => self.output.push_str("unsigned long"),
            ConstType::UnsignedLongLong => self.output.push_str("unsigned long long"),
            ConstType::UnsignedShort => self.output.push_str("unsigned short"),
        }
    }

    fn visit_const_value(&mut self, const_value: &'ast ConstValue) {
        match *const_value {
            ConstValue::BooleanLiteral(boolean_literal) => {
                self.output.push_str(&*boolean_literal.to_string());
            }
            ConstValue::FloatLiteral(float_literal) => {
                self.stringify_float_literal(float_literal);
            }
            ConstValue::IntegerLiteral(integer_literal) => {
                self.output.push_str(&*integer_literal.to_string());
            }
            ConstValue::Null => self.output.push_str("null"),
        }
    }

    fn visit_default_value(&mut self, default_value: &'ast DefaultValue) {
        match *default_value {
            DefaultValue::ConstValue(ref const_value) => self.visit_const_value(const_value),
            DefaultValue::EmptySequence => self.output.push_str("[]"),
            DefaultValue::StringLiteral(ref string_literal) => {
                self.output.push('"');
                self.output.push_str(&*string_literal);
                self.output.push('"');
            }
        }
    }

    fn visit_dictionary_member(&mut self, dictionary_member: &'ast DictionaryMember) {
        if !dictionary_member.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&dictionary_member.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    ");

        if dictionary_member.required {
            self.output.push_str("required ");
        }

        self.visit_type(&dictionary_member.type_);
        self.output.push(' ');
        self.visit_identifier(&dictionary_member.name);

        if let Some(ref default_value) = dictionary_member.default {
            self.output.push_str(" = ");
            self.visit_default_value(default_value);
        }

        self.output.push_str(";\n");
    }

    fn visit_enum(&mut self, enum_: &'ast Enum) {
        if !enum_.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&enum_.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("enum ");
        self.visit_identifier(&enum_.name);
        self.output.push_str(" {\n");

        for (index, variant) in enum_.variants.iter().enumerate() {
            if index != 0 {
                self.output.push_str(",\n");
            }

            self.output.push_str("    \"");
            self.output.push_str(&*variant);
            self.output.push('"');
        }

        self.output.push_str("\n};\n\n");
    }

    fn visit_explicit_stringifier_operation(
        &mut self,
        operation: &'ast ExplicitStringifierOperation,
    ) {
        if !operation.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&operation.extended_attributes);
            self.output.push('\n');
        }

        self.stringify_extended_attributes(&operation.extended_attributes);
        self.output.push_str("    stringifier ");
        self.visit_return_type(&operation.return_type);
        self.output.push(' ');

        if let Some(ref name) = operation.name {
            self.visit_identifier(name);
            self.output.push(' ');
        }

        self.stringify_arguments(&operation.arguments);
        self.output.push_str(";\n");
    }

    fn visit_identifier(&mut self, identifier: &'ast str) {
        let identifier = match identifier {
            "any" => "_any",
            "ArrayBuffer" => "_ArrayBuffer",
            "attribute" => "_attribute",
            "boolean" => "_boolean",
            "byte" => "_byte",
            "ByteString" => "_ByteString",
            "callback" => "_callback",
            "const" => "_const",
            "DataView" => "_DataView",
            "deleter" => "_deleter",
            "dictionary" => "_dictionary",
            "DOMString" => "_DOMString",
            "double" => "_double",
            "enum" => "_enum",
            "Error" => "_Error",
            "false" => "_false",
            "float" => "_float",
            "Float32Array" => "_Float32Array",
            "Float64Array" => "_Float64Array",
            "FrozenArray" => "_FrozenArray",
            "getter" => "_getter",
            "implements" => "_implements",
            "includes" => "_includes",
            "inherit" => "_inherit",
            "Int16Array" => "_Int16Array",
            "Int32Array" => "_Int32Array",
            "Int8Array" => "_Int8Array",
            "interface" => "_interface",
            "iterable" => "_iterable",
            "legacycaller" => "_legacycaller",
            "long" => "_long",
            "maplike" => "_maplike",
            "namespace" => "_namespace",
            "NaN" => "_NaN",
            "null" => "_null",
            "object" => "_object",
            "octet" => "_octet",
            "optional" => "_optional",
            "or" => "_or",
            "partial" => "_partial",
            "Infinity" => "_Infinity",
            "Promise" => "_Promise",
            "readonly" => "_readonly",
            "record" => "_record",
            "required" => "_required",
            "sequence" => "_sequence",
            "setlike" => "_setlike",
            "setter" => "_setter",
            "short" => "_short",
            "static" => "_static",
            "stringifier" => "_stringifier",
            "symbol" => "_symbol",
            "true" => "_true",
            "typedef" => "_typedef",
            "USVString" => "_USVString",
            "Uint16Array" => "_Uint16Array",
            "Uint32Array" => "_Uint32Array",
            "Uint8Array" => "_Uint8Array",
            "Uint8ClampedArray" => "_Uint8ClampedArray",
            "unrestricted" => "_unrestricted",
            "unsigned" => "_unsigned",
            "void" => "_void",
            identifier => identifier,
        };

        self.output.push_str(identifier);
    }

    fn visit_identifier_extended_attribute(
        &mut self,
        extended_attribute: &'ast IdentifierExtendedAttribute,
    ) {
        self.visit_identifier(&extended_attribute.lhs);
        self.output.push('=');
        self.visit_other(&extended_attribute.rhs);
    }

    fn visit_identifier_list_extended_attribute(
        &mut self,
        ex: &'ast IdentifierListExtendedAttribute,
    ) {
        self.visit_identifier(&ex.lhs);
        self.output.push_str("=(");

        for (index, identifier) in ex.rhs.iter().enumerate() {
            if index != 0 {
                self.output.push_str(", ");
            }

            self.visit_identifier(identifier);
        }

        self.output.push(')');
    }

    fn visit_implicit_stringifier_operation(
        &mut self,
        operation: &'ast ImplicitStringifierOperation,
    ) {
        if !operation.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&operation.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    stringifier;\n");
    }

    fn visit_implements(&mut self, implements: &'ast Implements) {
        if !implements.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&implements.extended_attributes);
            self.output.push('\n');
        }

        self.visit_identifier(&implements.implementer);
        self.output.push_str(" implements ");
        self.visit_identifier(&implements.implementee);
        self.output.push_str(";\n");
    }

    fn visit_includes(&mut self, includes: &'ast Includes) {
        if !includes.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&includes.extended_attributes);
            self.output.push('\n');
        }

        self.visit_identifier(&includes.includer);
        self.output.push_str(" includes ");
        self.visit_identifier(&includes.includee);
        self.output.push_str(";\n");
    }

    fn visit_iterable(&mut self, iterable: &'ast Iterable) {
        if !iterable.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&iterable.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    iterable<");

        if let Some(ref key_type) = iterable.key_type {
            self.visit_type(key_type);
            self.output.push_str(", ");
        }

        self.visit_type(&iterable.value_type);
        self.output.push_str(">;\n");
    }

    fn visit_maplike(&mut self, maplike: &'ast Maplike) {
        if !maplike.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&maplike.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    ");

        if maplike.read_only {
            self.output.push_str("readonly ");
        }

        self.output.push_str("maplike<");
        self.visit_type(&maplike.key_type);
        self.output.push_str(", ");
        self.visit_type(&maplike.value_type);
        self.output.push_str(">;\n");
    }

    fn visit_named_argument_list_extended_attribute(
        &mut self,
        ex: &'ast NamedArgumentListExtendedAttribute,
    ) {
        self.visit_identifier(&ex.lhs_name);
        self.output.push('=');
        self.visit_identifier(&ex.rhs_name);
        self.stringify_arguments(&ex.rhs_arguments);
    }

    fn visit_non_partial_dictionary(&mut self, non_partial_dictionary: &'ast NonPartialDictionary) {
        if !non_partial_dictionary.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&non_partial_dictionary.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("dictionary ");
        self.visit_identifier(&non_partial_dictionary.name);

        if let Some(ref inherit) = non_partial_dictionary.inherits {
            self.output.push_str(" : ");
            self.output.push_str(inherit);
        }

        self.output.push_str(" {\n");

        for member in &non_partial_dictionary.members {
            self.visit_dictionary_member(member);
        }

        self.output.push_str("};\n\n");
    }

    fn visit_non_partial_interface(&mut self, non_partial_interface: &'ast NonPartialInterface) {
        if !non_partial_interface.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&non_partial_interface.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("interface ");
        self.visit_identifier(&non_partial_interface.name);

        if let Some(ref inherit) = non_partial_interface.inherits {
            self.output.push_str(" : ");
            self.output.push_str(inherit);
        }

        self.output.push_str(" {\n");
        self.stringify_interface_members(&non_partial_interface.members);
        self.output.push_str("};\n\n");
    }

    fn visit_non_partial_mixin(&mut self, non_partial_mixin: &'ast NonPartialMixin) {
        if !non_partial_mixin.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&non_partial_mixin.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("interface mixin ");
        self.visit_identifier(&non_partial_mixin.name);

        self.output.push_str(" {\n");
        self.stringify_mixin_members(&non_partial_mixin.members);
        self.output.push_str("};\n\n");
    }

    fn visit_non_partial_namespace(&mut self, non_partial_namespace: &'ast NonPartialNamespace) {
        if !non_partial_namespace.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&non_partial_namespace.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("namespace ");
        self.visit_identifier(&non_partial_namespace.name);
        self.output.push_str(" {\n");
        self.stringify_namespace_members(&non_partial_namespace.members);
        self.output.push_str("};\n\n");
    }

    fn visit_other(&mut self, other: &'ast Other) {
        match *other {
            Other::Any => self.output.push_str("any"),
            Other::ArrayBuffer => self.output.push_str("ArrayBuffer"),
            Other::Attribute => self.output.push_str("attribute"),
            Other::Boolean => self.output.push_str("boolean"),
            Other::Byte => self.output.push_str("byte"),
            Other::ByteString => self.output.push_str("ByteString"),
            Other::Callback => self.output.push_str("callback"),
            Other::Const => self.output.push_str("const"),
            Other::DOMString => self.output.push_str("DOMString"),
            Other::DataView => self.output.push_str("DataView"),
            Other::Deleter => self.output.push_str("deleter"),
            Other::Dictionary => self.output.push_str("dictionary"),
            Other::Double => self.output.push_str("double"),
            Other::Enum => self.output.push_str("enum"),
            Other::False => self.output.push_str("false"),
            Other::Float => self.output.push_str("float"),
            Other::Float32Array => self.output.push_str("Float32Array"),
            Other::Float64Array => self.output.push_str("Float64Array"),
            Other::FrozenArray => self.output.push_str("FrozenArray"),
            Other::Getter => self.output.push_str("getter"),
            Other::Implements => self.output.push_str("implements"),
            Other::Includes => self.output.push_str("includes"),
            Other::Inherit => self.output.push_str("inherit"),
            Other::Int16Array => self.output.push_str("Int16Array"),
            Other::Int32Array => self.output.push_str("Int32Array"),
            Other::Int8Array => self.output.push_str("Int8Array"),
            Other::Interface => self.output.push_str("interface"),
            Other::Iterable => self.output.push_str("iterable"),
            Other::LegacyCaller => self.output.push_str("legacycaller"),
            Other::Long => self.output.push_str("long"),
            Other::Maplike => self.output.push_str("maplike"),
            Other::Namespace => self.output.push_str("namespace"),
            Other::NegativeInfinity => self.output.push_str("-Infinity"),
            Other::NaN => self.output.push_str("NaN"),
            Other::Null => self.output.push_str("null"),
            Other::Object => self.output.push_str("object"),
            Other::Octet => self.output.push_str("octet"),
            Other::Optional => self.output.push_str("optional"),
            Other::Or => self.output.push_str("or"),
            Other::Partial => self.output.push_str("partial"),
            Other::PositiveInfinity => self.output.push_str("Infinity"),
            Other::Required => self.output.push_str("required"),
            Other::Sequence => self.output.push_str("sequence"),
            Other::Setlike => self.output.push_str("setlike"),
            Other::Setter => self.output.push_str("setter"),
            Other::Short => self.output.push_str("short"),
            Other::Static => self.output.push_str("static"),
            Other::Stringifier => self.output.push_str("stringifier"),
            Other::True => self.output.push_str("true"),
            Other::Typedef => self.output.push_str("typedef"),
            Other::USVString => self.output.push_str("USVString"),
            Other::Uint16Array => self.output.push_str("Uint16Array"),
            Other::Uint32Array => self.output.push_str("Uint32Array"),
            Other::Uint8Array => self.output.push_str("Uint8Array"),
            Other::Uint8ClampedArray => self.output.push_str("Uint8ClampedArray"),
            Other::Unrestricted => self.output.push_str("unrestricted"),
            Other::Unsigned => self.output.push_str("unsigned"),
            Other::Void => self.output.push_str("void"),

            Other::FloatLiteral(float_literal) => {
                self.stringify_float_literal(float_literal);
            }
            Other::Identifier(ref identifier) => self.visit_identifier(identifier),
            Other::IntegerLiteral(integer_literal) => {
                self.output.push_str(&*integer_literal.to_string());
            }
            Other::OtherLiteral(other_literal) => self.output.push(other_literal),
            Other::StringLiteral(ref string_literal) => {
                self.output.push('"');
                self.output.push_str(&*string_literal);
                self.output.push('"');
            }

            Other::Colon => self.output.push(':'),
            Other::Ellipsis => self.output.push_str("..."),
            Other::Equals => self.output.push('='),
            Other::GreaterThan => self.output.push('>'),
            Other::Hyphen => self.output.push('-'),
            Other::LessThan => self.output.push('<'),
            Other::Period => self.output.push('.'),
            Other::QuestionMark => self.output.push('?'),
            Other::Semicolon => self.output.push(';'),
        }
    }

    fn visit_other_extended_attribute(&mut self, extended_attribute: &'ast OtherExtendedAttribute) {
        match *extended_attribute {
            OtherExtendedAttribute::Nested {
                ref group_type,
                ref inner,
                ref rest,
            } => {
                let end_group_char = match *group_type {
                    OtherExtendedAttributeGroupType::Brace => {
                        self.output.push('{');
                        '}'
                    }
                    OtherExtendedAttributeGroupType::Bracket => {
                        self.output.push('[');
                        ']'
                    }
                    OtherExtendedAttributeGroupType::Parenthesis => {
                        self.output.push('(');
                        ')'
                    }
                };

                if let Some(ref extended_attribute) = *inner {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.output.push(end_group_char);

                if let Some(ref extended_attribute) = *rest {
                    self.visit_extended_attribute(extended_attribute);
                }
            }
            OtherExtendedAttribute::Other {
                ref other,
                ref rest,
            } => {
                if let Some(ref other) = *other {
                    self.visit_other(other);
                }

                if let Some(ref extended_attribute) = *rest {
                    self.visit_extended_attribute(extended_attribute);
                }
            }
        }
    }

    fn visit_partial_dictionary(&mut self, partial_dictionary: &'ast PartialDictionary) {
        if !partial_dictionary.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&partial_dictionary.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("partial dictionary ");
        self.visit_identifier(&partial_dictionary.name);
        self.output.push_str(" {\n");

        for member in &partial_dictionary.members {
            self.visit_dictionary_member(member);
        }

        self.output.push_str("};\n\n");
    }

    fn visit_partial_interface(&mut self, partial_interface: &'ast PartialInterface) {
        if !partial_interface.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&partial_interface.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("partial interface ");
        self.visit_identifier(&partial_interface.name);
        self.output.push_str(" {\n");
        self.stringify_interface_members(&partial_interface.members);
        self.output.push_str("};\n\n");
    }

    fn visit_partial_mixin(&mut self, partial_mixin: &'ast PartialMixin) {
        if !partial_mixin.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&partial_mixin.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("partial interface mixin ");
        self.visit_identifier(&partial_mixin.name);
        self.output.push_str(" {\n");
        self.stringify_mixin_members(&partial_mixin.members);
        self.output.push_str("};\n\n");
    }

    fn visit_partial_namespace(&mut self, partial_namespace: &'ast PartialNamespace) {
        if !partial_namespace.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&partial_namespace.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("partial namespace ");
        self.visit_identifier(&partial_namespace.name);
        self.output.push_str(" {\n");
        self.stringify_namespace_members(&partial_namespace.members);
        self.output.push_str("};\n\n");
    }

    fn visit_regular_attribute(&mut self, regular_attribute: &'ast RegularAttribute) {
        if !regular_attribute.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&regular_attribute.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    ");

        if regular_attribute.inherits {
            self.output.push_str("inherit ");
        }

        if regular_attribute.read_only {
            self.output.push_str("readonly ");
        }

        self.output.push_str("attribute ");
        self.visit_type(&regular_attribute.type_);
        self.output.push(' ');
        self.visit_identifier(&regular_attribute.name);
        self.output.push_str(";\n");
    }

    fn visit_regular_operation(&mut self, regular_operation: &'ast RegularOperation) {
        if !regular_operation.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&regular_operation.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    ");
        self.visit_return_type(&regular_operation.return_type);
        self.output.push(' ');

        if let Some(ref name) = regular_operation.name {
            self.visit_identifier(name);
        }

        self.stringify_arguments(&regular_operation.arguments);
        self.output.push_str(";\n");
    }

    fn visit_return_type(&mut self, return_type: &'ast ReturnType) {
        match *return_type {
            ReturnType::NonVoid(ref type_) => self.visit_type(type_),
            ReturnType::Void => self.output.push_str("void"),
        }
    }

    fn visit_setlike(&mut self, setlike: &'ast Setlike) {
        if !setlike.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&setlike.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    ");

        if setlike.read_only {
            self.output.push_str("readonly ");
        }

        self.output.push_str("setlike<");
        self.visit_type(&setlike.type_);
        self.output.push_str(">;\n");
    }

    fn visit_special(&mut self, special: &'ast Special) {
        match *special {
            Special::Deleter => self.output.push_str("deleter"),
            Special::Getter => self.output.push_str("getter"),
            Special::LegacyCaller => self.output.push_str("legacycaller"),
            Special::Setter => self.output.push_str("setter"),
        }
    }

    fn visit_special_operation(&mut self, special_operation: &'ast SpecialOperation) {
        if !special_operation.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&special_operation.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    ");

        for special_keyword in &special_operation.special_keywords {
            self.visit_special(special_keyword);
            self.output.push(' ');
        }

        self.visit_return_type(&special_operation.return_type);
        self.output.push(' ');

        if let Some(ref name) = special_operation.name {
            self.visit_identifier(name);
        }

        self.stringify_arguments(&special_operation.arguments);
        self.output.push_str(";\n");
    }

    fn visit_static_attribute(&mut self, static_attribute: &'ast StaticAttribute) {
        if !static_attribute.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&static_attribute.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    static ");

        if static_attribute.read_only {
            self.output.push_str("readonly ");
        }

        self.output.push_str("attribute ");
        self.visit_type(&static_attribute.type_);
        self.output.push(' ');
        self.visit_identifier(&static_attribute.name);
        self.output.push_str(";\n");
    }

    fn visit_static_operation(&mut self, static_operation: &'ast StaticOperation) {
        if !static_operation.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&static_operation.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    static ");
        self.visit_return_type(&static_operation.return_type);
        self.output.push(' ');

        if let Some(ref name) = static_operation.name {
            self.visit_identifier(name);
        }

        self.stringify_arguments(&static_operation.arguments);
        self.output.push_str(";\n");
    }

    fn visit_string_type(&mut self, string_type: &'ast StringType) {
        match *string_type {
            StringType::ByteString => self.output.push_str("ByteString"),
            StringType::DOMString => self.output.push_str("DOMString"),
            StringType::USVString => self.output.push_str("USVString"),
        }
    }

    fn visit_stringifier_attribute(&mut self, stringifier_attribute: &'ast StringifierAttribute) {
        if !stringifier_attribute.extended_attributes.is_empty() {
            self.output.push_str("    ");
            self.stringify_extended_attributes(&stringifier_attribute.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("    stringifier ");

        if stringifier_attribute.read_only {
            self.output.push_str("readonly ");
        }

        self.output.push_str("attribute ");
        self.visit_type(&stringifier_attribute.type_);
        self.output.push(' ');
        self.visit_identifier(&stringifier_attribute.name);
        self.output.push_str(";\n");
    }

    fn visit_type(&mut self, type_: &'ast Type) {
        self.stringify_extended_attributes(&type_.extended_attributes);

        if !type_.extended_attributes.is_empty() {
            self.output.push(' ');
        }

        self.visit_type_kind(&type_.kind);

        if type_.nullable {
            self.output.push_str("?");
        }
    }

    fn visit_type_kind(&mut self, type_kind: &'ast TypeKind) {
        match *type_kind {
            TypeKind::Any => self.output.push_str("any"),
            TypeKind::ArrayBuffer => self.output.push_str("ArrayBuffer"),
            TypeKind::Boolean => self.output.push_str("boolean"),
            TypeKind::Byte => self.output.push_str("byte"),
            TypeKind::ByteString => self.output.push_str("ByteString"),
            TypeKind::DOMString => self.output.push_str("DOMString"),
            TypeKind::DataView => self.output.push_str("DataView"),
            TypeKind::Error => self.output.push_str("Error"),
            TypeKind::Float32Array => self.output.push_str("Float32Array"),
            TypeKind::Float64Array => self.output.push_str("Float64Array"),
            TypeKind::FrozenArray(ref type_) => {
                self.output.push_str("FrozenArray<");
                self.visit_type(type_);
                self.output.push('>');
            }
            TypeKind::Identifier(ref identifier) => self.visit_identifier(identifier),
            TypeKind::Int16Array => self.output.push_str("Int16Array"),
            TypeKind::Int32Array => self.output.push_str("Int32Array"),
            TypeKind::Int8Array => self.output.push_str("Int8Array"),
            TypeKind::Octet => self.output.push_str("octet"),
            TypeKind::Object => self.output.push_str("object"),
            TypeKind::Promise(ref return_type) => {
                self.output.push_str("Promise<");
                self.visit_return_type(return_type);
                self.output.push('>');
            }
            TypeKind::Record(ref string_type, ref value_type) => {
                self.output.push_str("record<");
                self.visit_string_type(string_type);
                self.output.push_str(", ");
                self.visit_type(value_type);
                self.output.push('>');
            }
            TypeKind::RestrictedDouble => self.output.push_str("double"),
            TypeKind::RestrictedFloat => self.output.push_str("float"),
            TypeKind::Sequence(ref type_) => {
                self.output.push_str("sequence<");
                self.visit_type(type_);
                self.output.push('>');
            }
            TypeKind::SignedLong => self.output.push_str("long"),
            TypeKind::SignedLongLong => self.output.push_str("long long"),
            TypeKind::SignedShort => self.output.push_str("short"),
            TypeKind::Symbol => self.output.push_str("symbol"),
            TypeKind::USVString => self.output.push_str("USVString"),
            TypeKind::Uint16Array => self.output.push_str("Uint16Array"),
            TypeKind::Uint32Array => self.output.push_str("Uint32Array"),
            TypeKind::Uint8Array => self.output.push_str("Uint8Array"),
            TypeKind::Uint8ClampedArray => self.output.push_str("Uint8ClampedArray"),
            TypeKind::Union(ref types) => {
                self.output.push('(');

                for (index, type_) in types.iter().enumerate() {
                    if index != 0 {
                        self.output.push_str(" or ");
                    }

                    self.visit_type(type_);
                }

                self.output.push(')');
            }
            TypeKind::UnrestrictedDouble => self.output.push_str("unrestricted double"),
            TypeKind::UnrestrictedFloat => self.output.push_str("unrestricted float"),
            TypeKind::UnsignedLong => self.output.push_str("unsigned long"),
            TypeKind::UnsignedLongLong => self.output.push_str("unsigned long long"),
            TypeKind::UnsignedShort => self.output.push_str("unsigned short"),
        }
    }

    fn visit_typedef(&mut self, typedef: &'ast Typedef) {
        if !typedef.extended_attributes.is_empty() {
            self.stringify_extended_attributes(&typedef.extended_attributes);
            self.output.push('\n');
        }

        self.output.push_str("typedef ");
        self.visit_type(&typedef.type_);
        self.output.push(' ');
        self.visit_identifier(&typedef.name);
        self.output.push_str(";\n");
    }
}
