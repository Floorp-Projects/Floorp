use thiserror::Error;

#[derive(Error, Debug)]
pub struct ErrorStruct {
    #[source]
    a: std::io::Error,
    #[source]
    b: anyhow::Error,
}

#[derive(Error, Debug)]
pub enum ErrorEnum {
    Confusing {
        #[source]
        a: std::io::Error,
        #[source]
        b: anyhow::Error,
    },
}

fn main() {}
