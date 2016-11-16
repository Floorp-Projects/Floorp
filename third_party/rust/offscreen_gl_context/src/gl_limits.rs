use gleam::gl;
#[cfg(feature = "serde")]
use serde::{Deserialize, Deserializer, Serialize, Serializer};

#[derive(Debug, Clone)]
pub struct GLLimits {
    pub max_vertex_attribs: u32,
    pub max_tex_size: u32,
    pub max_cube_map_tex_size: u32
}

#[cfg(feature = "serde")]
impl Deserialize for GLLimits {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
        where D: Deserializer
    {
        let values = try!(<[_; 3]>::deserialize(deserializer));
        Ok(GLLimits {
            max_vertex_attribs: values[0],
            max_tex_size: values[1],
            max_cube_map_tex_size: values[2],
        })
    }
}

#[cfg(feature = "serde")]
impl Serialize for GLLimits {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
        where S: Serializer
    {
        [self.max_vertex_attribs, self.max_tex_size, self.max_cube_map_tex_size]
            .serialize(serializer)
    }
}

impl GLLimits {
    pub fn detect() -> GLLimits {
        GLLimits {
            max_vertex_attribs: gl::get_integer_v(gl::MAX_VERTEX_ATTRIBS) as u32,
            max_tex_size: gl::get_integer_v(gl::MAX_TEXTURE_SIZE) as u32,
            max_cube_map_tex_size: gl::get_integer_v(gl::MAX_CUBE_MAP_TEXTURE_SIZE) as u32
        }
    }
}
