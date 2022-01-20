# Change Log

## v0.7.1 (2021-10-12)
  - implement casts from and to booleans in the backends

## v0.7 (2021-10-07)
  - development release for wgpu-0.11
  - API:
    - bit extraction and packing functions
    - hyperbolic trigonometry functionss
    - validation is gated by a cargo feature
    - `view_index` builtin
    - separate bounds checking policies for locals/buffers/textures
  - IR:
    - types and constants are guaranteed to be unique
  - WGSL-in:
    - new hex literal parser
    - updated list of reserved words
    - rewritten logic for resolving references and pointers
    - `switch` can use unsigned selectors
  - GLSL-in:
    - better support for texture sampling
    - better logic for auto-splatting scalars
  - GLSL-out:
    - fixed storage buffer layout
    - fix module operator
  - HLSL-out:
    - fixed texture queries
  - SPV-in:
    - control flow handling is rewritten from scratch
  - SPV-out:
    - fully covered out-of-bounds checking
    - option to emit point size
    - option to clamp output depth

### v0.6.3 (2021-09-08)
  - Reduced heap allocations when generating WGSL, HLSL, and GLSL
  - WGSL-in:
    - support module-scope `let` type inference
  - SPV-in:
    - fix depth sampling with projection
  - HLSL-out:
    - fix local struct construction
  - GLSL-out:
    - fix `select()` order
  - SPV-out:
    - allow working around Adreno issue with `OpName`

### v0.6.2 (2021-09-01)
  - SPV-out fixes:
    - requested capabilities for 1D and cube images, storage formats
    - handling `break` and `continue` in a `switch` statement
    - avoid generating duplicate `OpTypeImage` types
  - HLSL-out fixes:
    - fix output struct member names
  - MSL-out fixes:
    - fix packing of fields in interface structs
  - GLSL-out fixes:
    - fix non-fallthrough `switch` cases
  - GLSL-in fixes:
    - avoid infinite loop on invalid statements

### v0.6.1 (2021-08-24)
  - HLSL-out fixes:
    - array arguments
    - pointers to array arguments
    - switch statement
    - rewritten interface matching
  - SPV-in fixes:
    - array storage texture stores
    - tracking sampling across function parameters
    - updated petgraph dependencies
  - MSL-out:
    - gradient sampling
  - GLSL-out:
    - modulo operator on floats

## v0.6 (2021-08-18)
  - development release for wgpu-0.10
  - API:
    - atomic types and functions
    - storage access is moved from global variables to the storage class and storage texture type
    - new built-ins: `primitive_index` and `num_workgroups`
    - support for multi-sampled depth images
  - WGSL:
    - `select()` order of true/false is swapped
  - HLSL backend is vastly improved and now usable
  - GLSL frontend is heavily reworked

## v0.5 (2021-06-18)
  - development release for wgpu-0.9
  - API:
    - barriers
    - dynamic indexing of matrices and arrays is only allowed on variables
    - validator now accepts a list of IR capabilities to allow
    - improved documentation
  - Infrastructure:
    - much richer test suite, focused around consuming or emitting WGSL
    - lazy testing on large shader corpuses
    - the binary is moved to a sub-crate "naga-cli"
  - Frontends:
    - GLSL frontend:
      - rewritten from scratch and effectively revived, no longer depends on `pomelo`
      - only supports 440/450/460 versions for now
      - has optional support for codespan messages
    - SPIRV frontend has improved CFG resolution (still with issues unresolved)
    - WGSL got better error messages, workgroup memory support
  - Backends:
    - general: better expression naming and emitting
    - new HLSL backend (in progress)
    - MSL:
      - support `ArraySize` expression
      - better texture sampling instructions
    - GLSL:
      - multisampling on GLES
    - WGSL is vastly improved and now usable

### v0.4.2 (2021-05-28)
  - SPIR-V frontend:
    - fix image stores
    - fix matrix stride check
  - SPIR-V backend:
    - fix auto-deriving the capabilities
  - GLSL backend:
    - support sample interpolation
    - write out swizzled vector accesses

### v0.4.1 (2021-05-14)
  - numerous additions and improvements to SPIR-V frontend:
    - int8, in16, int64
    - null constant initializers for structs and matrices
    - `OpArrayLength`, `OpCopyMemory`, `OpInBoundsAccessChain`, `OpLogicalXxxEqual`
    - outer product
    - fix struct size alignment
    - initialize built-ins with default values
    - fix read-only decorations on struct members
  - fix struct size alignment in WGSL
  - fix `fwidth` in WGSL
  - fix scalars arrays in GLSL backend

## v0.4 (2021-04-29)
  - development release for wgpu-0.8
  - API:
    - expressions are explicitly emitted with `Statement::Emit`
    - entry points have inputs in arguments and outputs in the result type
    - `input`/`output` storage classes are gone, but `push_constant` is added
    - `Interpolation` is moved into `Binding::Location` variant
    - real pointer semantics with required `Expression::Load`
    - `TypeInner::ValuePointer` is added
    - image query expressions are added
    - new `Statement::ImageStore`
    - all function calls are `Statement::Call`
    - `GlobalUse` is moved out into processing
    - `Header` is removed
    - entry points are an array instead of a map
    - new `Swizzle` and `Splat` expressions
    - interpolation qualifiers are extended and required
    - struct member layout is based on the byte offsets
  - Infrastructure:
    - control flow uniformity analysis
    - texture-sampler combination gathering
    - `CallGraph` processor is moved out into `glsl` backend
    - `Interface` is removed, instead the analysis produces `ModuleInfo` with all the derived info
    - validation of statement tree, expressions, and constants
    - code linting is more strict for matches
  - new GraphViz `dot` backend for pretty visualization of the IR
  - Metal support for inlined samplers
  - `convert` example is transformed into the default binary target named `naga`
  - lots of frontend and backend fixes

### v0.3.2 (2021-02-15)
  - fix logical expression types
  - fix _FragDepth_ semantics
  - spv-in:
    - derive block status of structures
  - spv-out:
    - add lots of missing math functions
    - implement discard

### v0.3.1 (2021-01-31)
  - wgsl:
    - support constant array sizes
  - spv-out:
    - fix block decorations on nested structures
    - fix fixed-size arrays
    - fix matrix decorations inside structures
    - implement read-only decorations

## v0.3 (2021-01-30)
  - development release for wgpu-0.7
  - API:
    - math functions
    - type casts
    - updated storage classes
    - updated image sub-types
    - image sampling/loading options
    - storage images
    - interpolation qualifiers
    - early and conservative depth
  - Processors:
    - name manager
    - automatic layout
    - termination analysis
    - validation of types, constants, variables, and entry points

## v0.2 (2020-08-17)
  - development release for wgpu-0.6

## v0.1 (2020-02-26)
  - initial release
