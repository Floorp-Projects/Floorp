# gfx_device_dx11

[DX11](https://msdn.microsoft.com/en-us/library/windows/desktop/ff476080(v=vs.85).aspx) backend for gfx.

## Normalized Coordinates

Render | Depth | Texture
-------|-------|--------
![render_coordinates](../../../info/gl_render_coordinates.png) | ![depth_coordinates](../../../info/dx_depth_coordinates.png) | ![texture_coordinates](../../../info/dx_texture_coordinates.png)

## Binding Model

Writable storage bindings:
  1. Binding: 0 .. `D3D11_PS_CS_UAV_REGISTER_COUNT`

Other bindings:
  1. Shader stage: vs, fs, cs
  2. Register: constant buffers (CBV), shader resources (SRV), samplers
  3. Binding (tight)

## Mirroring

TODO
