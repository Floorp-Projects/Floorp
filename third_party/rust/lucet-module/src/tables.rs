use crate::functions::FunctionPointer;

#[repr(C)]
#[derive(Clone, Debug)]
pub struct TableElement {
    ty: u64,
    func: u64,
}

impl TableElement {
    pub fn function_pointer(&self) -> FunctionPointer {
        FunctionPointer::from_usize(self.func as usize)
    }
}
