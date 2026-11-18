#pragma once
namespace AnatomyShaders {
inline const char* vertex=R"(
#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in float cavity;
layout(location=3) in vec3 babyPos;
layout(location=4) in vec3 babyNormal;
layout(location=5) in vec3 childPos;
layout(location=6) in vec3 childNormal;
layout(location=7) in vec3 elderPos;
layout(location=8) in vec3 elderNormal;
uniform bool portraitMesh;
uniform bool lowerArm;
uniform vec4 portraitWeights;
uniform mat4 model,view,projection,localMatrix;
uniform mat3 normalMatrix;
uniform vec3 focus,detailScale;
uniform vec4 lifeLengths,lifeBuild,lifeFace;
uniform float beat,amount,age,physicalScale,expansion;
uniform int pattern,phenotype;
uniform bool overview,affected,systemic,gastric;
out vec3 worldPos,worldNormal,localPos,sourcePos;
out float occlusion;
float h(vec3 p){return fract(sin(dot(p,vec3(127.1,311.7,74.7)))*43758.5453);}
float n3(vec3 p){
 vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
 return mix(mix(mix(h(i),h(i+vec3(1,0,0)),f.x),mix(h(i+vec3(0,1,0)),h(i+vec3(1,1,0)),f.x),f.y),
 mix(mix(h(i+vec3(0,0,1)),h(i+vec3(1,0,1)),f.x),mix(h(i+vec3(0,1,1)),h(i+vec3(1,1,1)),f.x),f.y),f.z);
}
)"
#define LIFE_WARP(...) #__VA_ARGS__
#include "LifeWarp.inc"
#undef LIFE_WARP
R"(
vec3 ageMorph(vec3 p){return lifeWarp(p,lifeLengths,lifeBuild,lifeFace);}
float gastricFold(vec3 p){
 float phase=p.x*43.+sin(p.y*4.5)*2.8+n3(p*2.5)*7.+sin(p.y*21.)*.35;
 return .038*pow(.5+.5*sin(phase),4.)*(.45+.55*n3(p*5.));
}
void main(){
 localPos=(localMatrix*vec4(position,1.)).xyz;sourcePos=position;
 vec3 p=position,n=normal;
 if(portraitMesh){
    p=position*portraitWeights.x+babyPos*portraitWeights.y+childPos*portraitWeights.z+elderPos*portraitWeights.w;
    n=normalize(normal*portraitWeights.x+babyNormal*portraitWeights.y+childNormal*portraitWeights.z+elderNormal*portraitWeights.w);
    localPos=(localMatrix*vec4(p,1.)).xyz;
    float neck=length(vec2(sourcePos.x/.161,(sourcePos.z-.014)/.157));
    float collar=1.105+.085*smoothstep(.52,1.14,neck)-.035*smoothstep(.07,.20,sourcePos.z);
    float sleeve=lowerArm?smoothstep(.764,.782,sourcePos.y):1.;
    float cloth=smoothstep(-.421,-.404,sourcePos.y)*(1.-smoothstep(collar-.007,collar,sourcePos.y))*sleeve;
    p+=n*(.010+.002*sin(sourcePos.y*37.+sourcePos.x*17.))*cloth;
 }
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
   if(pattern==2)relief=(phenotype==45?.014:.032)*region*(.65+.45*grain);
   if(pattern==3)relief=.048*(n3(localPos*14.)-.34)*region;
   if(pattern==4)relief=.045*region;
   if(pattern==6)relief=.025*smoothstep(.60,.76,grain)*region;
   if(pattern==7)relief=-.095*exp(-r*r/0.009)+.029*exp(-pow((r-.15)*32.,2.));
   if(pattern==8)relief=.033*region;
   if(pattern==10||pattern==13)relief=expansion*region*.7;
   p+=normal*relief*amount*physicalScale;
   // Continuous normal tilt for the broad lesion, with fine tissue bump below.
   vec3 radial=localPos-focus;
   vec3 tangent=radial-normal*dot(radial,normal);
   if(pattern==7)n=normalize(normal-tangent*exp(-r*r/.021)*amount*4.);
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
 worldNormal=normalize(normalMatrix*n);occlusion=portraitMesh?1.:cavity;
 gl_Position=projection*view*vec4(worldPos,1.);
})";
inline const char* fragment=R"(
#version 330 core
in vec3 worldPos,worldNormal,localPos,sourcePos;
in float occlusion;
out vec4 fragColor;
uniform vec4 baseColor;
uniform mat4 model;
uniform vec3 diseaseColor,focus,cameraPosition;
uniform float amount,roughness,age,sectionDepth;
uniform int pattern,layer,phenotype;
uniform bool cutaway,systemic,skin,affected,gastric,cuttable,aperturePass,hair;
uniform bool windowSurface;
uniform bool portraitMesh;
uniform bool lowerArm;
uniform vec3 eyeCenter,eyeRadius;
float hash(vec3 p){return fract(sin(dot(p,vec3(127.1,311.7,74.7)))*43758.5453);}
float noise(vec3 p){
 vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
 return mix(mix(mix(hash(i),hash(i+vec3(1,0,0)),f.x),mix(hash(i+vec3(0,1,0)),hash(i+vec3(1,1,0)),f.x),f.y),
 mix(mix(hash(i+vec3(0,0,1)),hash(i+vec3(1,0,1)),f.x),mix(hash(i+vec3(0,1,1)),hash(i+vec3(1,1,1)),f.x),f.y),f.z);
}
void main(){
 if(cutaway && cuttable && localPos.z>sectionDepth)discard;
 // Rest-pose coordinates keep the opening and its stitched edge on the shirt during growth.
 float opening=0.;
 if(windowSurface && dot(mat3(model)*vec3(0.,0.,1.),cameraPosition-worldPos)>0.){
   vec2 oval=vec2(sourcePos.x/.298,(sourcePos.y-.693)/.465);
   opening=(1.-smoothstep(.82,1.06,dot(oval,oval)))*smoothstep(-.015,.10,sourcePos.z);
 }
 if(aperturePass){
   if(opening<=.45)discard;
   fragColor=vec4(mix(vec3(.09,.15,.16),vec3(.22,.29,.28),1.-smoothstep(.45,.72,opening)),1.);
   return;
 }
 vec3 n=normalize(worldNormal);if(!gl_FrontFacing)n=-n;
 vec3 eye=normalize(cameraPosition-worldPos);
 vec3 tissue=skin||hair?sourcePos:localPos;
 float footprint=max(length(dFdx(tissue)),length(dFdy(tissue)));
 float grain=mix(noise(tissue*94.),.5,smoothstep(.004,.016,footprint)),mottle=noise(tissue*21.);
 vec3 albedo=baseColor.rgb*(.94+.10*grain);
 float old=clamp((age-50.)/60.,0.,1.);
 float fabric=0.;
 if(portraitMesh&&skin){
    float y=sourcePos.y,x=abs(sourcePos.x);
    float neck=length(vec2(sourcePos.x/.161,(sourcePos.z-.014)/.157));
    float neckline=1.105+.085*smoothstep(.52,1.14,neck)-.035*smoothstep(.07,.20,sourcePos.z);
    fabric=smoothstep(-.416,-.407,y)*(1.-smoothstep(neckline-.006,neckline,y))*(lowerArm?smoothstep(.768,.777,y):1.);
    float shoulder=smoothstep(.8,1.03,y)*smoothstep(.12,.28,x);
    vec3 jersey=mix(vec3(.34,.64,.62),vec3(.23,.48,.48),shoulder);
    if(y<.14)jersey=vec3(.17,.31,.35);
    if(abs(y-.14)<.017||abs(y+.394)<.010||abs(y-neckline)<.014||(lowerArm&&abs(y-.79)<.01))jersey*=.78;
    // Two narrow sleeve bands and the chest pocket follow the fabric during growth.
    if(lowerArm&&y>.80&&y<.823)jersey=mix(jersey,vec3(.75,.83,.71),.65);
    float pocket=length(vec2((sourcePos.x-.119)/.061,(y-.883)/.065));
    if(pocket<1.&&sourcePos.z>.09&&y<.906)jersey=mix(jersey,vec3(.68,.78,.65),.42);
    float rib=.5+.5*sin(sourcePos.x*600.);
    albedo=mix(albedo,jersey*(.985+.02*rib*(1.-smoothstep(.001,.005,footprint))),fabric);
 }
 if(portraitMesh&&layer==8){
    vec3 ball=(sourcePos-eyeCenter)/max(eyeRadius,vec3(.0001));
    float iris=length(ball.xy-vec2(0,.04));
    float front=smoothstep(.1,.6,ball.z);
    vec3 pigment=mix(vec3(.15,.095,.065),vec3(.41,.27,.14),smoothstep(.17,.45,iris));
    pigment=mix(vec3(.046,.037,.035),pigment,smoothstep(.23,.28,iris));
    pigment=mix(pigment,vec3(.12,.085,.064),smoothstep(.42,.53,iris));
    albedo=mix(albedo,pigment,front*(1.-smoothstep(.51,.57,iris)));
    albedo=mix(albedo,vec3(.99,.97,.91),front*(1.-smoothstep(.065,.125,length(ball.xy-vec2(-.14,.21)))));
 }
 if(skin&&fabric<.01){
   vec2 cheek=(vec2(abs(sourcePos.x),sourcePos.y)-vec2(.096,1.425))/vec2(.047,.047);
   float blush=exp(-dot(cheek,cheek))*smoothstep(.02,.12,sourcePos.z);
   albedo=mix(baseColor.rgb,vec3(.91,.49,.45),.40*blush);
   if(portraitMesh&&sourcePos.z>.097){
     float ax=abs(sourcePos.x);
     float lips=length(vec2(ax/.047,(sourcePos.y-1.338-.004*smoothstep(.014,.046,ax))/.012));
     albedo=mix(albedo,vec3(.77,.47,.43),.58*(1.-smoothstep(.65,1.16,lips)));
     float browY=1.516+.009*(1.-pow(clamp((ax-.071)/.051,-1.,1.),2.))-.006*smoothstep(.07,.12,ax);
     float brow=exp(-pow((sourcePos.y-browY)/.007,2.))*smoothstep(.024,.043,ax)*(1.-smoothstep(.090,.124,ax));
     albedo=mix(albedo,vec3(.29,.21,.17),brow*mix(.42,.75,smoothstep(1.,16.,age)));
   }
   albedo*=.988+.025*grain-.08*old*smoothstep(.60,.78,mottle);
 }
 if(hair){
   float front=smoothstep(-.07,.16,sourcePos.z);
   float line=1.435+.151*front+.019*sin(sourcePos.x*19.+.65)*front
       -.041*smoothstep(.105,.16,abs(sourcePos.x))+.024*(1.-smoothstep(0.,5.,age));
   float earArc=sqrt(max(0.,1.-pow((sourcePos.z+.005)/.063,2.)));
   line=max(line,1.453+.08*earArc*smoothstep(.127,.158,abs(sourcePos.x)));
   line+=.0035*sin(sourcePos.x*295.+sourcePos.z*100.);
   if(layer==9 && sourcePos.y<line)discard;
   float strand=.5+.5*sin(sourcePos.y*680.+sourcePos.x*180.+mottle*4.);
   albedo=mix(baseColor.rgb,vec3(.66,.66,.62),smoothstep(43.,98.,age)*(.74+.26*mottle));
   albedo=mix(albedo,vec3(.40,.29,.21),.38*(1.-smoothstep(0.,5.,age)));
   albedo*=.92+.12*mottle+.09*strand*(1.-smoothstep(.001,.007,footprint));
 }
 if(affected){
   float r=length(localPos-focus);
   float region=systemic?.9:1.-smoothstep(.10+amount*.05,.22+amount*.34,r);
   float mask=region*amount;
   vec3 warped=localPos+vec3(mottle*.022);
   float lobule=noise(warped*12.);
   if(pattern==1){
      float vessel=1.-smoothstep(.012,.065,abs(noise(warped*7.)-.49));
      mask*=.26+.48*mottle+.38*vessel;
   }
   if(pattern==2)mask*=phenotype==45?smoothstep(.18,.64,lobule+.38*region):.55+.4*mottle;
   if(pattern==3){
      float septa=1.-smoothstep(.018,.093,abs(lobule-.49));
      mask*=.22+.65*septa;
   }
   if(pattern==4)mask*=.30+.66*smoothstep(.42,.68,lobule);
   if(pattern==5)mask*=.42+.58*smoothstep(.15,.8,lobule+.17*region);
   if(pattern==6)mask*=smoothstep(.36,.71,mottle+.14*lobule);
   if(pattern==7){
      float rr=r+.028*(mottle-.5);
      albedo=mix(albedo,vec3(.62,.21,.21),amount*exp(-pow((rr-.14)*21.,2.)));
      vec3 bed=phenotype==38?vec3(.26,.065,.082):vec3(.82,.66,.49);
      albedo=mix(albedo,bed,amount*(1.-smoothstep(.052,.13,rr)));
      mask*=.18;
   }
   if(pattern==9)mask*=.10;
   if(pattern==10)mask*=.40+.55*lobule;
   if(pattern==11)mask*=.65+.3*lobule;
   if(pattern==12)mask*=smoothstep(.28,.71,.70*lobule+.30*mottle);
   albedo=mix(albedo,diseaseColor,clamp(mask,0.,.91));
 }
 // Subtle surface relief at the actual skin/serosa, with screen derivatives.
 vec3 q0=dFdx(worldPos),q1=dFdy(worldPos);
 vec3 r1=cross(q1,n),r2=cross(n,q0);float det=dot(q0,r1);
 float strength=skin?.00010:.00030;
 if(abs(det)>1e-10)n=normalize(n-sign(det)*strength*(dFdx(grain)*r1+dFdy(grain)*r2)/abs(det));
 vec3 l=normalize(vec3(-3.,4.,5.)-worldPos),fill=normalize(vec3(4.,1.,2.));
 vec3 halfv=normalize(l+eye);
 float ndl=max(dot(n,l),0.),wrap=clamp((dot(n,l)+.35)/1.35,0.,1.);
 float fresnel=.035+.965*pow(1.-max(dot(n,eye),0.),5.);
 float spec=pow(max(dot(n,halfv),0.),mix(115.,24.,roughness))*(skin?.048:.16+.15*(1.-roughness));
 float rim=pow(1.-max(dot(n,eye),0.),3.);
 float ao=mix(skin?.78:.53,1.,occlusion);
 vec3 sky=mix(vec3(.78,.72,.69),vec3(.88,.96,1.),.5+.5*n.y);
 vec3 color=albedo*(.49*sky+.49*ndl+.16*wrap+vec3(.74,.83,1.)*.16*max(dot(n,fill),0.))*ao
     +vec3(1.,.93,.85)*spec+vec3(.40,.58,.62)*rim*.105;
 color+=albedo*vec3(.22,.07,.035)*pow(max(dot(-n,l),0.),2.)*.2;
 float binding=smoothstep(.10,.23,opening)*(1.-smoothstep(.38,.45,opening));
 color=mix(color,vec3(.47,.69,.66)*(.72+.28*ndl),binding*.8);
 fragColor=vec4(color,baseColor.a);
})";
}
