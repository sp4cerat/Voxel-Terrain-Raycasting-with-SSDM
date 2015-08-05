# Voxel-Terrain-Raycasting-with-SSDM
Voxel Terrain Raycaster with Hitpoint Refinement and Screen Space Displacement Mapping

The demo is an experiment to combine terrain raycasting of perlin noise generated terrain with screen space ambient occlusions (SSAO) and screen space displacement mapping (SSDM).

The terrain is generated at the beginning of the demo as a 1024x256x1024 sized perlin noise volume data in CUDA, which is raycasted in the demo using distance functions for acceleration. To improve the quality, third-order texture filtering and hitpoint refinement using binary search are included.

After the first hit is found, the screen-space normal vector is computed for shading and for the SSDM. The SSDM is not achieved using a stack of textures as this leads to problems in case of overlapping areas for large displacements. Instead, the SSDM is achieved in a post-process using a high-resolution triangle mesh with two triangles per pixel. That is the reason why sometimes articafts near the screen-boundary can be observed.

The ones of you adept in CG may play around with the shader thats included.

(Edit) Note: As the program generates the terrain as a 256MB PBO that is copied to the final texture, at least 768MB of GPU memory are recommended.

Youtube Vid below
[![HVOX Engine](http://img.youtube.com/vi/f4bYYWnQbSU/0.jpg)](http://www.youtube.com/watch?v=f4bYYWnQbSU)
