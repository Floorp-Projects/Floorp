extern crate gamma_lut;

fn main() {
    let contrast = 0.0;
    let gamma = 0.0;

    let table = gamma_lut::GammaLut::new(contrast, gamma, gamma);
    table.print_values(0);
}
