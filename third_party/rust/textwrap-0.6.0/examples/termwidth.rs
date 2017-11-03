extern crate textwrap;

use textwrap::Wrapper;

fn main() {
    let example = "
Memory safety without garbage collection.
Concurrency without data races.
Zero-cost abstractions.
";
    // Create a new Wrapper -- automatically set the width to the
    // current terminal width.
    let wrapper = Wrapper::with_termwidth();
    println!("Formatted in within {} columns:", wrapper.width);
    println!("----");
    println!("{}", wrapper.fill(example));
    println!("----");
}
