//! The Visage AST (abstract syntax tree).

pub mod arena;
pub mod associated_data;
pub mod source_location;

mod source_location_accessor_generated;
pub mod source_location_accessor {
    pub use crate::source_location_accessor_generated::*;
}
mod types_generated;
pub mod types {
    pub use crate::types_generated::*;
}
mod visit_generated;
pub mod visit {
    pub use crate::visit_generated::*;
}
mod type_id_generated;
pub mod type_id {
    pub use crate::type_id_generated::*;
}

pub use source_location::SourceLocation;
