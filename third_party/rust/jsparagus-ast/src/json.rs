use serde_json::Result;

pub fn to_string<'alloc>(_ast: &super::Program<'alloc>) -> Result<String> {
    // serde_json::to_string(ast)
    unimplemented!();
}

pub fn to_string_pretty<'alloc>(_ast: &super::Program<'alloc>) -> Result<String> {
    // serde_json::to_string_pretty(ast)
    unimplemented!();
}
