#[cfg(feature = "serde")]
use serde::{Deserialize, Deserializer, Serialize, Serializer};

/// This structure represents the attributes the context must support
/// It's almost (if not) identical to WebGLGLContextAttributes
#[derive(Clone, Debug, Copy)]
pub struct GLContextAttributes {
    pub alpha: bool,
    pub depth: bool,
    pub stencil: bool,
    pub antialias: bool,
    pub premultiplied_alpha: bool,
    pub preserve_drawing_buffer: bool,
}

#[cfg(feature = "serde")]
impl Deserialize for GLContextAttributes {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
        where D: Deserializer
    {
        let values = try!(<[_; 6]>::deserialize(deserializer));
        Ok(GLContextAttributes {
            alpha: values[0],
            depth: values[1],
            stencil: values[2],
            antialias: values[3],
            premultiplied_alpha: values[4],
            preserve_drawing_buffer: values[5],
        })
    }
}

#[cfg(feature = "serde")]
impl Serialize for GLContextAttributes {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
        where S: Serializer
    {
        let values = [
            self.alpha, self.depth, self.stencil,
            self.antialias, self.premultiplied_alpha, self.preserve_drawing_buffer,
        ];
        values.serialize(serializer)
    }
}

impl GLContextAttributes {
    pub fn any() -> GLContextAttributes {
        GLContextAttributes {
            alpha: false,
            depth: false,
            stencil: false,
            antialias: false,
            premultiplied_alpha: false,
            preserve_drawing_buffer: false,
        }
    }
}

impl Default for GLContextAttributes {
    // FIXME(ecoal95): `antialias` should be true by default
    //   but we do not support antialising so... We must change it
    //   when we do. See GLFeature.
    fn default() -> GLContextAttributes {
        GLContextAttributes {
            alpha: true,
            depth: true,
            stencil: false,
            antialias: false,
            premultiplied_alpha: true,
            preserve_drawing_buffer: false
        }
    }
}
