extern crate quote;
extern crate syn;

use utils::*;
use common::collections_prefix;

pub fn derive(ast: &syn::DeriveInput, mut prim_type: Option<syn::Ty>) -> quote::Tokens {

    let stdlib_prefix = collections_prefix();

    let ref name = ast.ident;
    let v = get_unitary_enum(ast);
    //panic!("v: {:?}", v);

    let from_primitive_match: Vec<_> = v.iter().map(|x| {
        let d = x.discriminant;
        let d = syn::Lit::Int(d, syn::IntTy::Unsuffixed);
        let negative = if x.negative {
            quote! { - }
        } else {
            quote! {}
        };

        let n = &x.variant.ident;
        quote! {
            #negative #d => Some(#name::#n)
    }}).collect();

    let to_display_str: Vec<_> = v.iter().map(|x| {
        let n = &x.variant.ident;
        let d = n.as_ref().to_string();
        quote! {
            #name::#n => (#d)
    }}).collect();

    let from_str: Vec<_> = v.iter().map(|x| {
        let n = &x.variant.ident;
        let d = n.as_ref().to_string();
        quote! {
            #d => Some(#name::#n)
    }}).collect();

    let from_str_lower: Vec<_> = v.iter().map(|x| {
        let n = &x.variant.ident;
        let d = n.as_ref().to_string().to_lowercase();
        quote! {
            #d => Some(#name::#n)
    }}).collect();

    let all_variants: Vec<_> = v.iter().map(|x| {
        let n = &x.variant.ident;
        quote! { #name::#n }
    }).collect();
    let all_variants_len = all_variants.len();

    if prim_type.is_none() {
        let min_ty: Vec<_> = v.iter().map(|d| {
            if d.int_ty != syn::IntTy::Isize && d.int_ty != syn::IntTy::Usize && d.int_ty != syn::IntTy::Unsuffixed {
                d.int_ty
            } else {
                if d.negative {
                    let n = d.discriminant as i64;
                    if n < <i32>::min_value() as i64 {
                        syn::IntTy::I64
                    } else {
                        let n = -n;
                        if n < <i16>::min_value() as i64 {
                            syn::IntTy::I32
                        } else if n < <i8>::min_value() as i64 {
                            syn::IntTy::I16
                        } else {
                            syn::IntTy::I8
                        }
                    }
                } else {
                    let n = d.discriminant as u64;
                    if n > <u32>::max_value() as u64 {
                        syn::IntTy::U64
                    } else if n > <u16>::max_value() as u64 {
                        syn::IntTy::U32
                    } else if n > <u8>::max_value() as u64 {
                        syn::IntTy::U16
                    } else {
                        syn::IntTy::U8
                    }
                }
            }
        }).collect();

        // first mention, higher priority
        let priority = [
            syn::IntTy::I64,
            syn::IntTy::I32,
            syn::IntTy::I16,
            syn::IntTy::I8,
            syn::IntTy::U64,
            syn::IntTy::U32,
            syn::IntTy::U16,
            syn::IntTy::U8
        ];
        
        let mut ty = syn::IntTy::U8;
        for t in min_ty {
            if priority.iter().position(|&x| x == t).unwrap() < priority.iter().position(|&x| x == ty).unwrap() {
                ty = t;
            }
        }
        
        let ty_str = match ty {
            syn::IntTy::I64 => "i64",
            syn::IntTy::I32 => "i32",
            syn::IntTy::I16 => "i16",
            syn::IntTy::I8 => "i8",
            syn::IntTy::U64 => "u64",
            syn::IntTy::U32 => "u32",
            syn::IntTy::U16 => "u16",
            syn::IntTy::U8 => "u8",
            _ => panic!("out of bounds ty!")
        };

        prim_type = Some(syn::parse_type(&ty_str).unwrap());
    }    

    let prim_type = prim_type.expect("Unable to detect the primitive type for this enum.");

    let all_variants_const_ident = syn::Ident::from(format!("{}_ALL", to_snake_case(name.as_ref()).to_uppercase() ));
    

    let mut str_format = {
        let to_display_str = to_display_str.clone();
        let all_variants_const_ident = all_variants_const_ident.clone();

        quote! {
            impl ::packed_struct::PrimitiveEnumStaticStr for #name {
                #[inline]
                fn to_display_str(&self) -> &'static str {
                    match *self {
                        #(#to_display_str),*
                    }
                }

                #[inline]
                fn all_variants() -> &'static [Self] {
                    #all_variants_const_ident
                }
            }
        }
    };


    if ::common::alloc_supported() {
        str_format.append(quote! {
            impl ::packed_struct::PrimitiveEnumDynamicStr for #name {
                #[inline]
                fn to_display_str(&self) -> #stdlib_prefix::borrow::Cow<'static, str> {
                    let s = match *self {
                        #(#to_display_str),*
                    };
                    s.into()
                }

                #[inline]
                fn all_variants() -> #stdlib_prefix::borrow::Cow<'static, [Self]> {
                    #stdlib_prefix::borrow::Cow::Borrowed(#all_variants_const_ident)
                }
            }
        });
    };


    quote! {

        const #all_variants_const_ident: &'static [#name; #all_variants_len] = &[ #(#all_variants),* ];

        impl ::packed_struct::PrimitiveEnum for #name {
            type Primitive = #prim_type;

            #[inline]
            fn from_primitive(val: #prim_type) -> Option<Self> {
                match val {
                    #(#from_primitive_match),* ,
                    _ => None
                }
            }

            #[inline]
            fn to_primitive(&self) -> #prim_type {
                *self as #prim_type
            }

            #[inline]
            fn from_str(s: &str) -> Option<Self> {
                match s {
                    #(#from_str),* ,
                    _ => None
                }
            }
            
            #[inline]
            fn from_str_lower(s: &str) -> Option<Self> {
                match s {
                    #(#from_str_lower),* ,
                    _ => None
                }
            }
        }

        #str_format
    }
}

#[derive(Debug)]
struct Variant {
    variant: syn::Variant,
    discriminant: u64,
    negative: bool,
    int_ty: syn::IntTy
}


fn get_unitary_enum(input: &syn::DeriveInput) -> Vec<Variant> {
    match input.body {
        syn::Body::Enum(ref variants) => {
            let mut r = Vec::new();

            let mut d = 0;
            let mut neg = false;

            for variant in variants {
                if variant.data != syn::VariantData::Unit {
                    break;
                }

                let (discriminant, negative, int_ty) = match variant.discriminant {
                    Some(syn::ConstExpr::Lit(syn::Lit::Int(v, int_ty))) => { (v, false, int_ty) },
                    Some(syn::ConstExpr::Unary(syn::UnOp::Neg, ref v)) => {
                        match **v {
                            syn::ConstExpr::Lit(syn::Lit::Int(v, int_ty)) => {
                                (v, true, int_ty)
                            },
                            ref p @ _ => {
                                panic!("Unsupported negated enum const expr: {:?}", p);
                            }
                        }
                    }
                    Some(ref p @ _) => {
                        panic!("Unsupported enum const expr: {:?}", p);
                    },
                    None => {
                        if neg {
                            (d-1, if d-1 == 0 { false } else { true }, syn::IntTy::Unsuffixed)
                        } else {
                            (d+1, false, syn::IntTy::Unsuffixed)
                        }
                    }
                };

                r.push(Variant {
                    variant: variant.clone(),
                    discriminant: discriminant,
                    negative: negative,
                    int_ty: int_ty
                });

                d = discriminant;                
                neg = negative;                
            }
            return r;
        },
        _ => () 
    }

    panic!("Enum's variants must be unitary.");
}