use crate::internal::FastStorageMap;
use crate::native::SerializableModuleInfo;
use std::fmt;
use std::sync::atomic::AtomicBool;

pub(crate) struct BinaryArchive {
    pub(crate) inner: metal::BinaryArchive,
    pub(crate) is_empty: AtomicBool,
}

unsafe impl Send for BinaryArchive {}

unsafe impl Sync for BinaryArchive {}

#[derive(serde::Serialize, serde::Deserialize, Clone, PartialEq, Eq, Hash)]
pub(crate) struct SpvToMslKey {
    pub(crate) options: naga::back::msl::Options,
    pub(crate) pipeline_options: naga::back::msl::PipelineOptions,
    pub(crate) spv_hash: u64,
}

pub(crate) type SpvToMsl = FastStorageMap<SpvToMslKey, SerializableModuleInfo>;

pub(crate) type SerializableSpvToMsl = Vec<(SpvToMslKey, SerializableModuleInfo)>;

pub(crate) fn load_spv_to_msl_cache(serializable: SerializableSpvToMsl) -> SpvToMsl {
    let cache = FastStorageMap::default();
    for (options, values) in serializable.into_iter() {
        cache.get_or_create_with(&options, || values);
    }

    cache
}

pub(crate) fn serialize_spv_to_msl_cache(cache: &SpvToMsl) -> SerializableSpvToMsl {
    cache
        .whole_write()
        .iter()
        .map(|(options, values)| (options.clone(), values.clone()))
        .collect()
}

#[derive(serde::Serialize, serde::Deserialize)]
pub(crate) struct SerializablePipelineCache<'a> {
    pub(crate) binary_archive: &'a [u8],
    pub(crate) spv_to_msl: SerializableSpvToMsl,
}

pub struct PipelineCache {
    pub(crate) binary_archive: Option<BinaryArchive>,
    pub(crate) spv_to_msl: SpvToMsl,
}

impl fmt::Debug for PipelineCache {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "PipelineCache")
    }
}

pub(crate) fn pipeline_cache_to_binary_archive(
    pipeline_cache: Option<&PipelineCache>,
) -> Option<&BinaryArchive> {
    pipeline_cache.and_then(|cache| cache.binary_archive.as_ref())
}
