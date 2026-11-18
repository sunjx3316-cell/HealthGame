// Shared C++ and GLSL. Organ callouts, the ray picker and the GPU must use
// exactly the same warp, including the forward lean in a side view.
LIFE_WARP(
vec3 lifeWarp(vec3 p,vec4 sizes,vec4 build,vec4 face){
    float x=abs(p.x), y=p.y;
    float head=smoothstep(1.07f,1.36f,y);
    float width=sizes.w+(sizes.z-sizes.w)*head;
    float shoulder=smoothstep(.52f,.98f,y)*(1.f-smoothstep(1.15f,1.34f,y));
    float hip=smoothstep(-.45f,-.03f,y)*(1.f-smoothstep(.18f,.52f,y));
    float arm=smoothstep(.32f,.47f,x)*smoothstep(-.30f,-.05f,y);
    float leg=1.f-smoothstep(-.4f,.03f,y);
    float belly=smoothstep(-.02f,.34f,y)*(1.f-smoothstep(.40f,.82f,y))*(1.f-arm);
    float anterior=smoothstep(-.08f,.18f,p.z);
    float cheek=smoothstep(1.30f,1.39f,y)*(1.f-smoothstep(1.43f,1.52f,y))*anterior;
    float skull=smoothstep(1.48f,1.69f,y);
    float upper=smoothstep(.12f,1.60f,y);
    vec3 q=p;
    float side=p.x<0.f?-1.f:1.f;
    float limbCenter=arm*.45f+leg*.17f;
    q.x=width*(p.x*(1.f+build.x*shoulder+build.y*hip+.12f*face.x*cheek+.16f*face.z*skull)
         +build.w*(arm+leg)*(p.x-side*limbCenter));
    q.y=-1.7f+min(y+1.7f,1.7f)*sizes.x+clamp(y,0.f,1.24f)*sizes.y+max(y-1.24f,0.f)*sizes.z;
    q.y-=.052f*(1.f-sizes.y)*smoothstep(1.06f,1.34f,y);
    q.y-=face.w*(.13f*shoulder+.19f*head);
    q.z=width*(p.z*(1.f+build.w*(arm+leg)+face.z*skull)+belly*build.z*anterior
         +face.x*.012f*cheek)+face.w*upper*upper;
    return q;
}
)
