extern crate argon2rs;

pub fn main() {
    let (password, salt) = ("argon2i!", "delicious salt");
    println!("argon2i(\"argon2i\", \"delicious\"):");
    for byte in argon2rs::argon2i_simple(&password, &salt).iter() {
        print!("{:02x}", byte);
    }
    println!("");
}
