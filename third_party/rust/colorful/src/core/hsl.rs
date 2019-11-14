use core::ColorInterface;
use core::rgb::RGB;

#[derive(Debug, Copy, Clone, PartialEq)]
pub struct HSL {
    // range 0 ≤ H < 1.0, 0 ≤ S ≤ 1.0 and 0 ≤ L ≤ 1.0:
    pub h: f32,
    s: f32,
    l: f32,
}

impl HSL {
    pub fn new(h: f32, s: f32, l: f32) -> HSL {
        HSL { h: _round(h), s: _round(s), l: _round(l) }
    }

    pub fn hsl_to_rgb(&self) -> RGB {
        let red: f32;
        let green: f32;
        let blue: f32;
        let var_1: f32;
        let var_2: f32;
        if self.s == 0.0 {
            let tmp = self.l * 255.0;
            red = tmp;
            green = tmp;
            blue = tmp;
        } else {
            if self.l < 0.5 {
                var_2 = self.l * (1.0 + self.s);
            } else {
                var_2 = (self.l + self.s) - (self.s * self.l);
            }
            var_1 = 2.0 * self.l - var_2;
            red = 255.0 * hue_2_rgb(var_1, var_2, &mut (self.h + (1.0 / 3.0)));
            green = 255.0 * hue_2_rgb(var_1, var_2, &mut self.h.clone());
            blue = 255.0 * hue_2_rgb(var_1, var_2, &mut (self.h - (1.0 / 3.0)));
        }
        RGB::new(red.round() as u8, green.round() as u8, blue.round() as u8)
    }
}

impl ColorInterface for HSL {
    fn to_color_str(&self) -> String {
        self.hsl_to_rgb().to_color_str()
    }
    fn to_hsl(&self) -> HSL { *self }
}

fn hue_2_rgb(v1: f32, v2: f32, vh: &mut f32) -> f32 {
    if *vh < 0.0 {
        *vh += 1.0;
    }
    if *vh > 1.0 {
        *vh -= 1.0;
    }
    if 6.0 * *vh < 1.0 {
        return v1 + (v2 - v1) * 6.0 * *vh;
    }
    if 2.0 * *vh < 1.0 {
        return v2;
    }
    if 3.0 * *vh < 2.0 {
        return v1 + (v2 - v1) * (2.0 / 3.0 - *vh) * 6.0;
    }
    v1
}

fn _round(value: f32) -> f32 {
    if value < 0.0 {
        0.0
    } else if value >= 1.0 {
        1.0
    } else {
        value
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hsl_2_rgb_1() {
        let hsl = HSL::new(0.7, 0.50, 0.60);
        let rgb = RGB::new(122, 102, 204);

        assert_eq!(rgb, hsl.hsl_to_rgb());
    }

    #[test]
    fn test_hsl_2_rgb_2() {
        let hsl = HSL::new(0.7, 0.0, 0.60);
        let rgb = RGB::new(153, 153, 153);
        assert_eq!(rgb, hsl.hsl_to_rgb());
    }

    #[test]
    fn test_hsl_2_rgb_3() {
        let hsl = HSL::new(0.7, 0.50, 0.30);
        let rgb = RGB::new(54, 38, 115);

        assert_eq!(rgb, hsl.hsl_to_rgb());
    }
}