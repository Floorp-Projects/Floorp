#![allow(missing_docs)]

/// Converts AST to a pretty printed source string.
pub mod pretty_print;

use super::ast::*;

pub use self::pretty_print::PrettyPrintVisitor;

macro_rules! make_visitor {
    ($visitor_trait_name:ident, $($mutability:ident)*) => {
        #[cfg_attr(rustfmt, rustfmt_skip)]
        pub trait $visitor_trait_name<'ast> {
            // Override the following functions. The `walk` functions are the default behavior.

            /// This is the initial function used to start traversing the AST. By default, this
            /// will simply recursively walk down the AST without performing any meaningful action.
            fn visit(&mut self, definitions: &'ast $($mutability)* [Definition]) {
                for definition in definitions {
                    self.visit_definition(definition);
                }
            }

            fn visit_argument(&mut self, argument: &'ast $($mutability)* Argument) {
                self.walk_argument(argument);
            }

            fn visit_argument_list_extended_attribute(
                &mut self,
                ex: &'ast $($mutability)* ArgumentListExtendedAttribute)
            {
                self.walk_argument_list_extended_attribute(ex);
            }

            fn visit_attribute(&mut self, attribute: &'ast $($mutability)* Attribute) {
                self.walk_attribute(attribute);
            }

            fn visit_callback(&mut self, callback: &'ast $($mutability)* Callback) {
                self.walk_callback(callback);
            }

            fn visit_callback_interface(
                &mut self,
                callback_interface: &'ast $($mutability)* CallbackInterface)
            {
                self.walk_callback_interface(callback_interface);
            }

            fn visit_const(&mut self, const_: &'ast $($mutability)* Const) {
                self.walk_const(const_);
            }

            fn visit_const_type(&mut self, const_type: &'ast $($mutability)* ConstType) {
                self.walk_const_type(const_type);
            }

            fn visit_const_value(&mut self, _const_value: &'ast $($mutability)* ConstValue) {}

            fn visit_default_value(&mut self, default_value: &'ast $($mutability)* DefaultValue) {
                self.walk_default_value(default_value);
            }

            fn visit_definition(&mut self, definition: &'ast $($mutability)* Definition) {
                self.walk_definition(definition);
            }

            fn visit_dictionary(&mut self, dictionary: &'ast $($mutability)* Dictionary) {
                self.walk_dictionary(dictionary);
            }

            fn visit_dictionary_member(&mut self,
                                       dictionary_member: &'ast $($mutability)* DictionaryMember) {
                self.walk_dictionary_member(dictionary_member);
            }

            fn visit_enum(&mut self, enum_: &'ast $($mutability)* Enum) {
                self.walk_enum(enum_);
            }

            fn visit_explicit_stringifier_operation(
                &mut self,
                op: &'ast $($mutability)* ExplicitStringifierOperation)
            {
                self.walk_explicit_stringifier_operation(op);
            }

            fn visit_extended_attribute(&mut self,
                                        ex: &'ast $($mutability)* ExtendedAttribute) {
                self.walk_extended_attribute(ex);
            }

            fn visit_identifier(&mut self, _identifier: &'ast $($mutability)* str) {}

            fn visit_identifier_extended_attribute(
                &mut self,
                ex: &'ast $($mutability)* IdentifierExtendedAttribute)
            {
                self.walk_identifier_extended_attribute(ex);
            }

            fn visit_identifier_list_extended_attribute(
                &mut self,
                ex: &'ast $($mutability)* IdentifierListExtendedAttribute)
            {
                self.walk_identifier_list_extended_attribute(ex);
            }

            fn visit_implicit_stringifier_operation(
                &mut self,
                op: &'ast $($mutability)* ImplicitStringifierOperation)
            {
                self.walk_implicit_stringifier_operation(op);
            }

            fn visit_implements(&mut self, implements: &'ast $($mutability)* Implements) {
                self.walk_implements(implements);
            }

            fn visit_includes(&mut self, includes: &'ast $($mutability)* Includes) {
                self.walk_includes(includes);
            }

            fn visit_interface(&mut self, interface: &'ast $($mutability)* Interface) {
                self.walk_interface(interface);
            }

            fn visit_interface_member(&mut self,
                                      interface_member: &'ast $($mutability)* InterfaceMember) {
                self.walk_interface_member(interface_member);
            }

            fn visit_iterable(&mut self, iterable: &'ast $($mutability)* Iterable) {
                self.walk_iterable(iterable);
            }

            fn visit_maplike(&mut self, maplike: &'ast $($mutability)* Maplike) {
                self.walk_maplike(maplike);
            }

            fn visit_mixin(&mut self, mixin: &'ast $($mutability)* Mixin) {
                self.walk_mixin(mixin);
            }

            fn visit_mixin_member(&mut self, mixin_member: &'ast $($mutability)* MixinMember) {
                self.walk_mixin_member(mixin_member);
            }

            fn visit_named_argument_list_extended_attribute(
                &mut self,
                ex: &'ast $($mutability)* NamedArgumentListExtendedAttribute)
            {
                self.walk_named_argument_list_extended_attribute(ex);
            }

            fn visit_namespace(&mut self, namespace: &'ast $($mutability)* Namespace) {
                self.walk_namespace(namespace);
            }

            fn visit_namespace_member(&mut self,
                                      namespace_member: &'ast $($mutability)* NamespaceMember) {
                self.walk_namespace_member(namespace_member);
            }

            fn visit_non_partial_dictionary(
                &mut self,
                dictionary: &'ast $($mutability)* NonPartialDictionary)
            {
                self.walk_non_partial_dictionary(dictionary);
            }

            fn visit_non_partial_interface(&mut self,
                                           interface: &'ast $($mutability)* NonPartialInterface) {
                self.walk_non_partial_interface(interface);
            }

            fn visit_non_partial_mixin(&mut self,
                                       mixin: &'ast $($mutability)* NonPartialMixin) {
                self.walk_non_partial_mixin(mixin);
            }

            fn visit_non_partial_namespace(&mut self,
                                           namespace: &'ast $($mutability)* NonPartialNamespace) {
                self.walk_non_partial_namespace(namespace);
            }

            fn visit_operation(&mut self, operation: &'ast $($mutability)* Operation) {
                self.walk_operation(operation);
            }

            fn visit_other(&mut self, other: &'ast $($mutability)* Other) {
                self.walk_other(other);
            }

            fn visit_other_extended_attribute(&mut self,
                                              ex: &'ast $($mutability)* OtherExtendedAttribute) {
                self.walk_other_extended_attribute(ex);
            }

            fn visit_partial_dictionary(&mut self,
                                        dictionary: &'ast $($mutability)* PartialDictionary) {
                self.walk_partial_dictionary(dictionary);
            }

            fn visit_partial_interface(&mut self,
                                       interface: &'ast $($mutability)* PartialInterface) {
                self.walk_partial_interface(interface);
            }

            fn visit_partial_mixin(&mut self,
                                   mixin: &'ast $($mutability)* PartialMixin) {
                self.walk_partial_mixin(mixin);
            }

            fn visit_partial_namespace(&mut self,
                                       namespace: &'ast $($mutability)* PartialNamespace) {
                self.walk_partial_namespace(namespace);
            }

            fn visit_regular_attribute(&mut self,
                                       regular_attribute: &'ast $($mutability)* RegularAttribute) {
                self.walk_regular_attribute(regular_attribute);
            }

            fn visit_regular_operation(&mut self,
                                       regular_operation: &'ast $($mutability)* RegularOperation) {
                self.walk_regular_operation(regular_operation);
            }

            fn visit_return_type(&mut self, return_type: &'ast $($mutability)* ReturnType) {
                self.walk_return_type(return_type);
            }

            fn visit_setlike(&mut self, setlike: &'ast $($mutability)* Setlike) {
                self.walk_setlike(setlike);
            }

            fn visit_special(&mut self, _special: &'ast $($mutability)* Special) {}

            fn visit_special_operation(&mut self,
                                       special_operation: &'ast $($mutability)* SpecialOperation) {
                self.walk_special_operation(special_operation);
            }

            fn visit_static_attribute(&mut self,
                                      static_attribute: &'ast $($mutability)* StaticAttribute) {
                self.walk_static_attribute(static_attribute);
            }

            fn visit_static_operation(&mut self,
                                      static_operation: &'ast $($mutability)* StaticOperation) {
                self.walk_static_operation(static_operation);
            }

            fn visit_string_type(&mut self, _string_type: &'ast $($mutability)* StringType) {}

            fn visit_stringifier_attribute(&mut self,
                                           attribute: &'ast $($mutability)* StringifierAttribute) {
                self.walk_stringifier_attribute(attribute);
            }

            fn visit_stringifier_operation(&mut self,
                                           operation: &'ast $($mutability)* StringifierOperation) {
                self.walk_stringifier_operation(operation);
            }

            fn visit_type(&mut self, type_: &'ast $($mutability)* Type) {
                self.walk_type(type_);
            }

            fn visit_type_kind(&mut self, type_kind: &'ast $($mutability)* TypeKind) {
                self.walk_type_kind(type_kind);
            }

            fn visit_typedef(&mut self, typedef: &'ast $($mutability)* Typedef) {
                self.walk_typedef(typedef);
            }

            // The `walk` functions are not meant to be overridden.

            fn walk_argument(&mut self, argument: &'ast $($mutability)* Argument) {
                for extended_attribute in &$($mutability)* argument.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_type(&$($mutability)* argument.type_);
                self.visit_identifier(&$($mutability)* argument.name);

                if let Some(ref $($mutability)* default_value) = argument.default {
                    self.visit_default_value(default_value);
                }
            }

            fn walk_argument_list_extended_attribute(
                &mut self,
                ex: &'ast $($mutability)* ArgumentListExtendedAttribute)
            {
                self.visit_identifier(&$($mutability)* ex.name);

                for argument in &$($mutability)* ex.arguments {
                    self.visit_argument(argument);
                }
            }

            fn walk_attribute(&mut self, attribute: &'ast $($mutability)* Attribute) {
                match *attribute {
                    Attribute::Regular(ref $($mutability)* attribute) => {
                        self.visit_regular_attribute(attribute);
                    }
                    Attribute::Static(ref $($mutability)* attribute) => {
                        self.visit_static_attribute(attribute);
                    }
                    Attribute::Stringifier(ref $($mutability)* attribute) => {
                        self.visit_stringifier_attribute(attribute);
                    }
                }
            }

            fn walk_callback(&mut self, callback: &'ast $($mutability)* Callback) {
                for extended_attribute in &$($mutability)* callback.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* callback.name);
                self.visit_return_type(&$($mutability)* callback.return_type);

                for argument in &$($mutability)* callback.arguments {
                    self.visit_argument(argument);
                }
            }

            fn walk_callback_interface(&mut self,
                                       callback_interface: &'ast $($mutability)* CallbackInterface)
            {
                for extended_attribute in &$($mutability)* callback_interface.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* callback_interface.name);

                if let Some(ref $($mutability)* inherits) = callback_interface.inherits {
                    self.visit_identifier(inherits);
                }

                for member in &$($mutability)* callback_interface.members {
                    self.visit_interface_member(member);
                }
            }

            fn walk_const(&mut self, const_: &'ast $($mutability)* Const) {
                for extended_attribute in &$($mutability)* const_.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_const_type(&$($mutability)* const_.type_);
                self.visit_identifier(&$($mutability)* const_.name);
                self.visit_const_value(&$($mutability)* const_.value);
            }

            fn walk_const_type(&mut self, const_type: &'ast $($mutability)* ConstType) {
                if let ConstType::Identifier(ref $($mutability)* identifier) = *const_type {
                    self.visit_identifier(identifier);
                }
            }

            fn walk_default_value(&mut self, default_value: &'ast $($mutability)* DefaultValue) {
                if let DefaultValue::ConstValue(ref $($mutability)* const_value) = *default_value {
                    self.visit_const_value(const_value);
                }
            }

            fn walk_definition(&mut self, definition: &'ast $($mutability)* Definition) {
                match *definition {
                    Definition::Callback(ref $($mutability)* callback) => {
                        self.visit_callback(callback);
                    }
                    Definition::Dictionary(ref $($mutability)* dictionary) => {
                        self.visit_dictionary(dictionary);
                    }
                    Definition::Enum(ref $($mutability)* enum_) => {
                        self.visit_enum(enum_);
                    }
                    Definition::Implements(ref $($mutability)* implements) => {
                        self.visit_implements(implements);
                    }
                    Definition::Includes(ref $($mutability)* includes) => {
                        self.visit_includes(includes);
                    }
                    Definition::Interface(ref $($mutability)* interface) => {
                        self.visit_interface(interface);
                    }
                    Definition::Mixin(ref $($mutability)* mixin) => {
                        self.visit_mixin(mixin);
                    }
                    Definition::Namespace(ref $($mutability)* namespace) => {
                        self.visit_namespace(namespace);
                    }
                    Definition::Typedef(ref $($mutability)* typedef) => {
                        self.visit_typedef(typedef);
                    }
                }
            }

            fn walk_dictionary(&mut self, dictionary: &'ast $($mutability)* Dictionary) {
                match *dictionary {
                    Dictionary::NonPartial(ref $($mutability)* dictionary) => {
                        self.visit_non_partial_dictionary(dictionary);
                    }
                    Dictionary::Partial(ref $($mutability)* dictionary) => {
                        self.visit_partial_dictionary(dictionary);
                    }
                }
            }

            fn walk_dictionary_member(&mut self,
                                      dictionary_member: &'ast $($mutability)* DictionaryMember)
            {
                for extended_attribute in &$($mutability)* dictionary_member.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_type(&$($mutability)* dictionary_member.type_);
                self.visit_identifier(&$($mutability)* dictionary_member.name);

                if let Some(ref $($mutability)* default_value) = dictionary_member.default {
                    self.visit_default_value(default_value);
                }
            }

            fn walk_enum(&mut self, enum_: &'ast $($mutability)* Enum) {
                for extended_attribute in &$($mutability)* enum_.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* enum_.name);
            }

            fn walk_explicit_stringifier_operation(
                &mut self,
                op: &'ast $($mutability)* ExplicitStringifierOperation)
            {
                for extended_attribute in &$($mutability)* op.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_return_type(&$($mutability)* op.return_type);

                if let Some(ref $($mutability)* name) = op.name {
                    self.visit_identifier(name);
                }

                for argument in &$($mutability)* op.arguments {
                    self.visit_argument(argument);
                }
            }

            fn walk_extended_attribute(&mut self,
                                       ex: &'ast $($mutability)* ExtendedAttribute)
            {
                use ast::ExtendedAttribute::*;

                match *ex {
                    ArgumentList(ref $($mutability)* extended_attribute) => {
                        self.visit_argument_list_extended_attribute(extended_attribute);
                    }
                    Identifier(ref $($mutability)* extended_attribute) => {
                        self.visit_identifier_extended_attribute(extended_attribute);
                    }
                    IdentifierList(ref $($mutability)* extended_attribute) => {
                        self.visit_identifier_list_extended_attribute(extended_attribute);
                    }
                    NamedArgumentList(ref $($mutability)* extended_attribute) => {
                        self.visit_named_argument_list_extended_attribute(extended_attribute);
                    }
                    NoArguments(ref $($mutability)* extended_attribute) => {
                        self.visit_other(extended_attribute);
                    }
                }
            }

            fn walk_identifier_extended_attribute(
                &mut self,
                ex: &'ast $($mutability)* IdentifierExtendedAttribute)
            {
                self.visit_identifier(&$($mutability)* ex.lhs);
                self.visit_other(&$($mutability)* ex.rhs);
            }

            fn walk_identifier_list_extended_attribute(
                &mut self,
                ex: &'ast $($mutability)* IdentifierListExtendedAttribute)
            {
                self.visit_identifier(&$($mutability)* ex.lhs);

                for identifier in &$($mutability)* ex.rhs {
                    self.visit_identifier(identifier);
                }
            }

            fn walk_implicit_stringifier_operation(
                &mut self,
                op: &'ast $($mutability)* ImplicitStringifierOperation)
            {
                for extended_attribute in &$($mutability)* op.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }
            }

            fn walk_implements(&mut self, implements: &'ast $($mutability)* Implements) {
                for extended_attribute in &$($mutability)* implements.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* implements.implementer);
                self.visit_identifier(&$($mutability)* implements.implementee);
            }

            fn walk_includes(&mut self, includes: &'ast $($mutability)* Includes) {
                for extended_attribute in &$($mutability)* includes.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* includes.includer);
                self.visit_identifier(&$($mutability)* includes.includee);
            }

            fn walk_interface(&mut self, interface: &'ast $($mutability)* Interface) {
                match *interface {
                    Interface::Callback(ref $($mutability)* interface) => {
                        self.visit_callback_interface(interface);
                    }
                    Interface::NonPartial(ref $($mutability)* interface) => {
                        self.visit_non_partial_interface(interface);
                    }
                    Interface::Partial(ref $($mutability)* interface) => {
                        self.visit_partial_interface(interface);
                    }
                }
            }

            fn walk_interface_member(&mut self,
                                     interface_member: &'ast $($mutability)* InterfaceMember) {
                match *interface_member {
                    InterfaceMember::Attribute(ref $($mutability)* member) => {
                        self.visit_attribute(member);
                    }
                    InterfaceMember::Const(ref $($mutability)* member) => {
                        self.visit_const(member);
                    }
                    InterfaceMember::Iterable(ref $($mutability)* member) => {
                        self.visit_iterable(member);
                    }
                    InterfaceMember::Maplike(ref $($mutability)* member) => {
                        self.visit_maplike(member);
                    }
                    InterfaceMember::Operation(ref $($mutability)* member) => {
                        self.visit_operation(member);
                    }
                    InterfaceMember::Setlike(ref $($mutability)* member) => {
                        self.visit_setlike(member);
                    }
                }
            }

            fn walk_iterable(&mut self, iterable: &'ast $($mutability)* Iterable) {
                for extended_attribute in &$($mutability)* iterable.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                if let Some(ref $($mutability)* key_type) = iterable.key_type {
                    self.visit_type(key_type);
                }

                self.visit_type(&$($mutability)* iterable.value_type);
            }

            fn walk_maplike(&mut self, maplike: &'ast $($mutability)* Maplike) {
                for extended_attribute in &$($mutability)* maplike.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_type(&$($mutability)* maplike.key_type);
                self.visit_type(&$($mutability)* maplike.value_type);
            }

            fn walk_mixin(&mut self, mixin: &'ast $($mutability)* Mixin) {
                match *mixin {
                    Mixin::NonPartial(ref $($mutability)* mixin) => {
                        self.visit_non_partial_mixin(mixin);
                    }
                    Mixin::Partial(ref $($mutability)* mixin) => {
                        self.visit_partial_mixin(mixin);
                    }
                }
            }

            fn walk_mixin_member(&mut self, mixin_member: &'ast $($mutability)* MixinMember) {
                match *mixin_member {
                    MixinMember::Attribute(ref $($mutability)* member) => {
                        self.visit_attribute(member);
                    }
                    MixinMember::Const(ref $($mutability)* member) => {
                        self.visit_const(member);
                    }
                    MixinMember::Operation(ref $($mutability)* member) => {
                        self.visit_operation(member);
                    }
                }
            }

            fn walk_named_argument_list_extended_attribute(
                &mut self,
                ex: &'ast $($mutability)* NamedArgumentListExtendedAttribute)
            {
                self.visit_identifier(&$($mutability)* ex.lhs_name);
                self.visit_identifier(&$($mutability)* ex.rhs_name);

                for argument in &$($mutability)* ex.rhs_arguments {
                    self.visit_argument(argument);
                }
            }

            fn walk_namespace(&mut self, namespace: &'ast $($mutability)* Namespace) {
                match *namespace {
                    Namespace::NonPartial(ref $($mutability)* namespace) => {
                        self.visit_non_partial_namespace(namespace);
                    }
                    Namespace::Partial(ref $($mutability)* namespace) => {
                        self.visit_partial_namespace(namespace);
                    }
                }
            }

            fn walk_namespace_member(&mut self,
                                     namespace_member: &'ast $($mutability)* NamespaceMember) {
                match *namespace_member {
                    NamespaceMember::Attribute(ref $($mutability)* member) => {
                        self.visit_attribute(member);
                    }
                    NamespaceMember::Operation(ref $($mutability)* member) => {
                        self.visit_operation(member);
                    }
                }
            }

            fn walk_non_partial_dictionary(
                &mut self,
                dictionary: &'ast $($mutability)* NonPartialDictionary)
            {
                for extended_attribute in &$($mutability)* dictionary.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* dictionary.name);

                if let Some(ref $($mutability)* inherits) = dictionary.inherits {
                    self.visit_identifier(inherits);
                }

                for member in &$($mutability)* dictionary.members {
                    self.visit_dictionary_member(member);
                }
            }

            fn walk_non_partial_interface(
                &mut self,
                interface: &'ast $($mutability)* NonPartialInterface)
            {
                for extended_attribute in &$($mutability)* interface.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* interface.name);

                if let Some(ref $($mutability)* inherits) = interface.inherits {
                    self.visit_identifier(inherits);
                }

                for member in &$($mutability)* interface.members {
                    self.visit_interface_member(member);
                }
            }

            fn walk_non_partial_mixin( &mut self, mixin: &'ast $($mutability)* NonPartialMixin)
            {
                for extended_attribute in &$($mutability)* mixin.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* mixin.name);

                for member in &$($mutability)* mixin.members {
                    self.visit_mixin_member(member);
                }
            }

            fn walk_non_partial_namespace(
                &mut self,
                namespace: &'ast $($mutability)* NonPartialNamespace)
            {
                for extended_attribute in &$($mutability)* namespace.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* namespace.name);

                for member in &$($mutability)* namespace.members {
                    self.visit_namespace_member(member);
                }
            }

            fn walk_operation(&mut self, operation: &'ast $($mutability)* Operation) {
                match *operation {
                    Operation::Regular(ref $($mutability)* operation) => {
                        self.visit_regular_operation(operation);
                    }
                    Operation::Special(ref $($mutability)* operation) => {
                        self.visit_special_operation(operation);
                    }
                    Operation::Static(ref $($mutability)* operation) => {
                        self.visit_static_operation(operation);
                    }
                    Operation::Stringifier(ref $($mutability)* operation) => {
                        self.visit_stringifier_operation(operation);
                    }
                }
            }

            fn walk_other(&mut self, other: &'ast $($mutability)* Other) {
                if let Other::Identifier(ref $($mutability)* identifier) = *other {
                    self.visit_identifier(identifier);
                }
            }

            fn walk_other_extended_attribute(&mut self,
                                             ex: &'ast $($mutability)* OtherExtendedAttribute)
            {
                match *ex {
                    OtherExtendedAttribute::Nested {
                        ref $($mutability)* inner,
                        ref $($mutability)* rest,
                        ..
                    } => {
                        if let Some(ref $($mutability)* inner) = *inner {
                            self.visit_extended_attribute(inner);
                        }

                        if let Some(ref $($mutability)* rest) = *rest {
                            self.visit_extended_attribute(rest);
                        }
                    }
                    OtherExtendedAttribute::Other {
                        ref $($mutability)* other,
                        ref $($mutability)* rest,
                        ..
                    } => {
                        if let Some(ref $($mutability)* other) = *other {
                            self.visit_other(other);
                        }

                        if let Some(ref $($mutability)* rest) = *rest {
                            self.visit_extended_attribute(rest);
                        }
                    }
                }
            }

            fn walk_partial_dictionary(&mut self,
                                       partial_dictionary: &'ast $($mutability)* PartialDictionary)
            {
                for extended_attribute in &$($mutability)* partial_dictionary.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* partial_dictionary.name);

                for member in &$($mutability)* partial_dictionary.members {
                    self.visit_dictionary_member(member);
                }
            }

            fn walk_partial_interface(&mut self,
                                      partial_interface: &'ast $($mutability)* PartialInterface)
            {
                for extended_attribute in &$($mutability)* partial_interface.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* partial_interface.name);

                for member in &$($mutability)* partial_interface.members {
                    self.visit_interface_member(member);
                }
            }

            fn walk_partial_mixin(&mut self, partial_mixin: &'ast $($mutability)* PartialMixin)
            {
                for extended_attribute in &$($mutability)* partial_mixin.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* partial_mixin.name);

                for member in &$($mutability)* partial_mixin.members {
                    self.visit_mixin_member(member);
                }
            }

            fn walk_partial_namespace(&mut self,
                                      partial_namespace: &'ast $($mutability)* PartialNamespace)
            {
                for extended_attribute in &$($mutability)* partial_namespace.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_identifier(&$($mutability)* partial_namespace.name);

                for member in &$($mutability)* partial_namespace.members {
                    self.visit_namespace_member(member);
                }
            }

            fn walk_regular_attribute(&mut self,
                                      regular_attribute: &'ast $($mutability)* RegularAttribute)
            {
                for extended_attribute in &$($mutability)* regular_attribute.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_type(&$($mutability)* regular_attribute.type_);
                self.visit_identifier(&$($mutability)* regular_attribute.name);
            }

            fn walk_regular_operation(&mut self,
                                      regular_operation: &'ast $($mutability)* RegularOperation)
            {
                for extended_attribute in &$($mutability)* regular_operation.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_return_type(&$($mutability)* regular_operation.return_type);

                if let Some(ref $($mutability)* name) = regular_operation.name {
                    self.visit_identifier(name);
                }

                for argument in &$($mutability)* regular_operation.arguments {
                    self.visit_argument(argument);
                }
            }

            fn walk_return_type(&mut self, return_type: &'ast $($mutability)* ReturnType) {
                if let ReturnType::NonVoid(ref $($mutability)* type_) = *return_type {
                    self.visit_type(type_);
                }
            }

            fn walk_setlike(&mut self, setlike: &'ast $($mutability)* Setlike) {
                for extended_attribute in &$($mutability)* setlike.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_type(&$($mutability)* setlike.type_);
            }

            fn walk_special_operation(&mut self,
                                      special_operation: &'ast $($mutability)* SpecialOperation)
            {
                for extended_attribute in &$($mutability)* special_operation.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                for special_keyword in &$($mutability)* special_operation.special_keywords {
                    self.visit_special(special_keyword);
                }

                self.visit_return_type(&$($mutability)* special_operation.return_type);

                if let Some(ref $($mutability)* name) = special_operation.name {
                    self.visit_identifier(name);
                }

                for argument in &$($mutability)* special_operation.arguments {
                    self.visit_argument(argument);
                }
            }

            fn walk_static_attribute(&mut self,
                                     static_attribute: &'ast $($mutability)* StaticAttribute)
            {
                for extended_attribute in &$($mutability)* static_attribute.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_type(&$($mutability)* static_attribute.type_);
                self.visit_identifier(&$($mutability)* static_attribute.name);
            }

            fn walk_static_operation(&mut self,
                                     static_operation: &'ast $($mutability)* StaticOperation)
            {
                for extended_attribute in &$($mutability)* static_operation.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_return_type(&$($mutability)* static_operation.return_type);

                if let Some(ref $($mutability)* name) = static_operation.name {
                    self.visit_identifier(name);
                }

                for argument in &$($mutability)* static_operation.arguments {
                    self.visit_argument(argument);
                }
            }

            fn walk_stringifier_attribute(
                &mut self,
                attribute: &'ast $($mutability)* StringifierAttribute)
            {
                for extended_attribute in &$($mutability)* attribute.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_type(&$($mutability)* attribute.type_);
                self.visit_identifier(&$($mutability)* attribute.name);
            }

            fn walk_stringifier_operation(
                &mut self,
                stringifier_operation: &'ast $($mutability)* StringifierOperation)
            {
                match *stringifier_operation {
                    StringifierOperation::Explicit(ref $($mutability)* operation) => {
                        self.visit_explicit_stringifier_operation(operation);
                    }
                    StringifierOperation::Implicit(ref $($mutability)* operation) => {
                        self.visit_implicit_stringifier_operation(operation);
                    }
                }
            }

            fn walk_type(&mut self, type_: &'ast $($mutability)* Type) {
                for extended_attribute in &$($mutability)* type_.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_type_kind(&$($mutability)* type_.kind);
            }

            fn walk_type_kind(&mut self, type_kind: &'ast $($mutability)* TypeKind) {
                match *type_kind {
                    TypeKind::FrozenArray(ref $($mutability)* type_) => {
                        self.visit_type(type_);
                    }
                    TypeKind::Identifier(ref $($mutability)* identifier) => {
                        self.visit_identifier(identifier);
                    }
                    TypeKind::Promise(ref $($mutability)* return_type) => {
                        self.visit_return_type(return_type);
                    }
                    TypeKind::Record(_, ref $($mutability)* type_) => {
                        self.visit_type(type_);
                    }
                    TypeKind::Sequence(ref $($mutability)* type_) => {
                        self.visit_type(type_);
                    }
                    TypeKind::Union(ref $($mutability)* types) => {
                        for type_ in types {
                            self.visit_type(type_);
                        }
                    }
                    _ => (),
                }
            }

            fn walk_typedef(&mut self, typedef: &'ast $($mutability)* Typedef) {
                for extended_attribute in &$($mutability)* typedef.extended_attributes {
                    self.visit_extended_attribute(extended_attribute);
                }

                self.visit_type(&$($mutability)* typedef.type_);
                self.visit_identifier(&$($mutability)* typedef.name);
            }
        }
    }
}

make_visitor!(ImmutableVisitor,);
make_visitor!(MutableVisitor, mut);
