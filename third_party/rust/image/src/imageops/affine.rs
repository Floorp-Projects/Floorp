//! Functions for performing affine transformations.

use buffer::{ImageBuffer, Pixel};
use image::GenericImageView;

/// Rotate an image 90 degrees clockwise.
// TODO: Is the 'static bound on `I` really required? Can we avoid it?
pub fn rotate90<I: GenericImageView + 'static>(
    image: &I,
) -> ImageBuffer<I::Pixel, Vec<<I::Pixel as Pixel>::Subpixel>>
where
    I::Pixel: 'static,
    <I::Pixel as Pixel>::Subpixel: 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(height, width);

    for y in 0..height {
        for x in 0..width {
            let p = image.get_pixel(x, y);
            out.put_pixel(height - 1 - y, x, p);
        }
    }

    out
}

/// Rotate an image 180 degrees clockwise.
// TODO: Is the 'static bound on `I` really required? Can we avoid it?
pub fn rotate180<I: GenericImageView + 'static>(
    image: &I,
) -> ImageBuffer<I::Pixel, Vec<<I::Pixel as Pixel>::Subpixel>>
where
    I::Pixel: 'static,
    <I::Pixel as Pixel>::Subpixel: 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(width, height);

    for y in 0..height {
        for x in 0..width {
            let p = image.get_pixel(x, y);
            out.put_pixel(width - 1 - x, height - 1 - y, p);
        }
    }

    out
}

/// Rotate an image 270 degrees clockwise.
// TODO: Is the 'static bound on `I` really required? Can we avoid it?
pub fn rotate270<I: GenericImageView + 'static>(
    image: &I,
) -> ImageBuffer<I::Pixel, Vec<<I::Pixel as Pixel>::Subpixel>>
where
    I::Pixel: 'static,
    <I::Pixel as Pixel>::Subpixel: 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(height, width);

    for y in 0..height {
        for x in 0..width {
            let p = image.get_pixel(x, y);
            out.put_pixel(y, width - 1 - x, p);
        }
    }

    out
}

/// Flip an image horizontally
// TODO: Is the 'static bound on `I` really required? Can we avoid it?
pub fn flip_horizontal<I: GenericImageView + 'static>(
    image: &I,
) -> ImageBuffer<I::Pixel, Vec<<I::Pixel as Pixel>::Subpixel>>
where
    I::Pixel: 'static,
    <I::Pixel as Pixel>::Subpixel: 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(width, height);

    for y in 0..height {
        for x in 0..width {
            let p = image.get_pixel(x, y);
            out.put_pixel(width - 1 - x, y, p);
        }
    }

    out
}

/// Flip an image vertically
// TODO: Is the 'static bound on `I` really required? Can we avoid it?
pub fn flip_vertical<I: GenericImageView + 'static>(
    image: &I,
) -> ImageBuffer<I::Pixel, Vec<<I::Pixel as Pixel>::Subpixel>>
where
    I::Pixel: 'static,
    <I::Pixel as Pixel>::Subpixel: 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(width, height);

    for y in 0..height {
        for x in 0..width {
            let p = image.get_pixel(x, y);
            out.put_pixel(x, height - 1 - y, p);
        }
    }

    out
}

#[cfg(test)]
mod test {
    use super::{flip_horizontal, flip_vertical, rotate180, rotate270, rotate90};
    use buffer::{GrayImage, ImageBuffer, Pixel};
    use image::GenericImage;

    macro_rules! assert_pixels_eq {
        ($actual:expr, $expected:expr) => {{
            let actual_dim = $actual.dimensions();
            let expected_dim = $expected.dimensions();

            if actual_dim != expected_dim {
                panic!(
                    "dimensions do not match. \
                     actual: {:?}, expected: {:?}",
                    actual_dim, expected_dim
                )
            }

            let diffs = pixel_diffs($actual, $expected);

            if !diffs.is_empty() {
                let mut err = "pixels do not match. ".to_string();

                let diff_messages = diffs
                    .iter()
                    .take(5)
                    .map(|d| format!("\nactual: {:?}, expected {:?} ", d.0, d.1))
                    .collect::<Vec<_>>()
                    .join("");

                err.push_str(&diff_messages);
                panic!(err)
            }
        }};
    }

    #[test]
    fn test_rotate90() {
        let image: GrayImage =
            ImageBuffer::from_raw(3, 2, vec![00u8, 01u8, 02u8, 10u8, 11u8, 12u8]).unwrap();

        let expected: GrayImage =
            ImageBuffer::from_raw(2, 3, vec![10u8, 00u8, 11u8, 01u8, 12u8, 02u8]).unwrap();

        assert_pixels_eq!(&rotate90(&image), &expected);
    }

    #[test]
    fn test_rotate180() {
        let image: GrayImage =
            ImageBuffer::from_raw(3, 2, vec![00u8, 01u8, 02u8, 10u8, 11u8, 12u8]).unwrap();

        let expected: GrayImage =
            ImageBuffer::from_raw(3, 2, vec![12u8, 11u8, 10u8, 02u8, 01u8, 00u8]).unwrap();

        assert_pixels_eq!(&rotate180(&image), &expected);
    }

    #[test]
    fn test_rotate270() {
        let image: GrayImage =
            ImageBuffer::from_raw(3, 2, vec![00u8, 01u8, 02u8, 10u8, 11u8, 12u8]).unwrap();

        let expected: GrayImage =
            ImageBuffer::from_raw(2, 3, vec![02u8, 12u8, 01u8, 11u8, 00u8, 10u8]).unwrap();

        assert_pixels_eq!(&rotate270(&image), &expected);
    }

    #[test]
    fn test_flip_horizontal() {
        let image: GrayImage =
            ImageBuffer::from_raw(3, 2, vec![00u8, 01u8, 02u8, 10u8, 11u8, 12u8]).unwrap();

        let expected: GrayImage =
            ImageBuffer::from_raw(3, 2, vec![02u8, 01u8, 00u8, 12u8, 11u8, 10u8]).unwrap();

        assert_pixels_eq!(&flip_horizontal(&image), &expected);
    }

    #[test]
    fn test_flip_vertical() {
        let image: GrayImage =
            ImageBuffer::from_raw(3, 2, vec![00u8, 01u8, 02u8, 10u8, 11u8, 12u8]).unwrap();

        let expected: GrayImage =
            ImageBuffer::from_raw(3, 2, vec![10u8, 11u8, 12u8, 00u8, 01u8, 02u8]).unwrap();

        assert_pixels_eq!(&flip_vertical(&image), &expected);
    }

    fn pixel_diffs<I, J, P>(left: &I, right: &J) -> Vec<((u32, u32, P), (u32, u32, P))>
    where
        I: GenericImage<Pixel = P>,
        J: GenericImage<Pixel = P>,
        P: Pixel + Eq,
    {
        left.pixels()
            .zip(right.pixels())
            .filter(|&(p, q)| p != q)
            .collect::<Vec<_>>()
    }
}
