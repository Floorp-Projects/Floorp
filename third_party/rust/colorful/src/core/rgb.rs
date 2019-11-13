use core::ColorInterface;
use HSL;

#[derive(Debug, Copy, Clone, PartialEq)]
pub struct RGB {
    // range 0 -255
    r: u8,
    g: u8,
    b: u8,
}

impl RGB {
    pub fn new(r: u8, g: u8, b: u8) -> RGB {
        RGB { r, g, b }
    }

    pub fn unpack(&self) -> (u8, u8, u8) {
        (self.r, self.g, self.b)
    }

    pub fn rgb_to_hsl(&self) -> HSL {
        let (r, g, b) = self.unpack();
        let r = r as f32 / 255.0;
        let g = g as f32 / 255.0;
        let b = b as f32 / 255.0;

        let max = r.max(g).max(b);
        let min = r.min(g).min(b);
        let mut h: f32 = 0.0;
        let mut s: f32 = 0.0;
        let l = (max + min) / 2.0;

        if max != min {
            let d = max - min;
            s = if l > 0.5 { d / (2.0 - max - min) } else { d / (max + min) };
            if max == r {
                h = (g - b) / d + (if g < b { 6.0 } else { 0.0 });
            } else if max == g {
                h = (b - r) / d + 2.0;
            } else {
                h = (r - g) / d + 4.0;
            }
            h /= 6.0;
        }
        return HSL::new(h, s, l);
    }
}

impl ColorInterface for RGB {
    fn to_color_str(&self) -> String {
        format!("{};{};{}", self.r, self.g, self.b)
    }
    fn to_hsl(&self) -> HSL { self.rgb_to_hsl() }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_rgb_2_hsl_1() {
        let hsl = HSL::new(0.69934636, 0.49999997, 0.60);
        let rgb = RGB::new(122, 102, 204);

        assert_eq!(hsl, rgb.rgb_to_hsl());
    }

    #[test]
    fn test_rgb_2_hsl_2() {
        let hsl = HSL::new(0.0, 0.0, 0.60);
        let rgb = RGB::new(153, 153, 153);
        assert_eq!(hsl, rgb.rgb_to_hsl());
    }

    #[test]
    fn test_rgb_2_hsl_3() {
        let hsl = HSL::new(0.7012987, 0.50326794, 0.30);
        let rgb = RGB::new(54, 38, 115);

        assert_eq!(hsl, rgb.rgb_to_hsl());
    }

    #[test]
    fn test_rgb_2_hsl_4() {
        let hsl = HSL::new(0.08333334, 1.0, 0.6862745);
        let rgb = RGB::new(255, 175, 95);

        assert_eq!(hsl, rgb.rgb_to_hsl());
    }
}