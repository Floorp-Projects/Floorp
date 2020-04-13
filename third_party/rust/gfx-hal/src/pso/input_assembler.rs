//! Input Assembler (IA) stage description.
//! The input assembler collects raw vertex and index data.

use crate::{format, IndexType};

/// Shader binding location.
pub type Location = u32;
/// Index of a vertex buffer.
pub type BufferIndex = u32;
/// Offset of an attribute from the start of the buffer, in bytes
pub type ElemOffset = u32;
/// Offset between attribute values, in bytes
pub type ElemStride = u32;
/// Number of instances between each advancement of the vertex buffer.
pub type InstanceRate = u8;
/// Number of vertices in a patch
pub type PatchSize = u8;

/// The rate at which to advance input data to shaders for the given buffer
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum VertexInputRate {
    /// Advance the buffer after every vertex
    Vertex,
    /// Advance the buffer after every instance
    Instance(InstanceRate),
}

impl VertexInputRate {
    /// Get the numeric representation of the rate
    pub fn as_uint(&self) -> u8 {
        match *self {
            VertexInputRate::Vertex => 0,
            VertexInputRate::Instance(divisor) => divisor,
        }
    }
}

/// A struct element descriptor.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Element<F> {
    /// Element format
    pub format: F,
    /// Offset from the beginning of the container, in bytes
    pub offset: ElemOffset,
}

/// Vertex buffer description. Notably, completely separate from resource `Descriptor`s
/// used in `DescriptorSet`s.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct VertexBufferDesc {
    /// Binding number of this vertex buffer. This binding number is
    /// used only for vertex buffers, and is completely separate from
    /// `Descriptor` and `DescriptorSet` bind points.
    pub binding: BufferIndex,
    /// Total container size, in bytes.
    /// Specifies the byte distance between two consecutive elements.
    pub stride: ElemStride,
    /// The rate at which to advance data for the given buffer
    ///
    /// i.e. the rate at which data passed to shaders will get advanced by
    /// `stride` bytes
    pub rate: VertexInputRate,
}

/// Vertex attribute description. Notably, completely separate from resource `Descriptor`s
/// used in `DescriptorSet`s.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct AttributeDesc {
    /// Attribute binding location in the shader. Attribute locations are
    /// shared between all vertex buffers in a pipeline, meaning that even if the
    /// data for this attribute comes from a different vertex buffer, it still cannot
    /// share the same location with another attribute.
    pub location: Location,
    /// Binding number of the associated vertex buffer.
    pub binding: BufferIndex,
    /// Attribute element description.
    pub element: Element<format::Format>,
}

/// Describes the type of geometric primitives,
/// created from vertex data.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum Primitive {
    /// Each vertex represents a single point.
    PointList,
    /// Each pair of vertices represent a single line segment. For example, with `[a, b, c, d,
    /// e]`, `a` and `b` form a line, `c` and `d` form a line, and `e` is discarded.
    LineList,
    /// Every two consecutive vertices represent a single line segment. Visually forms a "path" of
    /// lines, as they are all connected. For example, with `[a, b, c]`, `a` and `b` form a line
    /// line, and `b` and `c` form a line.
    LineStrip,
    /// Each triplet of vertices represent a single triangle. For example, with `[a, b, c, d, e]`,
    /// `a`, `b`, and `c` form a triangle, `d` and `e` are discarded.
    TriangleList,
    /// Every three consecutive vertices represent a single triangle. For example, with `[a, b, c,
    /// d]`, `a`, `b`, and `c` form a triangle, and `b`, `c`, and `d` form a triangle.
    TriangleStrip,
    /// Patch list,
    /// used with shaders capable of producing primitives on their own (tessellation)
    PatchList(PatchSize),
}

/// All the information needed to create an input assembler.
#[derive(Clone, Debug, Eq, PartialEq, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct InputAssemblerDesc {
    /// Type of the primitive
    pub primitive: Primitive,
    /// When adjacency information is enabled, every even-numbered vertex
    /// (every other starting from the first) represents an additional
    /// vertex for the primitive, while odd-numbered vertices (every other starting from the
    /// second) represent adjacent vertices.
    ///
    /// For example, with `[a, b, c, d, e, f, g, h]`, `[a, c,
    /// e, g]` form a triangle strip, and `[b, d, f, h]` are the adjacent vertices, where `b`, `d`,
    /// and `f` are adjacent to the first triangle in the strip, and `d`, `f`, and `h` are adjacent
    /// to the second.
    pub with_adjacency: bool,
    /// Describes whether or not primitive restart is supported for
    /// an input assembler. Primitive restart is a feature that
    /// allows a mark to be placed in an index buffer where it is
    /// is "broken" into multiple pieces of geometry.
    ///
    /// See <https://www.khronos.org/opengl/wiki/Vertex_Rendering#Primitive_Restart>
    /// for more detail.
    pub restart_index: Option<IndexType>,
}

impl InputAssemblerDesc {
    /// Create a new IA descriptor without primitive restart or adjucency.
    pub fn new(primitive: Primitive) -> Self {
        InputAssemblerDesc {
            primitive,
            with_adjacency: false,
            restart_index: None,
        }
    }
}
