// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::dynutil::UpcastDataPayload;
use crate::prelude::*;
use alloc::boxed::Box;
use databake::{Bake, CrateEnv, TokenStream};
use yoke::*;

trait ExportableYoke {
    fn bake_yoke(&self, env: &CrateEnv) -> TokenStream;
    fn serialize_yoke(
        &self,
        serializer: &mut dyn erased_serde::Serializer,
    ) -> Result<(), DataError>;
}

impl<Y, C> ExportableYoke for Yoke<Y, C>
where
    Y: for<'a> Yokeable<'a>,
    for<'a> <Y as Yokeable<'a>>::Output: Bake + serde::Serialize,
{
    fn bake_yoke(&self, ctx: &CrateEnv) -> TokenStream {
        self.get().bake(ctx)
    }

    fn serialize_yoke(
        &self,
        serializer: &mut dyn erased_serde::Serializer,
    ) -> Result<(), DataError> {
        use erased_serde::Serialize;
        self.get()
            .erased_serialize(serializer)
            .map_err(|e| DataError::custom("Serde export").with_display_context(&e))?;
        Ok(())
    }
}

#[doc(hidden)] // exposed for make_exportable_provider
#[derive(yoke::Yokeable)]
pub struct ExportBox {
    payload: Box<dyn ExportableYoke + Sync>,
}

impl core::fmt::Debug for ExportBox {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("ExportBox")
            .field("payload", &"<payload>")
            .finish()
    }
}

impl<M> UpcastDataPayload<M> for ExportMarker
where
    M: DataMarker,
    M::Yokeable: Sync,
    for<'a> <M::Yokeable as Yokeable<'a>>::Output: Bake + serde::Serialize,
{
    fn upcast(other: DataPayload<M>) -> DataPayload<ExportMarker> {
        DataPayload::from_owned(ExportBox {
            payload: Box::new(other.yoke),
        })
    }
}

impl DataPayload<ExportMarker> {
    /// Serializes this [`DataPayload`] into a serializer using Serde.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_provider::datagen::*;
    /// use icu_provider::dynutil::UpcastDataPayload;
    /// use icu_provider::hello_world::HelloWorldV1Marker;
    /// use icu_provider::prelude::*;
    ///
    /// // Create an example DataPayload
    /// let payload: DataPayload<HelloWorldV1Marker> = Default::default();
    /// let export: DataPayload<ExportMarker> = UpcastDataPayload::upcast(payload);
    ///
    /// // Serialize the payload to a JSON string
    /// let mut buffer: Vec<u8> = vec![];
    /// export
    ///     .serialize(&mut serde_json::Serializer::new(&mut buffer))
    ///     .expect("Serialization should succeed");
    /// assert_eq!(r#"{"message":"(und) Hello World"}"#.as_bytes(), buffer);
    /// ```
    pub fn serialize<S>(&self, serializer: S) -> Result<(), DataError>
    where
        S: serde::Serializer,
        S::Ok: 'static, // erased_serde requirement, cannot return values in `Ok`
    {
        self.get()
            .payload
            .serialize_yoke(&mut <dyn erased_serde::Serializer>::erase(serializer))
    }

    /// Serializes this [`DataPayload`]'s value into a [`TokenStream`]
    /// using its [`Bake`] implementations.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_provider::datagen::*;
    /// use icu_provider::dynutil::UpcastDataPayload;
    /// use icu_provider::hello_world::HelloWorldV1Marker;
    /// use icu_provider::prelude::*;
    /// # use databake::quote;
    /// # use std::collections::BTreeSet;
    ///
    /// // Create an example DataPayload
    /// let payload: DataPayload<HelloWorldV1Marker> = Default::default();
    /// let export: DataPayload<ExportMarker> = UpcastDataPayload::upcast(payload);
    ///
    /// let env = databake::CrateEnv::default();
    /// let tokens = export.tokenize(&env);
    /// assert_eq!(
    ///     quote! {
    ///         ::icu_provider::hello_world::HelloWorldV1 {
    ///             message: alloc::borrow::Cow::Borrowed("(und) Hello World"),
    ///         }
    ///     }
    ///     .to_string(),
    ///     tokens.to_string()
    /// );
    /// assert_eq!(
    ///     env.into_iter().collect::<BTreeSet<_>>(),
    ///     ["icu_provider", "alloc"]
    ///         .into_iter()
    ///         .collect::<BTreeSet<_>>()
    /// );
    /// ```
    pub fn tokenize(&self, env: &CrateEnv) -> TokenStream {
        self.get().payload.bake_yoke(env)
    }
}

/// Marker type for [`ExportBox`].
#[allow(clippy::exhaustive_structs)] // marker type
#[derive(Debug)]
pub struct ExportMarker {}

impl DataMarker for ExportMarker {
    type Yokeable = ExportBox;
}
