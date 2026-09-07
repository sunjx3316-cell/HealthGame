#pragma once
namespace AnatomyShaders {
inline const char* vertex=R"(
#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in float cavity;
uniform mat4 model,view,projection,localMatrix;
uniform mat3 normalMatrix;
uniform vec3 focus,ageParameters,detailScale;
uniform float beat,amount,age,physicalScale,expansion;
uniform int pattern;
uniform bool overview,affected,systemic,gastric;
out vec3 worldPos,worldNormal,localPos,sourcePos;
out float occlusion;
float h(vec3 p){return fract(sin(dot(p,vec3(127.1,311.7,74.7)))*43758.5453);}
float n3(vec3 p){
 vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
 return mix(mix(mix(h(i),h(i+vec3(1,0,0)),f.x),mix(h(i+vec3(0,1,0)),h(i+vec3(1,1,0)),f.x),f.y),
 mix(mix(h(i+vec3(0,0,1)),h(i+vec3(1,0,1)),f.x),mix(h(i+vec3(0,1,1)),h(i+vec3(1,1,1)),f.x),f.y),f.z);
}
vec3 ageMorph(vec3 p){
 float hw=clamp((p.y-1.18)/.20,0.,1.);
 float w=mix(.30+.70*ageParameters.y,ageParameters.z,hw);
 float y=-1.7+min(p.y+1.7,1.7)*ageParameters.x+clamp(p.y,0.,1.24)*ageParameters.y+max(p.y-1.24,0.)*ageParameters.z;
 float old=clamp((age-55.)/55.,0.,1.),upper=clamp((p.y-.25)/1.1,0.,1.);
 return vec3(p.x*w,y,p.z*w+old*.20*upper*upper);
}
float gastricFold(vec3 p){
 float phase=p.x*43.+sin(p.y*4.5)*2.8+n3(p*2.5)*7.+sin(p.y*21.)*.35;
 return .038*pow(.5+.5*sin(phase),4.)*(.45+.55*n3(p*5.));
}
void main(){
 localPos=(localMatrix*vec4(position,1.)).xyz;sourcePos=position;
 vec3 p=position,n=normal;
 if(gastric){
    float atrophy=affected&&pattern==3?1.-amount*.7:1.;
    float height=gastricFold(localPos)*atrophy;
    vec3 gradient=vec3(gastricFold(localPos+vec3(.002,0,0))-gastricFold(localPos-vec3(.002,0,0)),
        gastricFold(localPos+vec3(0,.002,0))-gastricFold(localPos-vec3(0,.002,0)),
        gastricFold(localPos+vec3(0,0,.002))-gastricFold(localPos-vec3(0,0,.002)))*atrophy/.004;
    // The source mucosal normals point into the lumen, unlike the serosa.
    p+=normal*height*physicalScale;
    n=normalize(normal-gradient+normal*dot(gradient,normal));
 }
 if(affected && pattern!=9){
   float r=length(localPos-focus),radius=.22+amount*.32;
   float region=systemic?1.:1.-smoothstep(radius*.18,radius,r);
   float grain=n3(localPos*20.),relief=0.;
   if(pattern==1)relief=.025*region;
   if(pattern==2)relief=.28*pow(region,1.25)*(.65+.45*grain);
   if(pattern==3)relief=.028*(n3(localPos*35.)-.3)*region;
   if(pattern==4)relief=.045*region;
   if(pattern==6)relief=.025*smoothstep(.60,.76,grain)*region;
   if(pattern==7)relief=-.095*exp(-r*r/0.009)+.019*exp(-pow((r-.15)*32.,2.));
   if(pattern==8)relief=.033*region;
   if(pattern==10||pattern==13)relief=expansion*region*.7;
   p+=normal*relief*amount*physicalScale;
   // Continuous normal tilt for the broad lesion, with fine tissue bump below.
   vec3 radial=localPos-focus;
   vec3 tangent=radial-normal*dot(radial,normal);
   if(pattern==2)n=normalize(normal+tangent*region*amount*2.6);
 }
 p*=beat;
 if(overview){
    // Inverse-transpose Jacobian keeps lighting correct under non-uniform growth.
    float e=.001;
    vec3 p0=ageMorph(p);
    mat3 j=mat3((ageMorph(p+vec3(e,0,0))-p0)/e,(ageMorph(p+vec3(0,e,0))-p0)/e,(ageMorph(p+vec3(0,0,e))-p0)/e);
    n=normalize(transpose(inverse(j))*n);p=p0;
 }else{p*=detailScale;n=normalize(n/detailScale);}
 worldPos=(model*vec4(p,1.)).xyz;
 worldNormal=normalize(normalMatrix*n);occlusion=cavity;
 gl_Position=projection*view*vec4(worldPos,1.);
})";
inline const char* fragment=R"(
#version 330 core
in vec3 worldPos,worldNormal,localPos,sourcePos;
in float occlusion;
out vec4 fragColor;
uniform vec4 baseColor;
uniform vec3 diseaseColor,focus,cameraPosition;
uniform float amount,roughness,age;
uniform int pattern,layer,shellPass;
uniform bool cutaway,systemic,skin,affected,overview,gastric,cuttable;
float hash(vec3 p){return fract(sin(dot(p,vec3(127.1,311.7,74.7)))*43758.5453);}
float noise(vec3 p){
 vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
 return mix(mix(mix(hash(i),hash(i+vec3(1,0,0)),f.x),mix(hash(i+vec3(0,1,0)),hash(i+vec3(1,1,0)),f.x),f.y),
 mix(mix(hash(i+vec3(0,0,1)),hash(i+vec3(1,0,1)),f.x),mix(hash(i+vec3(0,1,1)),hash(i+vec3(1,1,1)),f.x),f.y),f.z);
}
void main(){
 if(cutaway && cuttable && localPos.z>0.05)discard;
 vec3 n=normalize(worldNormal);if(!gl_FrontFacing)n=-n;
 vec3 eye=normalize(cameraPosition-worldPos);
 float grain=noise(localPos*94.),mottle=noise(localPos*21.);
 vec3 albedo=baseColor.rgb*(.94+.10*grain);
 float old=clamp((age-50.)/60.,0.,1.);
 if(skin||layer==0&&baseColor.r>.7&&baseColor.g>.47){
   float freckles=smoothstep(.69,.78,noise(localPos*58.))*old;
   albedo*=1.-freckles*.20;
 }
 if(affected){
   float r=length(localPos-focus);
   float region=systemic?.9:1.-smoothstep(.10+amount*.05,.22+amount*.34,r);
   float mask=region*amount;
   if(pattern==1)mask*=.28+.65*mottle;
   if(pattern==2)mask*=.7+.3*mottle;
   if(pattern==3)mask*=.22+.5*smoothstep(.39,.64,noise(localPos*36.));
   if(pattern==4)mask*=.45+.55*mottle;
   if(pattern==6)mask*=smoothstep(.44,.70,mottle);
   if(pattern==7)mask*=.4+.6*(1.-smoothstep(.06,.16,r));
   if(pattern==9)mask*=.10;
   if(pattern==12)mask*=smoothstep(.3,.7,mottle);
   albedo=mix(albedo,diseaseColor,clamp(mask,0.,.91));
 }
 // Subtle surface relief at the actual skin/serosa, with screen derivatives.
 vec3 q0=dFdx(worldPos),q1=dFdy(worldPos);
 vec3 r1=cross(q1,n),r2=cross(n,q0);float det=dot(q0,r1);
 float strength=skin?.0005:.00035;
 if(abs(det)>1e-10)n=normalize(n-sign(det)*strength*(dFdx(grain)*r1+dFdy(grain)*r2)/abs(det));
 vec3 l=normalize(vec3(-3.,4.,5.)-worldPos),fill=normalize(vec3(4.,1.,2.));
 vec3 halfv=normalize(l+eye);
 float ndl=max(dot(n,l),0.),wrap=clamp((dot(n,l)+.35)/1.35,0.,1.);
 float fresnel=.035+.965*pow(1.-max(dot(n,eye),0.),5.);
 float spec=pow(max(dot(n,halfv),0.),mix(115.,24.,roughness))*(.22+.18*(1.-roughness));
 float rim=pow(1.-max(dot(n,eye),0.),3.);
 float ao=mix(.42,1.,occlusion);
 vec3 color=albedo*(.22*ao+.62*ndl+.18*wrap+vec3(.72,.82,1.)*.18*max(dot(n,fill),0.))*ao
     +vec3(1.,.91,.82)*spec+vec3(.29,.47,.52)*rim*.09;
 color+=albedo*vec3(.22,.07,.035)*pow(max(dot(-n,l),0.),2.)*.2;
 float alpha=baseColor.a;
 if(skin&&overview){
   // Anatomical dissection window; keep the actual head, hands and limbs readable.
   float torso=(1.-smoothstep(.30,.41,abs(sourcePos.x)))*smoothstep(-.05,.12,sourcePos.y)*(1.-smoothstep(1.14,1.28,sourcePos.y));
   if(shellPass==0 && torso>.45)discard;
   if(shellPass==1 && torso<=.45)discard;
   alpha=shellPass==0?1.:.065;
   color=mix(color,vec3(.40,.62,.63),torso*.22);
 }
 fragColor=vec4(color,alpha);
})";
}
