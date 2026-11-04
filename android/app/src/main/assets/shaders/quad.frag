#ifdef VULKAN
layout(location=0) in vec2 uv;
#else
in vec2 uv;
#endif
layout(location=0) out vec4 outputColor;
#ifdef VULKAN
layout(set=0,binding=1) uniform sampler2D sourceTex;
layout(set=0,binding=2) uniform sampler2D noiseTex;
vec2 imageUV(vec2 q){return vec2(q.x,1.-q.y);}
#else
uniform sampler2D sourceTex;
uniform sampler2D noiseTex;
vec2 imageUV(vec2 q){return q;}
#endif
vec4 weights(float t){return vec4(-.5*t+t*t-.5*t*t*t,1.-2.5*t*t+1.5*t*t*t,.5*t+2.*t*t-1.5*t*t*t,-.5*t*t+.5*t*t*t);}
void main(){
    if(p.view.w<1.5){vec2 q=uv;if(p.settings.y>.5)q.x+=(texture(noiseTex,imageUV(vec2(1.-q.y,p.settings.w))).r-.5)*p.settings.z;outputColor=vec4(texture(sourceTex,imageUV(q)).rgb,p.settings.x);return;}
    if(p.view.w>3.5){vec2 q=imageUV(uv);if(p.settings.y>.5)q=(clamp(floor(q*p.texSize.xy),vec2(0),p.texSize.xy-1.)+.5)*p.texSize.zw;outputColor=texture(sourceTex,q);return;}
    vec2 q=vec2(uv.x,1.-uv.y);ivec2 size=textureSize(sourceTex,0);
    if(p.view.w<2.5){outputColor=texelFetch(sourceTex,clamp(ivec2(q*vec2(size)),ivec2(0),size-1),0);return;}
    vec2 pos=q*vec2(size)-.5;ivec2 base=ivec2(floor(pos));vec4 wx=weights(fract(pos.x)),wy=weights(fract(pos.y));vec3 color=vec3(0);
    for(int y=0;y<4;y++)for(int x=0;x<4;x++)color+=texelFetch(sourceTex,clamp(base+ivec2(x-1,y-1),ivec2(0),size-1),0).rgb*wx[x]*wy[y];
    outputColor=vec4(clamp(color,0.,1.),1.);
}
