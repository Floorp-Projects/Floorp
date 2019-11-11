// This examples allow to query all function exports of the
// provided wasm module

extern crate parity_wasm;

use std::env::args;

use parity_wasm::elements::{Internal, External, Type, FunctionType, Module};

// Auxillary function to resolve function type (signature) given it's callable index
fn type_by_index(module: &Module, index: usize) -> FunctionType {

	// Demand that function and type section exist. Otherwise, fail with a
	// corresponding error.
	let function_section = module.function_section().expect("No function section found");
	let type_section = module.type_section().expect("No type section found");

	// This counts the number of _function_ imports listed by the module, excluding
	// the globals, since indexing for actual functions for `call` and `export` purposes
	// includes both imported and own functions. So we actualy need the imported function count
	// to resolve actual index of the given function in own functions list.
	let import_section_len: usize = match module.import_section() {
			Some(import) =>
				import.entries().iter().filter(|entry| match entry.external() {
					&External::Function(_) => true,
					_ => false,
					}).count(),
			None => 0,
		};

	// Substract the value queried in the previous step from the provided index
	// to get own function index from which we can query type next.
	let function_index_in_section = index - import_section_len;

	// Query the own function given we have it's index
	let func_type_ref: usize = function_section.entries()[function_index_in_section].type_ref() as usize;

	// Finally, return function type (signature)
	match type_section.types()[func_type_ref] {
		Type::Function(ref func_type) => func_type.clone(),
	}
}

fn main() {

	// Example executable takes one argument which must
	// refernce the existing file with a valid wasm module
	let args: Vec<_> = args().collect();
	if args.len() < 2 {
		println!("Prints export function names with and their types");
		println!("Usage: {} <wasm file>", args[0]);
		return;
	}

	// Here we load module using dedicated for this purpose
	// `deserialize_file` function (which works only with modules)
	let module = parity_wasm::deserialize_file(&args[1]).expect("File to be deserialized");

	// Query the export section from the loaded module. Note that not every
	// wasm module obliged to contain export section. So in case there is no
	// any export section, we panic with the corresponding error.
	let export_section = module.export_section().expect("No export section found");

	// Process all exports, leaving only those which reference the internal function
	// of the wasm module
	let exports: Vec<String> = export_section.entries().iter()
		.filter_map(|entry|
			// This is match on export variant, which can be function, global,table or memory
			// We are interested only in functions for an example
			match *entry.internal() {
				// Return function export name (return by field() function and it's index)
				Internal::Function(index) => Some((entry.field(), index as usize)),
				_ => None
			})
		// Another map to resolve function signature index given it's internal index and return
		// the printable string of the export
		.map(|(field, index)| format!("{:}: {:?}", field, type_by_index(&module, index).params())).collect();

	// Print the result
	for export in exports {
		println!("{:}", export);
	}
}
