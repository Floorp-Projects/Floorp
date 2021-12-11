use syn::parse::{Error, ParseStream, Result};
use syn::TraitItemMethod;

pub fn parse(input: ParseStream) -> Result<Vec<TraitItemMethod>> {
    let mut items = Vec::new();
    let mut error = None;
    while !input.is_empty() {
        let item = input.parse::<TraitItemMethod>()?;
        if let Some(body) = item.default {
            let new_err = Error::new_spanned(body, "Default method can't have a body");
            match &mut error {
                None => error = Some(new_err),
                Some(e) => e.combine(new_err),
            }
        } else {
            items.push(item);
        }
    }
    match error {
        None => Ok(items),
        Some(err) => Err(err),
    }
}
