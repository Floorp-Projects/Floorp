//! An example that displays the type/features/configuration of the CPU.
extern crate raw_cpuid;

fn main() {
    let cpuid = raw_cpuid::CpuId::new();

    let vendor_string = cpuid.get_vendor_info().map_or_else(
        || String::from("Unknown Vendor"),
        |vf| String::from(vf.as_string()),
    );

    let brand_string: String = cpuid.get_extended_function_info().map_or_else(
        || String::from("Unknown CPU"),
        |extfuninfo| {
            println!("{:?}", extfuninfo.extended_signature());
            String::from(extfuninfo.processor_brand_string().unwrap_or("Unknown CPU"))
        },
    );

    /*let serial_nr = cpuid.get_processor_serial().map_or_else(
        || String::from(""),
        |extfuninfo| String::from(extfuninfo.processor_brand_string().unwrap_or("Unknown CPU")),
    );*/

    println!("Vendor is: {}", vendor_string);
    println!("CPU Model is: {}", brand_string);
}
