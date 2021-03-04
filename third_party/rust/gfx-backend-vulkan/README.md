# gfx-backend-vulkan

[Vulkan](https://www.khronos.org/vulkan/) backend for gfx-rs.

## Normalized Coordinates

Render | Depth | Texture
-------|-------|--------
![render_coordinates](../../../info/vk_render_coordinates.png) | ![depth_coordinates](../../../info/dx_depth_coordinates.png) | ![texture_coordinates](../../../info/dx_texture_coordinates.png)

## Binding Model

Dimensions of the model:
  1. Shader stage: vs, fs, cs, others
  2. Descriptor set: 0 .. `max_bound_descriptor_sets`
  3. Binding: sparse, but expected to be somewhat tight

## Mirroring

HAL is modelled after Vulkan, so everything should be 1:1.
