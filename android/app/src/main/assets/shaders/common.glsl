#ifdef VULKAN
layout(set=0,binding=0,std140) uniform Parameters {
#else
layout(std140) uniform Parameters {
#endif
    mat4 matrix;
    vec4 color;
    vec4 rect;
    vec4 crop;
    vec4 settings;
    vec4 view;
    vec4 texSize;
} p;
