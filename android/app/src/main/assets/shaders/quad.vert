layout(location=0) in vec3 position;
#ifdef VULKAN
layout(location=0) out vec2 uv;
#else
out vec2 uv;
#endif
void main(){gl_Position=vec4(position.xy*p.rect.xy+p.rect.zw,0,1);uv=p.view.w>=2.?(position.xy+1.)*.5:(position.xy/max(p.crop.z,.00001)*p.rect.xy+1.)*.5-p.crop.xy;}
