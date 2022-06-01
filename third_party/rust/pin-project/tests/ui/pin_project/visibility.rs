mod pub_ {
    use pin_project::pin_project;

    #[pin_project]
    pub struct Default(());

    #[pin_project(project_replace)]
    pub struct Replace(());
}
pub mod pub_use {
    #[rustfmt::skip]
    pub use crate::pub_::__DefaultProjection; //~ ERROR E0365
    #[rustfmt::skip]
    pub use crate::pub_::__DefaultProjectionRef; //~ ERROR E0365
    #[rustfmt::skip]
    pub use crate::pub_::__ReplaceProjection; //~ ERROR E0365
    #[rustfmt::skip]
    pub use crate::pub_::__ReplaceProjectionOwned; //~ ERROR E0365
    #[rustfmt::skip]
    pub use crate::pub_::__ReplaceProjectionRef; //~ ERROR E0365

    // Confirm that the visibility of the original type is not changed.
    pub use crate::pub_::{Default, Replace};
}
pub mod pub_use2 {
    // Ok
    #[allow(unused_imports)]
    pub(crate) use crate::pub_::{
        __DefaultProjection, __DefaultProjectionRef, __ReplaceProjection, __ReplaceProjectionOwned,
        __ReplaceProjectionRef,
    };
}

mod pub_crate {
    use pin_project::pin_project;

    #[pin_project]
    pub(crate) struct Default(());

    #[pin_project(project_replace)]
    pub(crate) struct Replace(());
}
pub mod pub_crate_use {
    // Ok
    #[allow(unused_imports)]
    pub(crate) use crate::pub_crate::{
        __DefaultProjection, __DefaultProjectionRef, __ReplaceProjection, __ReplaceProjectionOwned,
        __ReplaceProjectionRef,
    };
}

mod pub_renamed {
    use pin_project::pin_project;

    #[pin_project(project = DProj, project_ref = DProjRef)]
    pub struct Default(());

    #[pin_project(project = RProj, project_ref = RProjRef, project_replace = RProjOwn)]
    pub struct Replace(());
}
pub mod pub_renamed_use {
    #[rustfmt::skip]
    pub use crate::pub_renamed::DProj; //~ ERROR E0365
    #[rustfmt::skip]
    pub use crate::pub_renamed::DProjRef; //~ ERROR E0365
    #[rustfmt::skip]
    pub use crate::pub_renamed::RProj; //~ ERROR E0365
    #[rustfmt::skip]
    pub use crate::pub_renamed::RProjOwn; //~ ERROR E0365
    #[rustfmt::skip]
    pub use crate::pub_renamed::RProjRef; //~ ERROR E0365

    // Confirm that the visibility of the original type is not changed.
    pub use crate::pub_renamed::{Default, Replace};
}
pub mod pub_renamed_use2 {
    // Ok
    #[allow(unused_imports)]
    pub(crate) use crate::pub_renamed::{DProj, DProjRef, RProj, RProjOwn, RProjRef};
}

fn main() {}
