extern crate build_const;

include!("src/util.rs");

#[allow(non_snake_case)]
fn create_constants() {
    let mut crc32 = build_const::ConstWriter::for_build("crc32_constants")
        .unwrap()
        .finish_dependencies();
    let CASTAGNOLI: u32 = 0x82f63b78;
    crc32.add_value("CASTAGNOLI", "u32", CASTAGNOLI);
    crc32.add_array("CASTAGNOLI_TABLE", "u32", &make_table_crc32(CASTAGNOLI));

    let IEEE: u32 = 0xedb88320;
    crc32.add_value("IEEE", "u32", IEEE);
    crc32.add_array("IEEE_TABLE", "u32", &make_table_crc32(IEEE));

    let KOOPMAN: u32 = 0xeb31d82e;
    crc32.add_value("KOOPMAN", "u32", KOOPMAN);
    crc32.add_array("KOOPMAN_TABLE", "u32", &make_table_crc32(KOOPMAN));

    crc32.finish();

    let mut crc64 = build_const::ConstWriter::for_build("crc64_constants")
        .unwrap()
        .finish_dependencies();

    let ECMA: u64 = 0xc96c5795d7870f42;
    crc64.add_value("ECMA", "u64", ECMA);
    crc64.add_array("ECMA_TABLE", "u64", &make_table_crc64(ECMA));

    let ISO: u64 = 0xd800000000000000;
    crc64.add_value("ISO", "u64", ISO);
    crc64.add_array("ISO_TABLE", "u64", &make_table_crc64(ISO));

    crc64.finish();
}

fn main() {
    create_constants();
}
