use std;

/// The byte level filter applied to scanlines to prepare them for compression.
///
/// Compression in general benefits from repetitive data. The filter is a content-aware method of
/// compressing the range of occurring byte values to help the compression algorithm. Note that
/// this does not operate on pixels but on raw bytes of a scanline.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum FilterType {
    NoFilter = 0,
    Sub = 1,
    Up = 2,
    Avg = 3,
    Paeth = 4
}

 impl FilterType {  
    /// u8 -> Self. Temporary solution until Rust provides a canonical one.
    pub fn from_u8(n: u8) -> Option<FilterType> {
        match n {
            0 => Some(FilterType::NoFilter),
            1 => Some(FilterType::Sub),
            2 => Some(FilterType::Up),
            3 => Some(FilterType::Avg),
            4 => Some(FilterType::Paeth),
            _ => None
        }
    }
}

fn filter_paeth(a: u8, b: u8, c: u8) -> u8 {
    let ia = a as i16;
    let ib = b as i16;
    let ic = c as i16;

    let p = ia + ib - ic;

    let pa = (p - ia).abs();
    let pb = (p - ib).abs();
    let pc = (p - ic).abs();

    if pa <= pb && pa <= pc {
        a
    } else if pb <= pc {
        b
    } else {
        c
    }
}

pub fn unfilter(filter: FilterType, bpp: usize, previous: &[u8], current: &mut [u8]) -> std::result::Result<(), &'static str> {
    use self::FilterType::*;
    assert!(bpp > 0);
    let len = current.len();

    match filter {
        NoFilter => Ok(()),
        Sub => {
            for i in bpp..len {
                current[i] = current[i].wrapping_add(
                    current[i - bpp]
                );
            }
            Ok(())
        }
        Up => {
            if previous.len() < len {
                Err("Filtering failed: not enough data in previous row")
            } else {
                for i in 0..len {
                    current[i] = current[i].wrapping_add(
                        previous[i]
                    );
                }
                Ok(())
            }
        }
        Avg => {
            if previous.len() < len {
                Err("Filtering failed:  not enough data in previous row")
            } else if bpp > len {
                Err("Filtering failed: bytes per pixel is greater than length of row")
            } else {
                for i in 0..bpp {
                    current[i] = current[i].wrapping_add(
                        previous[i] / 2
                    );
                }

                for i in bpp..len {
                    current[i] = current[i].wrapping_add(
                        ((current[i - bpp] as i16 + previous[i] as i16) / 2) as u8
                    );
                }
                Ok(())
            }
        }
        Paeth => {
            if previous.len() < len {
                Err("Filtering failed: not enough data in previous row")
            } else if bpp > len {
                Err("Filtering failed: bytes per pixel is greater than length of row")
            } else {
                for i in 0..bpp {
                    current[i] = current[i].wrapping_add(
                        filter_paeth(0, previous[i], 0)
                    );
                }

                for i in bpp..len {
                    current[i] = current[i].wrapping_add(
                        filter_paeth(current[i - bpp], previous[i], previous[i - bpp])
                    );
                }
                Ok(())
            }
        }
    }
}

pub fn filter(method: FilterType, bpp: usize, previous: &[u8], current: &mut [u8]) {
    use self::FilterType::*;
    assert!(bpp > 0);
    let len  = current.len();

    match method {
        NoFilter => (),
        Sub      => {
            for i in (bpp..len).rev() {
                current[i] = current[i].wrapping_sub(current[i - bpp]);
            }
        }
        Up       => {
            for i in 0..len {
                current[i] = current[i].wrapping_sub(previous[i]);
            }
        }
        Avg  => {
            for i in (bpp..len).rev() {
                current[i] = current[i].wrapping_sub(current[i - bpp].wrapping_add(previous[i]) / 2);
            }

            for i in 0..bpp {
                current[i] = current[i].wrapping_sub(previous[i] / 2);
            }
        }
        Paeth    => {
            for i in (bpp..len).rev() {
                current[i] = current[i].wrapping_sub(filter_paeth(current[i - bpp], previous[i], previous[i - bpp]));
            }

            for i in 0..bpp {
                current[i] = current[i].wrapping_sub(filter_paeth(0, previous[i], 0));
            }
        }
    }
}
