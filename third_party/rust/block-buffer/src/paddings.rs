use byte_tools::{zero, set};

/// Trait for padding messages divided into blocks
pub trait Padding {
    /// Pads `block` filled with data up to `pos`
    fn pad(block: &mut [u8], pos: usize);
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
/// Error for indicating failed unpadding process
pub struct UnpadError;

/// Trait for extracting oringinal message from padded medium
pub trait Unpadding {
    /// Unpad given `data` by truncating it according to the used padding.
    /// In case of the malformed padding will return `UnpadError`
    fn unpad(data: &[u8]) -> Result<&[u8], UnpadError>;
}


#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum ZeroPadding{}

impl Padding for ZeroPadding {
    #[inline]
    fn pad(block: &mut [u8], pos: usize) {
        zero(&mut block[pos..])
    }
}

impl Unpadding for ZeroPadding {
    #[inline]
    fn unpad(data: &[u8]) -> Result<&[u8], UnpadError> {
        let mut n = data.len() - 1;
        while n != 0 {
            if data[n] != 0 {
                break;
            }
            n -= 1;
        }
        Ok(&data[..n+1])
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum Pkcs7{}

impl Padding for Pkcs7 {
    #[inline]
    fn pad(block: &mut [u8], pos: usize) {
        let n = block.len() - pos;
        set(&mut block[pos..], n as u8);
    }
}

impl Unpadding for Pkcs7 {
    #[inline]
    fn unpad(data: &[u8]) -> Result<&[u8], UnpadError> {
        if data.is_empty() { return Err(UnpadError); }
        let l = data.len();
        let n = data[l-1];
        if n == 0 {
            return Err(UnpadError)
        }
        for v in &data[l-n as usize..l-1] {
            if *v != n { return Err(UnpadError); }
        }
        Ok(&data[..l-n as usize])
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum AnsiX923{}

impl Padding for AnsiX923 {
    #[inline]
    fn pad(block: &mut [u8], pos: usize) {
        let n = block.len() - 1;
        zero(&mut block[pos..n]);
        block[n] = (n - pos) as u8;
    }
}

impl Unpadding for AnsiX923 {
    #[inline]
    fn unpad(data: &[u8]) -> Result<&[u8], UnpadError> {
        if data.is_empty() { return Err(UnpadError); }
        let l = data.len();
        let n = data[l-1] as usize;
        if n == 0 {
            return Err(UnpadError)
        }
        for v in &data[l-n..l-1] {
            if *v != 0 { return Err(UnpadError); }
        }
        Ok(&data[..l-n])
    }
}



#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum Iso7816{}

impl Padding for Iso7816 {
    #[inline]
    fn pad(block: &mut [u8], pos: usize) {
        let n = block.len() - pos;
        block[pos] = 0x80;
        for b in block[pos+1..].iter_mut() {
            *b = n as u8;
        }
    }
}

impl Unpadding for Iso7816 {
    fn unpad(data: &[u8]) -> Result<&[u8], UnpadError> {
        if data.is_empty() { return Err(UnpadError); }
        let mut n = data.len() - 1;
        while n != 0 {
            if data[n] != 0 {
                break;
            }
            n -= 1;
        }
        if data[n] != 0x80 { return Err(UnpadError); }
        Ok(&data[..n])
    }
}
