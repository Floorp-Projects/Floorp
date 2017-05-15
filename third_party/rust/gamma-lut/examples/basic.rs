extern crate gamma_lut;

fn main() {
    let contrast = 0.0;
    let gamma = 0.0;

    let table = gamma_lut::GammaLut::new(contrast, gamma, gamma);
    for (i, value) in table.get_table(0).iter().enumerate() {
        println!("[{:?}] = {:?}", i, *value);
    }
}
