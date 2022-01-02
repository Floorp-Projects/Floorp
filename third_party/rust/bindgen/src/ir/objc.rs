//! Objective C types

use super::context::{BindgenContext, ItemId};
use super::function::FunctionSig;
use super::item::Item;
use super::traversal::{Trace, Tracer};
use super::ty::TypeKind;
use crate::clang;
use crate::parse::ClangItemParser;
use clang_sys::CXChildVisit_Continue;
use clang_sys::CXCursor_ObjCCategoryDecl;
use clang_sys::CXCursor_ObjCClassMethodDecl;
use clang_sys::CXCursor_ObjCClassRef;
use clang_sys::CXCursor_ObjCInstanceMethodDecl;
use clang_sys::CXCursor_ObjCProtocolDecl;
use clang_sys::CXCursor_ObjCProtocolRef;
use clang_sys::CXCursor_ObjCSuperClassRef;
use clang_sys::CXCursor_TemplateTypeParameter;
use proc_macro2::{Ident, Span, TokenStream};

/// Objective C interface as used in TypeKind
///
/// Also protocols and categories are parsed as this type
#[derive(Debug)]
pub struct ObjCInterface {
    /// The name
    /// like, NSObject
    name: String,

    category: Option<String>,

    is_protocol: bool,

    /// The list of template names almost always, ObjectType or KeyType
    pub template_names: Vec<String>,

    /// The list of protocols that this interface conforms to.
    pub conforms_to: Vec<ItemId>,

    /// The direct parent for this interface.
    pub parent_class: Option<ItemId>,

    /// List of the methods defined in this interfae
    methods: Vec<ObjCMethod>,

    class_methods: Vec<ObjCMethod>,
}

/// The objective c methods
#[derive(Debug)]
pub struct ObjCMethod {
    /// The original method selector name
    /// like, dataWithBytes:length:
    name: String,

    /// Method name as converted to rust
    /// like, dataWithBytes_length_
    rust_name: String,

    signature: FunctionSig,

    /// Is class method?
    is_class_method: bool,
}

impl ObjCInterface {
    fn new(name: &str) -> ObjCInterface {
        ObjCInterface {
            name: name.to_owned(),
            category: None,
            is_protocol: false,
            template_names: Vec::new(),
            parent_class: None,
            conforms_to: Vec::new(),
            methods: Vec::new(),
            class_methods: Vec::new(),
        }
    }

    /// The name
    /// like, NSObject
    pub fn name(&self) -> &str {
        self.name.as_ref()
    }

    /// Formats the name for rust
    /// Can be like NSObject, but with categories might be like NSObject_NSCoderMethods
    /// and protocols are like PNSObject
    pub fn rust_name(&self) -> String {
        if let Some(ref cat) = self.category {
            format!("{}_{}", self.name(), cat)
        } else {
            if self.is_protocol {
                format!("P{}", self.name())
            } else {
                format!("I{}", self.name().to_owned())
            }
        }
    }

    /// Is this a template interface?
    pub fn is_template(&self) -> bool {
        !self.template_names.is_empty()
    }

    /// List of the methods defined in this interface
    pub fn methods(&self) -> &Vec<ObjCMethod> {
        &self.methods
    }

    /// Is this a protocol?
    pub fn is_protocol(&self) -> bool {
        self.is_protocol
    }

    /// Is this a category?
    pub fn is_category(&self) -> bool {
        self.category.is_some()
    }

    /// List of the class methods defined in this interface
    pub fn class_methods(&self) -> &Vec<ObjCMethod> {
        &self.class_methods
    }

    /// Parses the Objective C interface from the cursor
    pub fn from_ty(
        cursor: &clang::Cursor,
        ctx: &mut BindgenContext,
    ) -> Option<Self> {
        let name = cursor.spelling();
        let mut interface = Self::new(&name);

        if cursor.kind() == CXCursor_ObjCProtocolDecl {
            interface.is_protocol = true;
        }

        cursor.visit(|c| {
            match c.kind() {
                CXCursor_ObjCClassRef => {
                    if cursor.kind() == CXCursor_ObjCCategoryDecl {
                        // We are actually a category extension, and we found the reference
                        // to the original interface, so name this interface approriately
                        interface.name = c.spelling();
                        interface.category = Some(cursor.spelling());
                    }
                }
                CXCursor_ObjCProtocolRef => {
                    // Gather protocols this interface conforms to
                    let needle = format!("P{}", c.spelling());
                    let items_map = ctx.items();
                    debug!("Interface {} conforms to {}, find the item", interface.name, needle);

                    for (id, item) in items_map
                    {
                        if let Some(ty) = item.as_type() {
                            match *ty.kind() {
                                TypeKind::ObjCInterface(ref protocol) => {
                                    if protocol.is_protocol
                                    {
                                        debug!("Checking protocol {}, ty.name {:?}", protocol.name, ty.name());
                                        if Some(needle.as_ref()) == ty.name() {
                                            debug!("Found conforming protocol {:?}", item);
                                            interface.conforms_to.push(id);
                                            break;
                                        }
                                    }
                                }
                                _ => {}
                            }
                        }
                    }

                }
                CXCursor_ObjCInstanceMethodDecl |
                CXCursor_ObjCClassMethodDecl => {
                    let name = c.spelling();
                    let signature =
                        FunctionSig::from_ty(&c.cur_type(), &c, ctx)
                            .expect("Invalid function sig");
                    let is_class_method = c.kind() == CXCursor_ObjCClassMethodDecl;
                    let method = ObjCMethod::new(&name, signature, is_class_method);
                    interface.add_method(method);
                }
                CXCursor_TemplateTypeParameter => {
                    let name = c.spelling();
                    interface.template_names.push(name);
                }
                CXCursor_ObjCSuperClassRef => {
                    let item = Item::from_ty_or_ref(c.cur_type(), c, None, ctx);
                    interface.parent_class = Some(item.into());
                },
                _ => {}
            }
            CXChildVisit_Continue
        });
        Some(interface)
    }

    fn add_method(&mut self, method: ObjCMethod) {
        if method.is_class_method {
            self.class_methods.push(method);
        } else {
            self.methods.push(method);
        }
    }
}

impl ObjCMethod {
    fn new(
        name: &str,
        signature: FunctionSig,
        is_class_method: bool,
    ) -> ObjCMethod {
        let split_name: Vec<&str> = name.split(':').collect();

        let rust_name = split_name.join("_");

        ObjCMethod {
            name: name.to_owned(),
            rust_name: rust_name.to_owned(),
            signature,
            is_class_method,
        }
    }

    /// The original method selector name
    /// like, dataWithBytes:length:
    pub fn name(&self) -> &str {
        self.name.as_ref()
    }

    /// Method name as converted to rust
    /// like, dataWithBytes_length_
    pub fn rust_name(&self) -> &str {
        self.rust_name.as_ref()
    }

    /// Returns the methods signature as FunctionSig
    pub fn signature(&self) -> &FunctionSig {
        &self.signature
    }

    /// Is this a class method?
    pub fn is_class_method(&self) -> bool {
        self.is_class_method
    }

    /// Formats the method call
    pub fn format_method_call(&self, args: &[TokenStream]) -> TokenStream {
        let split_name: Vec<Option<Ident>> = self
            .name
            .split(':')
            .map(|name| {
                if name.is_empty() {
                    None
                } else {
                    Some(Ident::new(name, Span::call_site()))
                }
            })
            .collect();

        // No arguments
        if args.len() == 0 && split_name.len() == 1 {
            let name = &split_name[0];
            return quote! {
                #name
            };
        }

        // Check right amount of arguments
        if args.len() != split_name.len() - 1 {
            panic!(
                "Incorrect method name or arguments for objc method, {:?} vs {:?}",
                args,
                split_name,
            );
        }

        // Get arguments without type signatures to pass to `msg_send!`
        let mut args_without_types = vec![];
        for arg in args.iter() {
            let arg = arg.to_string();
            let name_and_sig: Vec<&str> = arg.split(' ').collect();
            let name = name_and_sig[0];
            args_without_types.push(Ident::new(name, Span::call_site()))
        }

        let args = split_name.into_iter().zip(args_without_types).map(
            |(arg, arg_val)| {
                if let Some(arg) = arg {
                    quote! { #arg: #arg_val }
                } else {
                    quote! { #arg_val: #arg_val }
                }
            },
        );

        quote! {
            #( #args )*
        }
    }
}

impl Trace for ObjCInterface {
    type Extra = ();

    fn trace<T>(&self, context: &BindgenContext, tracer: &mut T, _: &())
    where
        T: Tracer,
    {
        for method in &self.methods {
            method.signature.trace(context, tracer, &());
        }

        for class_method in &self.class_methods {
            class_method.signature.trace(context, tracer, &());
        }

        for protocol in &self.conforms_to {
            tracer.visit(*protocol);
        }
    }
}
