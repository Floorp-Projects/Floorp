fn main() {
    let ac = autocfg::new();
    if ac.probe_expression("format!(\"{:e}\", 0_isize)") {
        println!("cargo:rustc-cfg=has_int_exp_fmt");
    }

    autocfg::rerun_path(file!());
}
