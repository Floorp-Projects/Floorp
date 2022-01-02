use super::*;

/// A custom section holding arbitrary data.
#[derive(Clone, Debug)]
pub struct CustomSection<'a> {
    /// The name of this custom section.
    pub name: &'a str,
    /// This custom section's data.
    pub data: &'a [u8],
}

impl Section for CustomSection<'_> {
    fn id(&self) -> u8 {
        SectionId::Custom.into()
    }

    fn encode<S>(&self, sink: &mut S)
    where
        S: Extend<u8>,
    {
        let name_len = encoders::u32(u32::try_from(self.name.len()).unwrap());
        let n = name_len.len();

        sink.extend(
            encoders::u32(u32::try_from(n + self.name.len() + self.data.len()).unwrap())
                .chain(name_len)
                .chain(self.name.as_bytes().iter().copied())
                .chain(self.data.iter().copied()),
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_custom_section() {
        let custom = CustomSection {
            name: "test",
            data: &[11, 22, 33, 44],
        };

        let mut encoded = vec![];
        custom.encode(&mut encoded);

        #[rustfmt::skip]
        assert_eq!(encoded, vec![
            // LEB128 length of section.
            9,
            // LEB128 length of name.
            4,
            // Name.
            b't', b'e', b's', b't',
            // Data.
            11, 22, 33, 44,
        ]);
    }
}
