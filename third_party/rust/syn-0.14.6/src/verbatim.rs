use std::ops::Range;

use buffer::Cursor;
use proc_macro2::TokenStream;
use synom::PResult;

pub fn grab_cursor(cursor: Cursor) -> PResult<Cursor> {
    Ok((cursor, cursor))
}

pub fn token_range(range: Range<Cursor>) -> TokenStream {
    let mut tts = Vec::new();
    let mut cursor = range.start;
    while cursor != range.end {
        let (tt, next) = cursor.token_tree().expect("malformed token range");
        tts.push(tt);
        cursor = next;
    }
    tts.into_iter().collect()
}
