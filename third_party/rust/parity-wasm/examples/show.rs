extern crate parity_wasm;

use std::env;

fn main() {
	let args = env::args().collect::<Vec<_>>();
	if args.len() != 3 {
		println!("Usage: {} <wasm file> <index of function>", args[0]);
		return;
	}

	let module = parity_wasm::deserialize_file(&args[1]).expect("Failed to load module");
	let function_index = args[2].parse::<usize>().expect("Failed to parse function index");

	if module.code_section().is_none() {
		println!("no code in module!");
		std::process::exit(1);
	}

	let sig = match module.function_section().unwrap().entries().get(function_index) {
		Some(s) => s,
		None => {
			println!("no such function in module!");
			std::process::exit(1)
		}
	};

	let sig_type = &module.type_section().expect("No type section: module malformed").types()[sig.type_ref() as usize];
	let code = &module.code_section().expect("Already checked, impossible").bodies()[function_index];

	println!("signature: {:?}", sig_type);
	println!("code: ");
	for instruction in code.code().elements() {
		println!("{}", instruction);
	}
}