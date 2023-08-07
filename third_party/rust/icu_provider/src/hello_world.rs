// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider returning multilingual "Hello World" strings for testing.

#![allow(clippy::exhaustive_structs)] // data struct module

#[cfg(feature = "datagen")]
use crate::datagen::IterableDataProvider;
use crate::prelude::*;
use alloc::borrow::Cow;
use alloc::string::String;
use core::fmt::Debug;
use writeable::Writeable;
use yoke::*;
use zerofrom::*;

/// A struct containing "Hello World" in the requested language.
#[derive(Debug, PartialEq, Clone, Yokeable, ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(
    any(feature = "deserialize_json", feature = "datagen"),
    derive(serde::Serialize)
)]
#[cfg_attr(feature = "datagen", derive(databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_provider::hello_world))]
pub struct HelloWorldV1<'data> {
    /// The translation of "Hello World".
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub message: Cow<'data, str>,
}

impl Default for HelloWorldV1<'_> {
    fn default() -> Self {
        HelloWorldV1 {
            message: Cow::Borrowed("(und) Hello World"),
        }
    }
}

/// Marker type for [`HelloWorldV1`].
#[cfg_attr(feature = "datagen", derive(Default, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_provider::hello_world))]
#[derive(Debug)]
pub struct HelloWorldV1Marker;

impl DataMarker for HelloWorldV1Marker {
    type Yokeable = HelloWorldV1<'static>;
}

impl KeyedDataMarker for HelloWorldV1Marker {
    const KEY: DataKey = crate::data_key!("core/helloworld@1");
}

/// A data provider returning Hello World strings in different languages.
///
/// Mostly useful for testing.
///
/// # Examples
///
/// ```
/// use icu_locid::locale;
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
///
/// let german_hello_world: DataPayload<HelloWorldV1Marker> =
///     HelloWorldProvider
///         .load(DataRequest {
///             locale: &locale!("de").into(),
///             metadata: Default::default(),
///         })
///         .expect("Loading should succeed")
///         .take_payload()
///         .expect("Data should be present");
///
/// assert_eq!("Hallo Welt", german_hello_world.get().message);
/// ```
#[derive(Debug, PartialEq, Default)]
pub struct HelloWorldProvider;

impl HelloWorldProvider {
    // Data from https://en.wiktionary.org/wiki/Hello_World#Translations
    // Keep this sorted!
    const DATA: &'static [(&'static str, &'static str)] = &[
        ("bn", "ওহে বিশ্ব"),
        ("cs", "Ahoj světe"),
        ("de", "Hallo Welt"),
        ("el", "Καλημέρα κόσμε"),
        ("en", "Hello World"),
        ("eo", "Saluton, Mondo"),
        ("fa", "سلام دنیا‎"),
        ("fi", "hei maailma"),
        ("is", "Halló, heimur"),
        ("ja", "こんにちは世界"),
        ("la", "Ave, munde"),
        ("pt", "Olá, mundo"),
        ("ro", "Salut, lume"),
        ("ru", "Привет, мир"),
        ("vi", "Xin chào thế giới"),
        ("zh", "你好世界"),
    ];

    /// Converts this provider into a [`BufferProvider`] that uses JSON serialization.
    #[cfg(feature = "deserialize_json")]
    pub fn into_json_provider(self) -> HelloWorldJsonProvider {
        HelloWorldJsonProvider
    }
}

impl DataProvider<HelloWorldV1Marker> for HelloWorldProvider {
    fn load(&self, req: DataRequest) -> Result<DataResponse<HelloWorldV1Marker>, DataError> {
        #[allow(clippy::indexing_slicing)] // binary_search
        let data = Self::DATA
            .binary_search_by(|(k, _)| req.locale.strict_cmp(k.as_bytes()).reverse())
            .map(|i| Self::DATA[i].1)
            .map_err(|_| DataErrorKind::MissingLocale.with_req(HelloWorldV1Marker::KEY, req))?;
        Ok(DataResponse {
            metadata: Default::default(),
            payload: Some(DataPayload::from_static_str(data)),
        })
    }
}

impl DataPayload<HelloWorldV1Marker> {
    /// Make a [`DataPayload`]`<`[`HelloWorldV1Marker`]`>` from a static string slice.
    pub fn from_static_str(s: &'static str) -> DataPayload<HelloWorldV1Marker> {
        DataPayload::from_owned(HelloWorldV1 {
            message: Cow::Borrowed(s),
        })
    }
}

// AnyProvider support.
#[cfg(not(feature = "datagen"))]
impl_dynamic_data_provider!(HelloWorldProvider, [HelloWorldV1Marker,], AnyMarker);

#[cfg(feature = "deserialize_json")]
/// A data provider returning Hello World strings in different languages as JSON blobs.
///
/// Mostly useful for testing.
///
/// # Examples
///
/// ```
/// use icu_locid::locale;
/// use icu_provider::hello_world::*;
/// use icu_provider::prelude::*;
///
/// let german_hello_world = HelloWorldProvider
///     .into_json_provider()
///     .load_buffer(HelloWorldV1Marker::KEY, DataRequest {
///         locale: &locale!("de").into(),
///         metadata: Default::default(),
///     })
///     .expect("Loading should succeed")
///     .take_payload()
///     .expect("Data should be present");
///
/// assert_eq!(german_hello_world.get(), br#"{"message":"Hallo Welt"}"#);
#[derive(Debug)]
pub struct HelloWorldJsonProvider;

#[cfg(feature = "deserialize_json")]
impl BufferProvider for HelloWorldJsonProvider {
    fn load_buffer(
        &self,
        key: DataKey,
        req: DataRequest,
    ) -> Result<DataResponse<BufferMarker>, DataError> {
        key.match_key(HelloWorldV1Marker::KEY)?;
        let result = HelloWorldProvider.load(req)?;
        let (mut metadata, old_payload) =
            DataResponse::<HelloWorldV1Marker>::take_metadata_and_payload(result)?;
        metadata.buffer_format = Some(crate::buf::BufferFormat::Json);
        #[allow(clippy::unwrap_used)] // HelloWorldV1::serialize is infallible
        Ok(DataResponse {
            metadata,
            payload: Some(DataPayload::from_owned_buffer(
                serde_json::to_string(old_payload.get())
                    .unwrap()
                    .into_bytes()
                    .into_boxed_slice(),
            )),
        })
    }
}

#[cfg(feature = "datagen")]
impl IterableDataProvider<HelloWorldV1Marker> for HelloWorldProvider {
    fn supported_locales(&self) -> Result<Vec<DataLocale>, DataError> {
        #[allow(clippy::unwrap_used)] // datagen
        Ok(Self::DATA
            .iter()
            .map(|(s, _)| s.parse::<icu_locid::LanguageIdentifier>().unwrap())
            .map(DataLocale::from)
            .collect())
    }
}

#[cfg(feature = "datagen")]
make_exportable_provider!(HelloWorldProvider, [HelloWorldV1Marker,]);

/// A type that formats localized "hello world" strings.
///
/// This type is intended to take the shape of a typical ICU4X formatter API.
///
/// # Examples
///
/// ```
/// use icu_locid::locale;
/// use icu_provider::hello_world::{HelloWorldFormatter, HelloWorldProvider};
/// use writeable::assert_writeable_eq;
///
/// let fmt = HelloWorldFormatter::try_new_unstable(
///     &HelloWorldProvider,
///     &locale!("eo").into(),
/// )
/// .expect("locale exists");
///
/// assert_writeable_eq!(fmt.format(), "Saluton, Mondo");
/// ```
#[derive(Debug)]
pub struct HelloWorldFormatter {
    data: DataPayload<HelloWorldV1Marker>,
}

/// A formatted hello world message. Implements [`Writeable`].
///
/// For an example, see [`HelloWorldFormatter`].
#[derive(Debug)]
pub struct FormattedHelloWorld<'l> {
    data: &'l HelloWorldV1<'l>,
}

impl HelloWorldFormatter {
    /// Creates a new [`HelloWorldFormatter`] for the specified locale.
    ///
    /// See [`HelloWorldFormatter`] for an example.
    ///
    /// [📚 Help choosing a constructor](crate::constructors)
    /// <div class="stab unstable">
    /// ⚠️ The bounds on this function may change over time, including in SemVer minor releases.
    /// </div>
    pub fn try_new_unstable<P>(provider: &P, locale: &DataLocale) -> Result<Self, DataError>
    where
        P: DataProvider<HelloWorldV1Marker>,
    {
        let data = provider
            .load(DataRequest {
                locale,
                metadata: Default::default(),
            })?
            .take_payload()?;
        Ok(Self { data })
    }

    crate::gen_any_buffer_constructors!(locale: include, options: skip, error: DataError);

    /// Formats a hello world message, returning a [`FormattedHelloWorld`].
    #[allow(clippy::needless_lifetimes)] // documentary example
    pub fn format<'l>(&'l self) -> FormattedHelloWorld<'l> {
        FormattedHelloWorld {
            data: self.data.get(),
        }
    }

    /// Formats a hello world message, returning a [`String`].
    pub fn format_to_string(&self) -> String {
        self.format().write_to_string().into_owned()
    }
}

impl<'l> Writeable for FormattedHelloWorld<'l> {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        self.data.message.write_to(sink)
    }

    fn write_to_string(&self) -> Cow<str> {
        self.data.message.clone()
    }

    fn writeable_length_hint(&self) -> writeable::LengthHint {
        self.data.message.writeable_length_hint()
    }
}

writeable::impl_display_with_writeable!(FormattedHelloWorld<'_>);

#[cfg(feature = "datagen")]
#[test]
fn test_iter() {
    use icu_locid::locale;

    assert_eq!(
        HelloWorldProvider.supported_locales().unwrap(),
        vec![
            locale!("bn").into(),
            locale!("cs").into(),
            locale!("de").into(),
            locale!("el").into(),
            locale!("en").into(),
            locale!("eo").into(),
            locale!("fa").into(),
            locale!("fi").into(),
            locale!("is").into(),
            locale!("ja").into(),
            locale!("la").into(),
            locale!("pt").into(),
            locale!("ro").into(),
            locale!("ru").into(),
            locale!("vi").into(),
            locale!("zh").into()
        ]
    );
}
