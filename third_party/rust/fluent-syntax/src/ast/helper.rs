#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

use super::Comment;
// This is a helper struct used to properly deserialize referential
// JSON comments which are single continous String, into a vec of
// content slices.
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(untagged))]
pub enum CommentDef<S> {
    Single { content: S },
    Multi { content: Vec<S> },
}

impl<'s, S> From<CommentDef<S>> for Comment<S> {
    fn from(input: CommentDef<S>) -> Self {
        match input {
            CommentDef::Single { content } => Self {
                content: vec![content],
            },
            CommentDef::Multi { content } => Self { content },
        }
    }
}
