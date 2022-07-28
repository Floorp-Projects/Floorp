use arbitrary::{Arbitrary, Unstructured};
use rand::{rngs::SmallRng, RngCore, SeedableRng};
use wasm_smith::Component;

#[test]
fn smoke_test_component() {
    const NUM_RUNS: usize = 4096;

    let mut rng = SmallRng::seed_from_u64(0);
    let mut buf = vec![0; 1024];
    let mut ok_count = 0;

    for _ in 0..NUM_RUNS {
        rng.fill_bytes(&mut buf);
        let u = Unstructured::new(&buf);
        if let Ok(component) = Component::arbitrary_take_rest(u) {
            ok_count += 1;
            let component = component.to_bytes();

            let mut validator =
                wasmparser::Validator::new_with_features(wasmparser::WasmFeatures {
                    component_model: true,
                    ..Default::default()
                });
            if let Err(e) = validator.validate_all(&component) {
                std::fs::write("component.wasm", &component).unwrap();
                panic!(
                    "generated component should be valid; failing binary written \
                     to `component.wasm`. Error: {}",
                    e
                );
            }
        }
    }

    println!(
        "Generated {} / {} ({:.02}%) arbitrary components okay",
        ok_count,
        NUM_RUNS,
        ok_count as f64 / NUM_RUNS as f64 * 100.0
    );
}
