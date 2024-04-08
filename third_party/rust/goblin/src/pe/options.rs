/// Parsing Options structure for the PE parser
#[derive(Debug, Copy, Clone)]
pub struct ParseOptions {
    /// Wether the parser should resolve rvas or not. Default: true
    pub resolve_rva: bool,
}

impl ParseOptions {
    /// Returns a parse options structure with default values
    pub fn default() -> Self {
        ParseOptions { resolve_rva: true }
    }
}
