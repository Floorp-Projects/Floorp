fn main() {
    println!("{:?}", whatsys::kernel_version());
    #[cfg(target_os = "windows")]
    println!("{:?}", whatsys::windows_build_number());
}
