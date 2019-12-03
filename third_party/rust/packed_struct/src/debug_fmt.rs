//! Helper structures for runtime packing visualization.

use internal_prelude::v1::*;

#[cfg(any(feature="alloc", feature="std"))]
pub trait PackedStructDebug {
    fn fmt_fields(&self, fmt: &mut Formatter) -> Result<(), FmtError>;
    fn packed_struct_display_header() -> &'static str;
}

pub struct DebugBinaryByteSlice<'a> {
    pub bits: &'a Range<usize>,
    pub slice: &'a [u8]
}

impl<'a> fmt::Binary for DebugBinaryByteSlice<'a> {
    fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
        for i in self.bits.start..(self.bits.end + 1) {
            let byte = i / 8;
            let bit = i % 8;
            let bit = 7 - bit;

            let src_byte = self.slice[byte];
            let src_bit = (src_byte & (1 << bit)) == (1 << bit);

            let s = if src_bit { "1" } else { "0" };
            try!(fmt.write_str(s));
        }

        Ok(())
    }
}

pub struct DebugBitField<'a> { 
	pub name: Cow<'a, str>,
	pub bits: Range<usize>,
	pub display_value: Cow<'a, str>
}


pub fn packable_fmt_fields(f: &mut Formatter, packed_bytes: &[u8], fields: &[DebugBitField]) -> fmt::Result {
    if fields.len() == 0 {
		return Ok(());
	}

    let max_field_length_name = fields.iter().map(|x| x.name.len()).max().unwrap();
	let max_bit_width = fields.iter().map(|x| x.bits.len()).max().unwrap();

    if max_bit_width > 32 {
        for field in fields {
            try!(write!(f, "{name:>0$} | {base_value:?}\r\n",
                            max_field_length_name + 1,
                            base_value = field.display_value,
                            name = field.name
                            ));
        }
    } else {    
        for field in fields {

            let debug_binary = DebugBinaryByteSlice {
                bits: &field.bits,
                slice: packed_bytes
            };

            try!(write!(f, "{name:>0$} | bits {bits_start:>3}:{bits_end:<3} | 0b{binary_value:>0width_bits$b}{dummy:>0spaces$} | {base_value:?}\r\n",
                            max_field_length_name + 1,
                            base_value = field.display_value,
                            binary_value = debug_binary,
                            dummy = "",
                            bits_start = field.bits.start,
                            bits_end = field.bits.end,
                            width_bits = field.bits.len(),
                            spaces = (max_bit_width - field.bits.len()) as usize,
                            name = field.name
                            ));
        }
    }

    Ok(())
}

pub struct PackedStructDisplay<'a, P: 'a, B: 'a> {
    pub packed_struct: &'a P,
    pub packed_struct_packed: PhantomData<B>,
    pub header: bool,
    pub raw_decimal: bool,
    pub raw_hex: bool,
    pub raw_binary: bool,
    pub fields: bool
}

impl<'a, P, B> PackedStructDisplay<'a, P, B> {
    pub fn new(packed_struct: &'a P) -> Self {
        PackedStructDisplay {
            packed_struct: packed_struct,
            packed_struct_packed: Default::default(),
            
            header: true,
            raw_decimal: true,
            raw_hex: true,
            raw_binary: true,
            fields: true
        }
    }
}

use packing::{PackedStruct, PackedStructSlice};

impl<'a, P, B> fmt::Display for PackedStructDisplay<'a, P, B> where P: PackedStruct<B> + PackedStructSlice + PackedStructDebug {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        match self.packed_struct.pack_to_vec() {
            Ok(packed) => {
                let l = packed.len();

                if self.header {
                    try!(f.write_str(P::packed_struct_display_header()));
                    try!(f.write_str("\r\n"));
                    try!(f.write_str("\r\n"));
                }

                // decimal
                if self.raw_decimal {
                    try!(f.write_str("Decimal\r\n"));
                    try!(f.write_str("["));
                    for i in 0..l {
                        try!(write!(f, "{}", packed[i]));
                        if (i + 1) != l {
                            try!(f.write_str(", "));
                        }
                    }
                    try!(f.write_str("]"));

                    try!(f.write_str("\r\n"));
                    try!(f.write_str("\r\n"));
                }
                                
                // hex
                if self.raw_hex {
                    try!(f.write_str("Hex\r\n"));
                    try!(f.write_str("["));
                    for i in 0..l {
                        try!(write!(f, "0x{:X}", packed[i]));
                        if (i + 1) != l {
                            try!(f.write_str(", "));
                        }
                    }
                    try!(f.write_str("]"));
                    try!(f.write_str("\r\n"));
                    try!(f.write_str("\r\n"));
                }

                if self.raw_binary {
                    try!(f.write_str("Binary\r\n"));
                    try!(f.write_str("["));
                    for i in 0..l {
                        try!(write!(f, "0b{:08b}", packed[i]));
                        if (i + 1) != l {
                            try!(f.write_str(", "));
                        }
                    }
                    try!(f.write_str("]"));
                    try!(f.write_str("\r\n"));
                    try!(f.write_str("\r\n"));
                }

                if self.fields {
                    try!(self.packed_struct.fmt_fields(f));
                }                
            },
            Err(e) => {
                write!(f, "Error packing for display: {:?}", e)?;
            }
        }

        Ok(())
    }
}
