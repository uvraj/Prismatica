# Prismatica: A C++ global illumination renderer

<img align="left" style="width:180px" src="https://github.com/uvraj/Prismatica/blob/main/resources/test_gudder.png?raw=true" width="400px">

<div style="text-align: justify"> 
  Prismatica is a rudimentary, yet fully functional renderer written in C++. To achieve photorealistic renders,
  Prismatica implements a rendering technique called Monte-Carlo path tracing. Prismatica leverages multithreading to accelerate the path tracer. As of writing, it only supports perfectly diffuse (Lambertian) or perfectly specular materials. Moreover, the image is outputted as a portable network graphic. Using a compatible viewer to view the rendered image is advised.
</div>






## Features
Prismatica includes the following features:
- Perfectly diffuse materials
- Perfectly reflective materials
- Reflective caustics
- Cylindrical camera projection

## Implementation details

Prismatica's multithreaded execution sequence starts with the initialization, followed by the dispatch
of ```n``` threads responsible for rendering the image. Afterwards, the threads are joined to ensure synchronization. In order to provide visual output to the user, the resulting image is saved as a portable network graphic. Further described in subsection "Benchmark Mode", the time it took the render the image is published to an MQTT broker.

<p align="center">
  <img src="https://github.com/uvraj/Prismatica/blob/main/resources/Prismatica_Overview.svg?raw=true" width = "500px"/>
</p>

To facilitate multithreading, the screen - a grid of pixels - is divided into multiple strips. Assuming ```n``` threads results in ```n``` strips. The following example elucidates the allocation of strips (hereafter referred to as "tiles") with 4 threads:

<p align="center">
  <img src="https://github.com/uvraj/Prismatica/blob/main/resources/stripes.jpg?raw=true" width = "300px"/>
</p>

Each thread executes the ```DispatchTile()``` function, which renders a strip of the scene using ```Render()```.
For each pixel, a total of 3 GI rays are dispatched. Keen readers will have noticed the absence of reflection rays, which were not given dedicated rays.
Instead, reflection rays are implemented recursively into the ```RayTraceScene()``` function.

The ```DispatchTile()```-function is outlined below:

<p align="center">
  <img src="https://github.com/uvraj/Prismatica/blob/main/resources/DispatchTile.svg?raw=true" width = "500px"/>
</p>

Once again, keen readers will notice that the path tracer does not gather light recursively, presenting
a moot point in the implementation. Therefore, the amount of light bounces bear a hard limit of 2 presently.

To enhance visual quality and aesthetics, the linear image (cd/m^2) is tonemapped and then converted into the sRGB color space. This ensures correct colors and pleasing aesthetics.  The utilized tonemap stems from [here.](https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/)

<p style="text-align: center;">Text with basic formatting applied</p>