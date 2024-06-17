# Prismatica: A C++ global illumination renderer

<img align="left" style="width:180px" src="https://github.com/uvraj/Prismatica/blob/main/resources/test_gudder.png?raw=true" width="400px">

Prismatica is a rudimentary, yet fully functional renderer written in C++. To achieve photorealistic renders,
Prismatica implements a rendering technique called Monte-Carlo path tracing. 

Prismatica leverages multithreading to accelerate the path tracer. As of writing, it only supports perfectly diffuse (Lambertian) or perfectly specular materials. Moreover, the image is outputted as a portable network graphic. Using a compatible viewer to view the rendered image is advised.

## Features
Prismatica includes the following features:
- Perfectly diffuse materials
- Perfectly reflective materials
- Reflective caustics
- Cylindrical camera projection

## Implementation details

Prismatica's internal execution sequence is structured as follows:

<p align="center">
  <img src="https://raw.githubusercontent.com/uvraj/Prismatica/ffa03e5d1f4fb47f450e80449b64025984fcffa3/resources/Prismatica_Overview.svg" width = "400px"/>
</p>

To facilitate multithreading, the screen - a grid of pixels - is divided into multiple strips. The following example illustrates the allocation of strips (from here on referred to as "tiles") assuming 4 threads:

<p align="center">
  <img src="https://raw.githubusercontent.com/uvraj/Prismatica/ffa03e5d1f4fb47f450e80449b64025984fcffa3/resources/stripes.svg" width = "400px"/>
</p>