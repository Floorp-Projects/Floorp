use crate::vk;
pub type VkResult<T> = Result<T, vk::Result>;

impl From<vk::Result> for VkResult<()> {
    fn from(err_code: vk::Result) -> Self {
        err_code.result()
    }
}

impl vk::Result {
    pub fn result(self) -> VkResult<()> {
        self.result_with_success(())
    }

    pub fn result_with_success<T>(self, v: T) -> VkResult<T> {
        match self {
            vk::Result::SUCCESS => Ok(v),
            _ => Err(self),
        }
    }
}
