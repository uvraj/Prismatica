# Prismatica: A multithreaded CPU-only Path Tracer written in C++

<img align="left" style="width:210px" src="https://github.com/Alx-179/DeltaRobot/blob/master/img/icon.svg?raw=true" width="288px">

Prismatica is a rudimentary, yet fully functional renderer written in C++. To achieve photorealistic renders,
Tantalum implements a rendering technique called Monte-Carlo Path tracing. 

Prismatica leverages multithreading to accelerate the path tracer. As of writing, it only support perfectly diffuse (Lambertian) or perfectly specular materials.