use crate::unicode_data::{
    char_info, is_id_continue_non_bmp, is_id_start_non_bmp, IS_ID_CONTINUE_TABLE, IS_ID_START_TABLE,
};

const UTF16_MAX: char = '\u{ffff}';

fn is_id_start_ascii(c: char) -> bool {
    IS_ID_START_TABLE[c as usize]
}

fn is_id_continue_ascii(c: char) -> bool {
    IS_ID_CONTINUE_TABLE[c as usize]
}

fn is_id_start_bmp_non_ascii(c: char) -> bool {
    char_info(c).is_id_start()
}

fn is_id_continue_bmp_non_ascii(c: char) -> bool {
    char_info(c).is_id_continue()
}

pub fn is_id_start(c: char) -> bool {
    if c > UTF16_MAX {
        return is_id_start_non_bmp(c);
    }
    if c < '\u{80}' {
        return is_id_start_ascii(c);
    }
    is_id_start_bmp_non_ascii(c)
}

pub fn is_id_continue(c: char) -> bool {
    if c > UTF16_MAX {
        return is_id_continue_non_bmp(c);
    }
    if c < '\u{80}' {
        return is_id_continue_ascii(c);
    }
    is_id_continue_bmp_non_ascii(c)
}
