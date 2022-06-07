use crate::argument::ArgumentList;
use crate::attribute::ExtendedAttributeList;
use crate::common::{Identifier, Parenthesized};
use crate::types::{AttributedType, ReturnType};

/// Parses namespace members declaration
pub type NamespaceMembers<'a> = Vec<NamespaceMember<'a>>;

ast_types! {
    /// Parses namespace member declaration
    enum NamespaceMember<'a> {
        /// Parses `[attributes]? returntype identifier? (( args ));`
        ///
        /// (( )) means ( ) chars
        Operation(struct OperationNamespaceMember<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            return_type: ReturnType<'a>,
            identifier: Option<Identifier<'a>>,
            args: Parenthesized<ArgumentList<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attribute]? readonly attributetype type identifier;`
        Attribute(struct AttributeNamespaceMember<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            readonly: term!(readonly),
            attribute: term!(attribute),
            type_: AttributedType<'a>,
            identifier: Identifier<'a>,
            semi_colon: term!(;),
        }),
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::Parse;

    test!(should_parse_attribute_namespace_member { "readonly attribute short name;" =>
        "";
        AttributeNamespaceMember;
        attributes.is_none();
        identifier.0 == "name";
    });

    test!(should_parse_operation_namespace_member { "short (long a, long b);" =>
        "";
        OperationNamespaceMember;
        attributes.is_none();
        identifier.is_none();
    });
}
