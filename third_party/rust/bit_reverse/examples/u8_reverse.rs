// Load the bit_reverse external crate
extern crate bit_reverse;

// Imports the Parallel Bit reversal algorithm to bit reverse numbers.
use bit_reverse::ParallelReverse;

// Uncomment one of these if you want to use a different algorithm.
// use bit_reverse::BitwiseReverse;
// use bit_reverse::LookupReverse;

fn main() {
    // Calculate the bit reversal of all u8 numbers and print them out.
    let reverse: Vec<u8> = (0..).take(256).map(|x| (x as u8).swap_bits()).collect();
    println!("{:?}", reverse);
}