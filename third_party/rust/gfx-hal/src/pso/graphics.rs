//! Graphics pipeline descriptor.

use crate::{
    image,
    pass,
    pso::{
        input_assembler::{AttributeDesc, InputAssemblerDesc, Primitive, VertexBufferDesc},
        output_merger::{ColorBlendDesc, DepthStencilDesc, Face},
        BasePipeline,
        EntryPoint,
        PipelineCreationFlags,
        State,
    },
    Backend,
};

use std::ops::Range;

/// A simple struct describing a rect with integer coordinates.
#[derive(Clone, Copy, Debug, Hash, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Rect {
    /// X position.
    pub x: i16,
    /// Y position.
    pub y: i16,
    /// Width.
    pub w: i16,
    /// Height.
    pub h: i16,
}

/// A simple struct describing a rect with integer coordinates.
#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct ClearRect {
    /// 2D region.
    pub rect: Rect,
    /// Layer range.
    pub layers: Range<image::Layer>,
}

/// A viewport, generally equating to a window on a display.
#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Viewport {
    /// The viewport boundaries.
    pub rect: Rect,
    /// The viewport depth limits.
    pub depth: Range<f32>,
}

/// A single RGBA float color.
pub type ColorValue = [f32; 4];
/// A single depth value from a depth buffer.
pub type DepthValue = f32;
/// A single value from a stencil buffer.
pub type StencilValue = u32;

/// A complete set of shaders to build a graphics pipeline.
///
/// All except the vertex shader are optional; omitting them
/// passes through the inputs without change.
///
/// If a fragment shader is omitted, the results of fragment
/// processing are undefined. Specifically, any fragment color
/// outputs are considered to have undefined values, and the
/// fragment depth is considered to be unmodified. This can
/// be useful for depth-only rendering.
#[derive(Clone, Debug)]
pub struct GraphicsShaderSet<'a, B: Backend> {
    /// A shader that outputs a vertex in a model.
    pub vertex: EntryPoint<'a, B>,
    /// A hull shader takes in an input patch (values representing
    /// a small portion of a shape, which may be actual geometry or may
    /// be parameters for creating geometry) and produces one or more
    /// output patches.
    pub hull: Option<EntryPoint<'a, B>>,
    /// A shader that takes in domains produced from a hull shader's output
    /// patches and computes actual vertex positions.
    pub domain: Option<EntryPoint<'a, B>>,
    /// A shader that takes given input vertexes and outputs zero
    /// or more output vertexes.
    pub geometry: Option<EntryPoint<'a, B>>,
    /// A shader that outputs a value for a fragment.
    /// Usually this value is a color that is then displayed as a
    /// pixel on a screen.
    pub fragment: Option<EntryPoint<'a, B>>,
}

/// Baked-in pipeline states.
#[derive(Clone, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct BakedStates {
    /// Static viewport. TODO: multiple viewports
    pub viewport: Option<Viewport>,
    /// Static scissor. TODO: multiple scissors
    pub scissor: Option<Rect>,
    /// Static blend constant color.
    pub blend_color: Option<ColorValue>,
    /// Static depth bounds.
    pub depth_bounds: Option<Range<f32>>,
}

/// A description of all the settings that can be altered
/// when creating a graphics pipeline.
#[derive(Debug)]
pub struct GraphicsPipelineDesc<'a, B: Backend> {
    /// A set of graphics shaders to use for the pipeline.
    pub shaders: GraphicsShaderSet<'a, B>,
    /// Rasterizer setup
    pub rasterizer: Rasterizer,
    /// Vertex buffers (IA)
    pub vertex_buffers: Vec<VertexBufferDesc>,
    /// Vertex attributes (IA)
    pub attributes: Vec<AttributeDesc>,
    /// Input assembler attributes, describes how
    /// vertices are assembled into primitives (such as triangles).
    pub input_assembler: InputAssemblerDesc,
    /// Description of how blend operations should be performed.
    pub blender: BlendDesc,
    /// Depth stencil (DSV)
    pub depth_stencil: DepthStencilDesc,
    /// Multisampling.
    pub multisampling: Option<Multisampling>,
    /// Static pipeline states.
    pub baked_states: BakedStates,
    /// Pipeline layout.
    pub layout: &'a B::PipelineLayout,
    /// Subpass in which the pipeline can be executed.
    pub subpass: pass::Subpass<'a, B>,
    /// Options that may be set to alter pipeline properties.
    pub flags: PipelineCreationFlags,
    /// The parent pipeline, which may be
    /// `BasePipeline::None`.
    pub parent: BasePipeline<'a, B::GraphicsPipeline>,
}

impl<'a, B: Backend> GraphicsPipelineDesc<'a, B> {
    /// Create a new empty PSO descriptor.
    pub fn new(
        shaders: GraphicsShaderSet<'a, B>,
        primitive: Primitive,
        rasterizer: Rasterizer,
        layout: &'a B::PipelineLayout,
        subpass: pass::Subpass<'a, B>,
    ) -> Self {
        GraphicsPipelineDesc {
            shaders,
            rasterizer,
            vertex_buffers: Vec::new(),
            attributes: Vec::new(),
            input_assembler: InputAssemblerDesc::new(primitive),
            blender: BlendDesc::default(),
            depth_stencil: DepthStencilDesc::default(),
            multisampling: None,
            baked_states: BakedStates::default(),
            layout,
            subpass,
            flags: PipelineCreationFlags::empty(),
            parent: BasePipeline::None,
        }
    }
}

/// Methods for rasterizing polygons, ie, turning the mesh
/// into a raster image.
#[derive(Clone, Copy, Debug, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum PolygonMode {
    /// Rasterize as a point.
    Point,
    /// Rasterize as a line with the given width.
    Line,
    /// Rasterize as a face.
    Fill,
}

/// The front face winding order of a set of vertices. This is
/// the order of vertexes that define which side of a face is
/// the "front".
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum FrontFace {
    /// Clockwise winding order.
    Clockwise,
    /// Counter-clockwise winding order.
    CounterClockwise,
}

/// A depth bias allows changing the produced depth values
/// for fragments slightly but consistently. This permits
/// drawing of multiple polygons in the same plane without
/// Z-fighting, such as when trying to draw shadows on a wall.
///
/// For details of the algorithm and equations, see
/// [the Vulkan spec](https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#primsrast-depthbias).
#[derive(Copy, Clone, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct DepthBias {
    /// A constant depth value added to each fragment.
    pub const_factor: f32,
    /// The minimum or maximum depth bias of a fragment.
    pub clamp: f32,
    /// A constant bias applied to the fragment's slope.
    pub slope_factor: f32,
}

/// Rasterization state.
#[derive(Copy, Clone, Debug, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Rasterizer {
    /// How to rasterize this primitive.
    pub polygon_mode: PolygonMode,
    /// Which face should be culled.
    pub cull_face: Face,
    /// Which vertex winding is considered to be the front face for culling.
    pub front_face: FrontFace,
    /// Whether or not to enable depth clamping; when enabled, instead of
    /// fragments being omitted when they are outside the bounds of the z-plane,
    /// they will be clamped to the min or max z value.
    pub depth_clamping: bool,
    /// What depth bias, if any, to use for the drawn primitives.
    pub depth_bias: Option<State<DepthBias>>,
    /// Controls how triangles will be rasterized depending on their overlap with pixels.
    pub conservative: bool,
    /// Controls width of rasterized line segments.
    pub line_width: State<f32>,
}

impl Rasterizer {
    /// Simple polygon-filling rasterizer state
    pub const FILL: Self = Rasterizer {
        polygon_mode: PolygonMode::Fill,
        cull_face: Face::NONE,
        front_face: FrontFace::CounterClockwise,
        depth_clamping: false,
        depth_bias: None,
        conservative: false,
        line_width: State::Static(1.0),
    };
}

/// A description of an equation for how to blend transparent, overlapping fragments.
#[derive(Clone, Debug, Default, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct BlendDesc {
    /// The logic operation to apply to the blending equation, if any.
    pub logic_op: Option<LogicOp>,
    /// Which color targets to apply the blending operation to.
    pub targets: Vec<ColorBlendDesc>,
}

/// Logic operations used for specifying blend equations.
#[derive(Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[allow(missing_docs)]
pub enum LogicOp {
    Clear = 0,
    And = 1,
    AndReverse = 2,
    Copy = 3,
    AndInverted = 4,
    NoOp = 5,
    Xor = 6,
    Or = 7,
    Nor = 8,
    Equivalent = 9,
    Invert = 10,
    OrReverse = 11,
    CopyInverted = 12,
    OrInverted = 13,
    Nand = 14,
    Set = 15,
}

///
pub type SampleMask = u64;

///
#[derive(Clone, Debug, PartialEq)]
pub struct Multisampling {
    ///
    pub rasterization_samples: image::NumSamples,
    ///
    pub sample_shading: Option<f32>,
    ///
    pub sample_mask: SampleMask,
    /// Toggles alpha-to-coverage multisampling, which can produce nicer edges
    /// when many partially-transparent polygons are overlapping.
    /// See [here]( https://msdn.microsoft.com/en-us/library/windows/desktop/bb205072(v=vs.85).aspx#Alpha_To_Coverage) for a full description.
    pub alpha_coverage: bool,
    ///
    pub alpha_to_one: bool,
}
