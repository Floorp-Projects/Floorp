
compute:
	xcrun -sdk macosx metal -c examples/compute/shaders.metal -o examples/compute/shaders.air
	xcrun -sdk macosx metallib examples/compute/shaders.air -o examples/compute/shaders.metallib

window:
	xcrun -sdk macosx metal -c examples/window/shaders.metal -o examples/window/shaders.air
	xcrun -sdk macosx metallib examples/window/shaders.air -o examples/window/shaders.metallib

