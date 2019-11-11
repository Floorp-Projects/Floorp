extern crate parity_wasm;

use std::env;

fn main() {
	let args = env::args().collect::<Vec<_>>();
	if args.len() != 3 {
		println!("Usage: {} in.wasm out.wasm", args[0]);
		return;
	}

	let module = match parity_wasm::deserialize_file(&args[1])
		.expect("Failed to load module")
		.parse_names()
		.and_then(|module| module.parse_reloc())
	{
		Ok(m) => m,
		Err((errors, m)) => {
			for (index, error) in errors.into_iter() {
				println!("Custom section #{} parse error: {:?}", index, error);
			}
			m
		}
	};

	parity_wasm::serialize_to_file(&args[2], module).expect("Failed to write module");
}
