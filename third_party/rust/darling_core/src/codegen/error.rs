use quote::{Tokens, ToTokens};

/// Declares the local variable into which errors will be accumulated.
pub struct ErrorDeclaration {
    __hidden: ()
}

impl ErrorDeclaration {
    pub fn new() -> Self {
        ErrorDeclaration {
            __hidden: ()
        }
    }
}

impl ToTokens for ErrorDeclaration {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(quote! {
            let mut __errors = Vec::new();
        })
    }
}

/// Returns early if attribute or body parsing has caused any errors.
pub struct ErrorCheck<'a> {
    location: Option<&'a str>,
    __hidden: ()
}

impl<'a> ErrorCheck<'a> {
    pub fn new() -> Self {
        ErrorCheck {
            location: None,
            __hidden: ()
        }
    }

    pub fn with_location(location: &'a str) -> Self {
        ErrorCheck {
            location: Some(location),
            __hidden: (),
        }
    }
}

impl<'a> ToTokens for ErrorCheck<'a> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        let at_call = if let Some(ref s) = self.location {
            quote!(.at(#s))
        } else {
            quote!()
        };

        tokens.append(quote! {
            if !__errors.is_empty() {
                return ::darling::export::Err(::darling::Error::multiple(__errors) #at_call);
            }
        })
    }
}