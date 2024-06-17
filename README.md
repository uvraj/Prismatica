# Prismatica: A C++ global illumination renderer

<img align="left" style="width:210px" src="https://github.com/uvraj/Prismatica/blob/main/resources/test.png?raw=true" width="400px">

Prismatica is a rudimentary, yet fully functional renderer written in C++. To achieve photorealistic renders,
Prismatica implements a rendering technique called Monte-Carlo path tracing. 

Prismatica leverages multithreading to accelerate the path tracer. As of writing, it only supports perfectly diffuse (Lambertian) or perfectly specular materials. Moreover, the image is outputted as a portable network graphic. Using a compatible viewer to view the rendered image is advised.

## Features
Prismatica includes the following features:
- Perfectly diffuse materials
- Perfectly reflective materials
- Reflective caustics should be supported

## Implementation details

