use crate::argument::ArgumentList;
use crate::attribute::ExtendedAttributeList;
use crate::common::{Identifier, Parenthesized};
use crate::interface::{ConstMember, StringifierMember};
use crate::types::{AttributedType, ReturnType};

/// Parses the members declarations of a mixin
pub type MixinMembers<'a> = Vec<MixinMember<'a>>;

ast_types! {
    /// Parses one of the variants of a mixin member
    enum MixinMember<'a> {
        Const(ConstMember<'a>),
        /// Parses `[attributes]? stringifier? returntype identifier? (( args ));`
        ///
        /// (( )) means ( ) chars
        Operation(struct OperationMixinMember<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            stringifier: Option<term!(stringifier)>,
            return_type: ReturnType<'a>,
            identifier: Option<Identifier<'a>>,
            args: Parenthesized<ArgumentList<'a>>,
            semi_colon: term!(;),
        }),
        /// Parses `[attributes]? stringifier? readonly? attribute attributedtype identifier;`
        Attribute(struct AttributeMixinMember<'a> {
            attributes: Option<ExtendedAttributeList<'a>>,
            stringifier: Option<term!(stringifier)>,
            readonly: Option<term!(readonly)>,
            attribute: term!(attribute),
            type_: AttributedType<'a>,
            identifier: Identifier<'a>,
            semi_colon: term!(;),
        }),
        Stringifier(StringifierMember<'a>),
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::Parse;

    test!(should_parse_attribute_mixin_member { "stringifier readonly attribute short name;" =>
        "";
        AttributeMixinMember;
        attributes.is_none();
        stringifier.is_some();
        readonly.is_some();
        identifier.0 == "name";
    });

    test!(should_parse_operation_mixin_member { "short fnName(long a);" =>
        "";
        OperationMixinMember;
        attributes.is_none();
        stringifier.is_none();
        identifier.is_some();
    });
}
