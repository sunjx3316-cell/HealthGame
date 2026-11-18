#include "LifeShape.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace {
struct Key { float year; LifeShape shape; };
const Key keys[] = {
    {0,   {{.30,.43,.80,.49},{-.15,-.04,.105,.16},{.23,-.24,.14,0}}},
    {2,   {{.43,.55,.87,.59},{-.14,-.045,.118,.18},{.21,-.19,.12,0}}},
    {6,   {{.60,.69,.93,.69},{-.115,-.055,.055,.072},{.135,-.125,.066,0}}},
    {12,  {{.84,.85,.975,.81},{-.085,-.032,.016,-.035},{.062,-.074,.027,0}}},
    {18,  {{.99,.985,1,.955},{-.025,-.012,.002,-.012},{.014,-.021,0,0}}},
    {28,  {{1,1,1,1},{.016,.006,0,.018},{0,0,0,0}}},
    {48,  {{1,1,1,1},{.013,.025,.037,.012},{-.022,.004,0,.012}}},
    {68,  {{.997,.978,1,.99},{-.019,.035,.057,-.025},{-.053,-.006,0,.066}}},
    {86,  {{.985,.947,1,.97},{-.05,.036,.062,-.066},{-.095,-.022,0,.145}}},
    {110, {{.965,.905,1,.95},{-.087,.032,.066,-.111},{-.122,-.04,0,.235}}}
};
constexpr int last=int(std::size(keys))-1;
using Member=QVector4D LifeShape::*;
// Monotone cubic Hermite interpolation. A turning point has zero tangent;
// other knots share a weighted harmonic tangent from both adjacent spans.
QVector4D tangent(int k, Member m){
    if(k==0 || k==last)return {};
    float h=keys[k].year-keys[k-1].year,j=keys[k+1].year-keys[k].year;
    auto a=(keys[k].shape.*m-keys[k-1].shape.*m)/h;
    auto b=(keys[k+1].shape.*m-keys[k].shape.*m)/j;
    QVector4D out;
    for(int c=0;c<4;++c)if(a[c]*b[c]>0)
        out[c]=3*(h+j)/((2*j+h)/a[c]+(j+2*h)/b[c]);
    return out;
}
float smoothstep(float a,float b,float x){
    float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);
}
using std::abs;using std::min;using std::max;using std::clamp;
struct vec3 { float x,y,z; };
struct vec4 { float x,y,z,w; };
#define LIFE_WARP(...) __VA_ARGS__
#include "LifeWarp.inc"
#undef LIFE_WARP
vec4 packed(QVector4D v){return {v.x(),v.y(),v.z(),v.w()};}
}
LifeShape LifeShape::at(float age){
    age=std::clamp(age,0.f,110.f);
    int k=0;while(k<last-1 && age>keys[k+1].year)++k;
    const float h=keys[k+1].year-keys[k].year,t=(age-keys[k].year)/h,s=t*t*(3-2*t);
    auto interpolate=[&](Member m){return (1-s)*(keys[k].shape.*m)+s*(keys[k+1].shape.*m)
        +h*(t*(1-t)*(1-t)*tangent(k,m)-t*t*(1-t)*tangent(k+1,m));};
    return {interpolate(&LifeShape::length),interpolate(&LifeShape::build),interpolate(&LifeShape::face)};
}
QVector3D LifeShape::map(QVector3D v)const{
    auto p=lifeWarp({v.x(),v.y(),v.z()},packed(length),packed(build),packed(face));
    return {p.x,p.y,p.z};
}
