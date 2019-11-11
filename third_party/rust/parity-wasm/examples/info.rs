extern crate parity_wasm;

use std::env;
use parity_wasm::elements::Section;

fn main() {
	let args = env::args().collect::<Vec<_>>();
	if args.len() != 2 {
		println!("Usage: {} somefile.wasm", args[0]);
		return;
	}

	let module = parity_wasm::deserialize_file(&args[1]).expect("Failed to load module");

	println!("Module sections: {}", module.sections().len());

	for section in module.sections() {
		match *section {
			Section::Import(ref import_section) => {
				println!("  Imports: {}", import_section.entries().len());
				import_section.entries().iter().map(|e| println!("    {}.{}", e.module(), e.field())).count();
			},
			Section::Export(ref exports_section) => {
				println!("  Exports: {}", exports_section.entries().len());
				exports_section.entries().iter().map(|e| println!("    {}", e.field())).count();
			},
			Section::Function(ref function_section) => {
				println!("  Functions: {}", function_section.entries().len());
			},
			Section::Type(ref type_section) => {
				println!("  Types: {}", type_section.types().len());
			},
			Section::Global(ref globals_section) => {
				println!("  Globals: {}", globals_section.entries().len());
			},
			Section::Table(ref table_section) => {
				println!("  Tables: {}", table_section.entries().len());
			},
			Section::Memory(ref memory_section) => {
				println!("  Memories: {}", memory_section.entries().len());
			},
			Section::Data(ref data_section) if data_section.entries().len() > 0 => {
				let data = &data_section.entries()[0];
				println!("  Data size: {}", data.value().len());
			},
			_ => {},
		}
	}
}