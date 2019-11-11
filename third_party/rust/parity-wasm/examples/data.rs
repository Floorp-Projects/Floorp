// This short example provides the utility to inspect
// wasm file data section.

extern crate parity_wasm;

use std::env;

fn main() {

	// Example executable takes one argument which must
	// refernce the existing file with a valid wasm module
	let args = env::args().collect::<Vec<_>>();
	if args.len() != 2 {
		println!("Usage: {} somefile.wasm", args[0]);
		return;
	}

	// Here we load module using dedicated for this purpose
	// `deserialize_file` function (which works only with modules)
	let module = parity_wasm::deserialize_file(&args[1]).expect("Failed to load module");

	// We query module for data section. Note that not every valid
	// wasm module must contain a data section. So in case the provided
	// module does not contain data section, we panic with an error
	let data_section = module.data_section().expect("no data section in module");

	// Printing the total count of data segments
	println!("Data segments: {}", data_section.entries().len());

	let mut index = 0;
	for entry in data_section.entries() {
		// Printing the details info of each data segment
		// see `elements::DataSegment` for more properties
		// you can query
		println!("  Entry #{}", index);

		// This shows the initialization member of data segment
		// (expression which must resolve in the linear memory location).
		if let Some(offset) = entry.offset() {
			println!("	init: {}", offset.code()[0]);
		}

		// This shows the total length of the data segment in bytes.
		println!("	size: {}", entry.value().len());

		index += 1;
	}
}
