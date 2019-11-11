// Simple example of how to use parity-wasm builder api.
// Builder api introduced as a method for fast generation of
// different small wasm modules.

extern crate parity_wasm;

use std::env;

use parity_wasm::builder;
use parity_wasm::elements;

fn main() {

	// Example binary accepts one parameter which is the output file
	// where generated wasm module will be written at the end of execution
	let args = env::args().collect::<Vec<_>>();
	if args.len() != 2 {
		println!("Usage: {} output_file.wasm", args[0]);
		return;
	}

	// Main entry for the builder api is the module function
	// It returns empty module builder structure which can be further
	// appended with various wasm artefacts
	let module = builder::module()
		// Here we append function to the builder
		// function() function returns a function builder attached
		// to the module builder.
		.function()
			// We describe signature for the function via signature()
			// function. In our simple example it's just one input
			// argument of type 'i32' without return value
			.signature().with_param(elements::ValueType::I32).build()
			// body() without any further arguments means that the body
			// of the function will be empty
			.body().build()
			// This is the end of the function builder. When `build()` is
			// invoked, function builder returns original module builder
			// from which it was invoked
			.build()
		// And finally we finish our module builder to produce actual
		// wasm module.
		.build();

	// Module structure can be serialzed to produce a valid wasm file
	parity_wasm::serialize_to_file(&args[1], module).unwrap();
}