#[cfg(feature = "term_size")]
extern crate textwrap;

#[cfg(not(feature = "term_size"))]
fn main() {
    println!("Please enable the term_size feature to run this example.");
}

#[cfg(feature = "term_size")]
fn main() {
    let example = "Memory safety without garbage collection. \
                   Concurrency without data races. \
                   Zero-cost abstractions.";
    // Create a new Wrapper -- automatically set the width to the
    // current terminal width.
    let wrapper = textwrap::Wrapper::with_termwidth();
    println!("Formatted in within {} columns:", wrapper.width);
    println!("----");
    println!("{}", wrapper.fill(example));
    println!("----");
}
