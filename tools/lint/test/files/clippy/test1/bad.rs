fn main() {
    // Statements here are executed when the compiled binary is called

    // Print text to the console
    println!("Hello World!");
    // Clippy detects this as a swap and considers this as an error
    let mut a=1;
    let mut b=1;

    a = b;
    b = a;


}
