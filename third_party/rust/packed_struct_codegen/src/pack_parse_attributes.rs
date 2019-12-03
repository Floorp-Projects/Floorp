use pack_parse::*;

#[derive(Clone, Copy)]
pub enum PackStructAttributeKind {
    SizeBytes,
    //SizeBits,
    DefaultIntEndianness,
    BitNumbering
}

impl PackStructAttributeKind {
    fn get_attr_name(&self) -> &'static str {
        use self::PackStructAttributeKind::*;

        match *self {
            SizeBytes => "size_bytes",
            //SizeBits => "size_bits",
            DefaultIntEndianness => "endian",
            BitNumbering => "bit_numbering"
        }
    }
}

pub enum PackStructAttribute {
    SizeBytes(usize),
    //SizeBits(usize),
    DefaultIntEndianness(IntegerEndianness),
    BitNumbering(BitNumbering)
}

impl PackStructAttribute {
    pub fn parse(name: &str, val: &str) -> Result<Self, ()> {
        if name == PackStructAttributeKind::DefaultIntEndianness.get_attr_name() {
            return Ok(PackStructAttribute::DefaultIntEndianness(IntegerEndianness::from_str(val).expect(&format!("Invalid default int endian value: {}", val))));
        }

        if name == PackStructAttributeKind::BitNumbering.get_attr_name() {
            let b = BitNumbering::from_str(val).expect("Invalid bit numbering attribute value");
            return Ok(PackStructAttribute::BitNumbering(b));
        }

        if name == PackStructAttributeKind::SizeBytes.get_attr_name() {
            let b = parse_num(val);
            return Ok(PackStructAttribute::SizeBytes(b));
        }

        /*
        if name == PackStructAttributeKind::SizeBits.get_attr_name() {
            let b = parse_num(val);
            return Ok(PackStructAttribute::SizeBits(b));
        }
        */

        Err(())
    }

    pub fn parse_all(attributes: &Vec<(String, String)>) -> Vec<Self> {
        let mut r = vec![];
        for &(ref name, ref val) in attributes {
            if let Ok(attr) = Self::parse(name, val) {
                r.push(attr)
            }
        }
        r
    }    
}

#[derive(Clone, Copy)]
pub enum PackFieldAttributeKind {
    IntEndiannes,
    BitPosition,
    BytePosition,
    ElementSizeBytes,
    ElementSizeBits,
    SizeBytes,
    SizeBits,
    Ty
}

impl PackFieldAttributeKind {
    fn get_attr_name(&self) -> &'static str {
        use self::PackFieldAttributeKind::*;

        match *self {
            IntEndiannes => "endian",
            BitPosition => "bits",
            BytePosition => "bytes",
            SizeBytes => "size_bytes",
            SizeBits => "size_bits",
            ElementSizeBytes => "element_size_bytes",
            ElementSizeBits => "element_size_bits",
            Ty => "ty"
        }
    }
}

pub enum PackFieldAttribute {
    IntEndiannes(IntegerEndianness),
    BitPosition(BitsPositionParsed),
    BytePosition(BitsPositionParsed),
    SizeBits(usize),
    ElementSizeBits(usize),
    Ty(TyKind)
}

pub enum TyKind {
    Enum
}

impl PackFieldAttribute {
    pub fn parse(name: &str, val: &str) -> Result<Self, ()> {
        if name == PackFieldAttributeKind::IntEndiannes.get_attr_name() {            
            return Ok(PackFieldAttribute::IntEndiannes(IntegerEndianness::from_str(val).unwrap()));
        }

        if name == PackFieldAttributeKind::BitPosition.get_attr_name() {
            let b = parse_position_val(val, 1);
            return Ok(PackFieldAttribute::BitPosition(b));
        }

        if name == PackFieldAttributeKind::BytePosition.get_attr_name() {
            let b = parse_position_val(val, 8);
            return Ok(PackFieldAttribute::BytePosition(b));
        }

        if name == PackFieldAttributeKind::SizeBytes.get_attr_name() {
            let b = parse_num(val);
            return Ok(PackFieldAttribute::SizeBits(b * 8));
        }

        if name == PackFieldAttributeKind::SizeBits.get_attr_name() {
            let b = parse_num(val);
            return Ok(PackFieldAttribute::SizeBits(b));
        }

        if name == PackFieldAttributeKind::ElementSizeBytes.get_attr_name() {
            let b = parse_num(val);
            return Ok(PackFieldAttribute::ElementSizeBits(b * 8));
        }

        if name == PackFieldAttributeKind::ElementSizeBits.get_attr_name() {
            let b = parse_num(val);
            return Ok(PackFieldAttribute::ElementSizeBits(b));
        }

        if name == PackFieldAttributeKind::Ty.get_attr_name() {
            match val {
                "enum" => { return Ok(PackFieldAttribute::Ty(TyKind::Enum)); },
                _ => ()
            }
        }

        Err(())
    }

    pub fn parse_all(attributes: &Vec<(String, String)>) -> Vec<Self> {
        let mut r = vec![];
        for &(ref name, ref val) in attributes {
            if let Ok(attr) = Self::parse(name, val) {
                r.push(attr)
            }
        }
        r
    }
}


/// Supported formats:
/// 
/// Single bit
/// 0
///
/// Open ended, start bit:
/// 0..
/// 0:
/// 
/// Inclusive range
/// 0:1
/// 0..=1
/// 
/// Exclusive range
/// 0..2
/// 
/// Returns: INCLUSIVE range
pub fn parse_position_val(v: &str, multiplier: usize) -> BitsPositionParsed {
    let v = v.trim();
    if v.ends_with("..") {
        let v = v.replace("..", "");
        let n = parse_num(&v);
        return BitsPositionParsed::Start(n * multiplier);
    } else if v.ends_with(":") {
        let v = v.replace(":", "");
        let n = parse_num(&v);
        return BitsPositionParsed::Start(n * multiplier);
    } else if v.contains(":") || v.contains("..=") {
        // inclusive

        let s: Vec<_> = {
            if v.contains(":") {
                v.split(":").collect()
            } else {
                v.split("..=").collect()
            }
        };

        if s.len() == 2 {
            let start = parse_num(s[0]);
            let end = parse_num(s[1]);
            if multiplier > 1 {
                return BitsPositionParsed::range_in_order(start * multiplier, ((end+1) * multiplier)-1);
            } else {
                return BitsPositionParsed::range_in_order(start, end);
            }
        }

    } else if v.contains("..") {
        // exclusive

        let s: Vec<_> = v.split("..").collect();
        if s.len() == 2 {
            let start = parse_num(s[0]);
            let end = parse_num(s[1]);
            if end == 0 {
                panic!("Ending cannot be 0 for exclusive ranges.");
            }
            
            if multiplier > 1 {
                return BitsPositionParsed::range_in_order(start * multiplier, ((end-1) * multiplier)-1);
            } else {
                return BitsPositionParsed::range_in_order(start, end - 1);
            }
        }
    } else {
        // single bit

        let start = parse_num(v);
        if multiplier > 1 {            
            return BitsPositionParsed::Range(start * multiplier, ((start+1) * multiplier)-1);
        } else {
            return BitsPositionParsed::Range(start, start);
        }
    }

    panic!("Invalid bits position. Tried to parse: '{}'", v);
}


#[test]
fn test_parse_position_val() {
    {
        assert_eq!(BitsPositionParsed::Range(1, 1), parse_position_val("1", 1));
        assert_eq!(BitsPositionParsed::Range(8, 15), parse_position_val("1", 8));
        assert_eq!(BitsPositionParsed::Range(0, 7), parse_position_val("0", 8));
    }

    {
        assert_eq!(BitsPositionParsed::Start(1), parse_position_val("1..", 1));
        assert_eq!(BitsPositionParsed::Start(1), parse_position_val("1:", 1));
    }

    {
        assert_eq!(BitsPositionParsed::Range(1, 2), parse_position_val("1:2", 1));
        assert_eq!(BitsPositionParsed::Range(8, 23), parse_position_val("1:2", 8));        
        assert_eq!(BitsPositionParsed::Range(0, 15), parse_position_val("0:1", 8));
        assert_eq!(BitsPositionParsed::Range(1, 2), parse_position_val("1..=2", 1));
    }

    {
        assert_eq!(BitsPositionParsed::Range(1, 2), parse_position_val("1..3", 1));
        assert_eq!(BitsPositionParsed::Range(8, 15), parse_position_val("1..3", 8));
    }

    {
        assert_eq!(BitsPositionParsed::Range(0, 7), parse_position_val("0", 8));
        assert_eq!(BitsPositionParsed::Range(8, 39), parse_position_val("1:4", 8));
        assert_eq!(BitsPositionParsed::Start(40), parse_position_val("5..", 8));
    }
}
