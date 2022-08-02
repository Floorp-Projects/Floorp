use extend::ext;
use async_trait::async_trait;

#[ext]
#[async_trait]
impl String {
    async fn foo() -> usize {
        1
    }
}

#[ext]
#[async_trait]
pub impl i32 {
    async fn bar() -> usize {
        1
    }
}

async fn foo() {
    let _: usize = String::foo().await;
    let _: usize = i32::bar().await;
}

fn main() {}
