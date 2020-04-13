#[cfg(feature = "spirv_cross")]
use {
    hal::{device::ShaderError, pso},
    spirv_cross::spirv,
};

/// Fast hash map used internally.
pub type FastHashMap<K, V> =
    std::collections::HashMap<K, V, std::hash::BuildHasherDefault<fxhash::FxHasher>>;
pub type FastHashSet<K> =
    std::collections::HashSet<K, std::hash::BuildHasherDefault<fxhash::FxHasher>>;

#[cfg(feature = "spirv_cross")]
pub fn spirv_cross_specialize_ast<T>(
    ast: &mut spirv::Ast<T>,
    specialization: &pso::Specialization,
) -> Result<(), ShaderError>
where
    T: spirv::Target,
    spirv::Ast<T>: spirv::Compile<T> + spirv::Parse<T>,
{
    let spec_constants = ast.get_specialization_constants().map_err(|err| {
        ShaderError::CompilationFailed(match err {
            spirv_cross::ErrorCode::CompilationError(msg) => msg,
            spirv_cross::ErrorCode::Unhandled => "Unexpected specialization constant error".into(),
        })
    })?;

    for spec_constant in spec_constants {
        if let Some(constant) = specialization
            .constants
            .iter()
            .find(|c| c.id == spec_constant.constant_id)
        {
            // Override specialization constant values
            let value = specialization.data
                [constant.range.start as usize .. constant.range.end as usize]
                .iter()
                .rev()
                .fold(0u64, |u, &b| (u << 8) + b as u64);

            ast.set_scalar_constant(spec_constant.id, value)
                .map_err(|err| {
                    ShaderError::CompilationFailed(match err {
                        spirv_cross::ErrorCode::CompilationError(msg) => msg,
                        spirv_cross::ErrorCode::Unhandled => {
                            "Unexpected specialization constant error".into()
                        }
                    })
                })?;
        }
    }

    Ok(())
}
