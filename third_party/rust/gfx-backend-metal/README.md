# gfx-backend-metal

[Metal](https://developer.apple.com/metal/) backend for gfx-rs.

## Normalized Coordinates

Render | Depth | Texture
-------|-------|--------
![render_coordinates](../../../info/gl_render_coordinates.png) | ![depth_coordinates](../../../info/dx_depth_coordinates.png) | ![texture_coordinates](../../../info/dx_texture_coordinates.png)

## Binding Model

Dimensions of the model:
  1. Shader stage: vs, fs, cs
  2. Register: buffers, textures, samplers
  3. Binding: 0..31 buffers, 0..128 textures, 0..16 samplers

## Mirroring

TODO
