extern crate quote;
extern crate syn;

use pack::*;
use pack_parse_attributes::*;

use utils::*;

use std::ops::Range;

pub fn parse_sub_attributes(attributes: &Vec<syn::Attribute>, main_attribute: &str) -> Vec<(String, String)> {
    let mut r = vec![];

    for attr in attributes {        
        if let &syn::Attribute { value: syn::MetaItem::List(ref ident, ref list), .. } = attr {
            if ident.as_ref() != main_attribute { continue; }

            for item in list {
                if let &syn::NestedMetaItem::MetaItem(syn::MetaItem::NameValue(ref ident, ref lit)) = item {
                    let n = ident.as_ref();
                    
                    if let &syn::Lit::Str(ref v, _) = lit {
                        r.push((n.to_string(), v.to_string()));
                    }
                }
            }
        }
    }

    r
}


#[derive(Clone, Copy, Debug, PartialEq)]
/// https://en.wikipedia.org/wiki/Bit_numbering
pub enum BitNumbering {
    Lsb0,
    Msb0
}

impl BitNumbering {
    pub fn from_str(s: &str) -> Option<Self> {
        let s = s.to_lowercase();
        match s.as_str() {
            "lsb0" => Some(BitNumbering::Lsb0),
            "msb0" => Some(BitNumbering::Msb0),
            _ => None
        }
    }
}


#[derive(Clone, Copy, Debug)]
/// https://en.wikipedia.org/wiki/Endianness
pub enum IntegerEndianness {
    Msb,
    Lsb
}

impl IntegerEndianness {
    pub fn from_str(s: &str) -> Option<Self> {
        let s = s.to_lowercase();
        match s.as_str() {
            "lsb" | "le" => Some(IntegerEndianness::Lsb),
            "msb" | "be" => Some(IntegerEndianness::Msb),
            _ => None
        }
    }
}


fn get_builtin_type_bit_width(p: &syn::PathSegment) -> Option<usize> {

    match p.ident.as_ref() {
        "bool" => Some(1),
        "u8" | "i8" => Some(8),
        "u16" | "i16" => Some(16),
        "u32" | "i32" => Some(32),
        "u64" | "i64" => Some(64),
        "ReservedZero" | "ReservedZeroes" | "ReservedOne" | "ReservedOnes" |
        "Integer" => {
            match p.parameters {
                ::syn::PathParameters::AngleBracketed(ref params) => {
                    for t in &params.types {
                        let b = syn_to_string(t);

                        if let Some(bits_pos) = b.find("Bits") {
                            let possible_int = &b[(bits_pos + 4)..];
                            if let Ok(bits) = possible_int.parse::<usize>() {
                                return Some(bits);
                            }
                        }
                    }

                    None
                },
                _ => None
            }
        },
        _ => {
            None
        }
    }
}


fn get_field_mid_positioning(field: &syn::Field) -> FieldMidPositioning {
    
    let mut array_size = 1;
    let bit_width_builtin: Option<usize>;

    let _ty = match field.ty {
        syn::Ty::Path (None, syn::Path { ref segments, .. }) => {
            if segments.len() == 1 {                
                let ref segment = segments[0];

                bit_width_builtin = get_builtin_type_bit_width(segment);
                segment.clone()
            } else {
                panic!("Unsupported path type: {:#?}", field.ty);
            }
        },
        syn::Ty::Array(ref ty, ref size) => {
        
            if let syn::Ty::Path (None, syn::Path { ref segments, .. }) = **ty {
                if segments.len() == 1 {
                    if let &syn::ConstExpr::Lit(syn::Lit::Int(size, _)) = size {
                        let ref segment = segments[0];
                        bit_width_builtin = get_builtin_type_bit_width(segment);
                        array_size = size as usize;

                        if size == 0 { panic!("Arrays sized 0 are not supported."); }
                        
                        segment.clone()
                    } else {
                        panic!("unsupported array size: {:?}", size);
                    }
                } else {
                    panic!("Unsupported path type: {:#?}", ty);
                }
            } else {
                panic!("Unsupported path type: {:#?}", ty);
            }

        },
        _ => { panic!("Unsupported type: {:?}", field.ty); }
    };

    let field_attributes = PackFieldAttribute::parse_all(&parse_sub_attributes(&field.attrs, "packed_field"));

    let bits_position = field_attributes.iter().filter_map(|a| match a {
        &PackFieldAttribute::BitPosition(b) | &PackFieldAttribute::BytePosition(b) => Some(b),
        _ => None
    }).next().unwrap_or(BitsPositionParsed::Next);
    
    let bit_width = if let Some(bits) = field_attributes.iter().filter_map(|a| if let &PackFieldAttribute::SizeBits(bits) = a { Some(bits) } else { None }).next() {
        if array_size > 1 { panic!("Please use the 'element_size_bits' or 'element_size_bytes' for arrays."); }
        bits
    } else if let Some(bits) = field_attributes.iter().filter_map(|a| if let &PackFieldAttribute::ElementSizeBits(bits) = a { Some(bits) } else { None }).next() {
        bits * array_size
    } else if let BitsPositionParsed::Range(a, b) = bits_position {
        (b as isize - a as isize).abs() as usize + 1
    } else if let Some(bit_width_builtin) = bit_width_builtin {
        // todo: is it even possible to hit this branch?
        bit_width_builtin * array_size
    } else {
        panic!("Couldn't determine the width of this field: {:?}", field);
    };

    FieldMidPositioning {
        bit_width: bit_width,
        bits_position: bits_position
    }
}


fn parse_field(field: &syn::Field, mp: &FieldMidPositioning, bit_range: &Range<usize>, default_endianness: Option<IntegerEndianness>) -> FieldKind {
    match field.ty {
        syn::Ty::Path (None, syn::Path { ref segments, .. }) => {
            if segments.len() == 1 {                
                
                let ty = syn::parse_type(&syn_to_string(&segments[0])).expect("error parsing path segment to ty");
                                    
                return FieldKind::Regular {
                    ident: field.ident.clone().expect("mah ident?"),
                    field: parse_reg_field(field, &ty, bit_range, default_endianness)
                };

            } else {
                panic!("huh 1x");
            }
        },
        syn::Ty::Array(ref ty, ref size) => {
        
            if let syn::Ty::Path (None, syn::Path { ref segments, .. }) = **ty {
                if segments.len() == 1 {
                    if let &syn::ConstExpr::Lit(syn::Lit::Int(size, _)) = size {
                        let ty = syn::parse_type(&syn_to_string(&segments[0])).expect("error parsing path segment to ty");
                                                
                        let element_size_bits: usize = mp.bit_width as usize / size as usize;
                        if (mp.bit_width % element_size_bits) != 0 {
                            panic!("element and array size mismatch!");
                        }

                        let mut elements = vec![];
                        for i in 0..size as usize {
                            let s = bit_range.start + (i * element_size_bits);
                            let element_bit_range = s..(s + element_size_bits - 1);
                            elements.push(parse_reg_field(field, &ty, &element_bit_range, default_endianness));
                            //panic!("field: {:#?}, mp: {:#?}, bit_range: {:#?}", field, mp, bit_range);
                        }
                        
                        return FieldKind::Array {
                            ident: field.ident.clone().expect("mah ident?"),
                            size: size as usize,
                            elements: elements
                        };
                    }
                }
            }
        },
        _ => {  }
    };

    panic!("Field not supported: {:?}", field);
}

fn parse_reg_field(field: &syn::Field, ty: &syn::Ty, bit_range: &Range<usize>, default_endianness: Option<IntegerEndianness>) -> FieldRegular {
    let mut wrappers = vec![];

    let bit_width = (bit_range.end - bit_range.start) + 1;
    let ty_str = syn_to_string(ty);
    let field_attributes = PackFieldAttribute::parse_all(&parse_sub_attributes(&field.attrs, "packed_field"));

    let is_enum_ty = field_attributes.iter().filter_map(|a| match a {
        &PackFieldAttribute::Ty(TyKind::Enum) => Some(()),
        _ => None
    }).next().is_some();    

    let needs_int_wrap = {
        let int_types = ["u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64"];
        is_enum_ty || int_types.iter().any(|t| t == &ty_str)
    };

    let needs_endiannes_wrap = {
        let our_int_ty = ty_str.starts_with("Integer < ") && ty_str.contains("Bits");
        our_int_ty || needs_int_wrap
    };
    
    if is_enum_ty {
        wrappers.push(SerializationWrapper::PrimitiveEnumWrapper);
    }

    if needs_int_wrap {
        let ty = if is_enum_ty {
            format!("<{} as PrimitiveEnum>::Primitive",syn_to_string(ty))
        } else {
            ty_str.clone()
        };
        let integer_wrap_ty = syn::parse_type(&format!("Integer<{}, Bits{}>", ty, bit_width)).unwrap();
        wrappers.push(SerializationWrapper::IntegerWrapper { integer: integer_wrap_ty });
    }

    if needs_endiannes_wrap {
        let mut endiannes = if let Some(endiannes) = field_attributes
            .iter()
            .filter_map(|a| if let &PackFieldAttribute::IntEndiannes(endiannes) = a {
                                Some(endiannes)
                            } else {
                                None
                            }).next()
        {
            Some(endiannes)
        } else {
            default_endianness
        };

        if bit_width <= 8 {
            endiannes = Some(IntegerEndianness::Msb);
        }

        if endiannes.is_none() {
            panic!("Missing serialization wrapper for simple type {:?} - did you specify the integer endiannes on the field or a default for the struct?", ty_str);
        }

        let ty_prefix = match endiannes.unwrap() {
            IntegerEndianness::Msb => "Msb",
            IntegerEndianness::Lsb => "Lsb"
        };

        let endiannes_wrap_ty = syn::parse_type(&format!("{}Integer", ty_prefix)).unwrap();
        wrappers.push(SerializationWrapper::EndiannesWrapper { endian: endiannes_wrap_ty });
    }

    FieldRegular {
        ty: ty.clone(),
        serialization_wrappers: wrappers,
        bit_width: bit_width,
        bit_range: bit_range.clone(),
        bit_range_rust: bit_range.start..(bit_range.end + 1)
    }
}



#[derive(Copy, Clone, Debug, PartialEq)]
pub enum BitsPositionParsed {
    Next,
    Start(usize),
    Range(usize, usize)
}

impl BitsPositionParsed {
    fn to_bits_position(&self) -> Box<BitsRange> {
        match *self {
            BitsPositionParsed::Next => Box::new(NextBits),
            BitsPositionParsed::Start(s) => Box::new(s),
            BitsPositionParsed::Range(a, b) => Box::new(a..b)
        }
    }

    pub fn range_in_order(a: usize, b: usize) -> Self {
        BitsPositionParsed::Range(::std::cmp::min(a, b), ::std::cmp::max(a, b))
    }
}



pub fn parse_num(s: &str) -> usize {
    let s = s.trim();

    if s.starts_with("0x") || s.starts_with("0X") {
        usize::from_str_radix(&s[2..], 16).expect(&format!("Invalid hex number: {:?}", s))
    } else {
        s.parse().expect(&format!("Invalid decimal number: {:?}", s))
    }
}



pub fn parse_struct(ast: &syn::MacroInput) -> PackStruct {
    let attributes = PackStructAttribute::parse_all(&parse_sub_attributes(&ast.attrs, "packed_struct"));

    let fields: Vec<_> = match ast.body {
        syn::Body::Struct(syn::VariantData::Struct(ref fields)) => {
            fields.iter().collect()
        },
        _ => panic!("#[derive(PackedStruct)] can only be used with braced structs"),
    };

    if ast.generics.ty_params.len() > 0 {
        panic!("Structures with generic fields currently aren't supported.");
    }

    let bit_positioning = {
        attributes.iter().filter_map(|a| match a {
            &PackStructAttribute::BitNumbering(b) => Some(b),
            _ => None
        }).next()
    };

    let default_int_endianness = attributes.iter().filter_map(|a| match a {
        &PackStructAttribute::DefaultIntEndianness(i) => Some(i),
        _ => None
    }).next();

    let struct_size_bytes = attributes.iter().filter_map(|a| {
        if let &PackStructAttribute::SizeBytes(size_bytes) = a {
            Some(size_bytes)
        } else {
            None
        }}).next();



    let first_field_is_auto_positioned = {
        if let Some(ref field) = fields.first() {
            let mp = get_field_mid_positioning(field);
            mp.bits_position == BitsPositionParsed::Next
        } else {
            false
        }
    };


    let mut fields_parsed: Vec<FieldKind> = vec![];
    {
        let mut prev_bit_range = None;
        for field in &fields {
            let mp = get_field_mid_positioning(field);
            let bits_position = match (bit_positioning, mp.bits_position) {
                (Some(BitNumbering::Lsb0), BitsPositionParsed::Next) | (Some(BitNumbering::Lsb0), BitsPositionParsed::Start(_)) => {
                    panic!("LSB0 field positioning currently requires explicit, full field positions.");
                },
                (Some(BitNumbering::Lsb0), BitsPositionParsed::Range(start, end)) => {
                    if let Some(struct_size_bytes) = struct_size_bytes {
                        BitsPositionParsed::range_in_order( (struct_size_bytes * 8) - 1 - start, (struct_size_bytes * 8) - 1 - end )
                    } else {
                        panic!("LSB0 field positioning currently requires explicit struct byte size.");
                    }
                },

                (None, p @ BitsPositionParsed::Next) => p,
                (Some(BitNumbering::Msb0), p) => p,

                (None, _) => {
                    panic!("Please explicitly specify the bit numbering mode on the struct with an attribute: #[packed_struct(bit_numbering=\"msb0\")] or \"lsb0\".");
                }
            };
            let bit_range = bits_position.to_bits_position().get_bits_range(mp.bit_width, &prev_bit_range);

            fields_parsed.push(parse_field(field, &mp, &bit_range, default_int_endianness));

            prev_bit_range = Some(bit_range);
        }        
    }

    let num_bits: usize = {
        if let Some(struct_size_bytes) = struct_size_bytes {
            struct_size_bytes * 8
        } else {
            let last_bit = fields_parsed.iter().map(|f| match f {
                &FieldKind::Regular { ref field, .. } => field.bit_range_rust.end,
                &FieldKind::Array { ref elements, .. } => elements.last().unwrap().bit_range_rust.end
            }).max().unwrap();
            last_bit
        }
    };

    let num_bytes = (num_bits as f32 / 8.0).ceil() as usize;

    if first_field_is_auto_positioned && (num_bits % 8) != 0 && struct_size_bytes == None {
        panic!("Please explicitly position the bits of the first field of this structure ({}), as alignment isn't obvious to the end user.", ast.ident);
    }

    // check for overlaps
    {
        let mut bits = vec![None; num_bytes * 8];
        for field in &fields_parsed {
            let mut find_overlaps = |name: String, range: &Range<usize>| {
                for i in range.start .. (range.end+1) {
                    if let Some(&Some(ref n)) = bits.get(i) {
                        panic!("Overlap in bits between fields {} and {}", n, name);
                    }

                    bits[i] = Some(name.clone());
                }
            };

            match field {
                &FieldKind::Regular { ref field, ref ident } => {
                    find_overlaps(syn_to_string(ident), &field.bit_range);
                },
                &FieldKind::Array { ref ident, ref elements, .. } => {
                    for (i, field) in elements.iter().enumerate() {
                        find_overlaps(format!("{}[{}]", syn_to_string(ident), i), &field.bit_range);
                    }
                }
            }
        }
    }
        
    PackStruct {
        ast: ast.clone(),
        fields: fields_parsed,
        num_bytes: num_bytes,
        num_bits: num_bits
    }
}


pub fn syn_to_string<T: ::quote::ToTokens>(thing: &T) -> String {
    syn_to_tokens(thing).as_str().into()
}

pub fn append_to_tokens<T: ::quote::ToTokens>(thing: &T, tokens: &mut ::quote::Tokens) {
    thing.to_tokens(tokens)
}

pub fn syn_to_tokens<T: ::quote::ToTokens>(thing: &T) -> quote::Tokens {
    let mut t = ::quote::Tokens::new();
    append_to_tokens(thing, &mut t);
    t
}