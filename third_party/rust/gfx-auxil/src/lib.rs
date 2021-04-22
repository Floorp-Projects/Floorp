#[cfg(feature = "spirv_cross")]
use spirv_cross::spirv;
use std::{io, slice};

/// Fast hash map used internally.
pub type FastHashMap<K, V> =
    std::collections::HashMap<K, V, std::hash::BuildHasherDefault<fxhash::FxHasher>>;
pub type FastHashSet<K> =
    std::collections::HashSet<K, std::hash::BuildHasherDefault<fxhash::FxHasher>>;

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum ShaderStage {
    Vertex,
    Hull,
    Domain,
    Geometry,
    Fragment,
    Compute,
    Task,
    Mesh,
}

impl ShaderStage {
    pub fn to_flag(self) -> hal::pso::ShaderStageFlags {
        use hal::pso::ShaderStageFlags as Ssf;
        match self {
            ShaderStage::Vertex => Ssf::VERTEX,
            ShaderStage::Hull => Ssf::HULL,
            ShaderStage::Domain => Ssf::DOMAIN,
            ShaderStage::Geometry => Ssf::GEOMETRY,
            ShaderStage::Fragment => Ssf::FRAGMENT,
            ShaderStage::Compute => Ssf::COMPUTE,
            ShaderStage::Task => Ssf::TASK,
            ShaderStage::Mesh => Ssf::MESH,
        }
    }
}

/// Safely read SPIR-V
///
/// Converts to native endianness and returns correctly aligned storage without unnecessary
/// copying. Returns an `InvalidData` error if the input is trivially not SPIR-V.
///
/// This function can also be used to convert an already in-memory `&[u8]` to a valid `Vec<u32>`,
/// but prefer working with `&[u32]` from the start whenever possible.
///
/// # Examples
/// ```no_run
/// let mut file = std::fs::File::open("/path/to/shader.spv").unwrap();
/// let words = gfx_auxil::read_spirv(&mut file).unwrap();
/// ```
/// ```
/// const SPIRV: &[u8] = &[
///     0x03, 0x02, 0x23, 0x07, // ...
/// ];
/// let words = gfx_auxil::read_spirv(std::io::Cursor::new(&SPIRV[..])).unwrap();
/// ```
pub fn read_spirv<R: io::Read + io::Seek>(mut x: R) -> io::Result<Vec<u32>> {
    let size = x.seek(io::SeekFrom::End(0))?;
    if size % 4 != 0 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "input length not divisible by 4",
        ));
    }
    if size > usize::MAX as u64 {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "input too long"));
    }
    let words = (size / 4) as usize;
    let mut result = Vec::<u32>::with_capacity(words);
    x.seek(io::SeekFrom::Start(0))?;
    unsafe {
        // Writing all bytes through a pointer with less strict alignment when our type has no
        // invalid bitpatterns is safe.
        x.read_exact(slice::from_raw_parts_mut(
            result.as_mut_ptr() as *mut u8,
            words * 4,
        ))?;
        result.set_len(words);
    }
    const MAGIC_NUMBER: u32 = 0x07230203;
    if result.len() > 0 && result[0] == MAGIC_NUMBER.swap_bytes() {
        for word in &mut result {
            *word = word.swap_bytes();
        }
    }
    if result.len() == 0 || result[0] != MAGIC_NUMBER {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "input missing SPIR-V magic number",
        ));
    }
    Ok(result)
}

#[cfg(feature = "spirv_cross")]
pub fn spirv_cross_specialize_ast<T>(
    ast: &mut spirv::Ast<T>,
    specialization: &hal::pso::Specialization,
) -> Result<(), String>
where
    T: spirv::Target,
    spirv::Ast<T>: spirv::Compile<T> + spirv::Parse<T>,
{
    let spec_constants = ast
        .get_specialization_constants()
        .map_err(|err| match err {
            spirv_cross::ErrorCode::CompilationError(msg) => msg,
            spirv_cross::ErrorCode::Unhandled => "Unexpected specialization constant error".into(),
        })?;

    for spec_constant in spec_constants {
        if let Some(constant) = specialization
            .constants
            .iter()
            .find(|c| c.id == spec_constant.constant_id)
        {
            // Override specialization constant values
            let value = specialization.data
                [constant.range.start as usize..constant.range.end as usize]
                .iter()
                .rev()
                .fold(0u64, |u, &b| (u << 8) + b as u64);

            ast.set_scalar_constant(spec_constant.id, value)
                .map_err(|err| match err {
                    spirv_cross::ErrorCode::CompilationError(msg) => msg,
                    spirv_cross::ErrorCode::Unhandled => {
                        "Unexpected specialization constant error".into()
                    }
                })?;
        }
    }

    Ok(())
}
