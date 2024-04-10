use scroll::{ctx, Endian, Pread, BE};

#[derive(Debug)]
struct Data<'a> {
    name: &'a str,
    id: u32,
}

impl<'a> ctx::TryFromCtx<'a, Endian> for Data<'a> {
    type Error = scroll::Error;
    fn try_from_ctx(src: &'a [u8], endian: Endian) -> Result<(Self, usize), Self::Error> {
        let name = src.pread::<&'a str>(0)?;
        let id = src.pread_with(name.len() + 1, endian)?;
        Ok((Data { name: name, id: id }, name.len() + 4))
    }
}

fn main() {
    let bytes = b"UserName\x00\x01\x02\x03\x04";
    let data = bytes.pread_with::<Data>(0, BE).unwrap();
    assert_eq!(data.id, 0x01020304);
    assert_eq!(data.name.to_string(), "UserName".to_string());
    println!("Data: {:?}", &data);
}
