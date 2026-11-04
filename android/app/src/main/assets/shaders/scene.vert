layout(location=0) in vec3 position;
void main(){gl_Position=p.matrix*vec4(position,1);
#ifdef VULKAN
gl_Position.z=(gl_Position.z+gl_Position.w)*.5;
#endif
}
