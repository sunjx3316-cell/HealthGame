#include "AnatomyGeometry.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <limits>
namespace {
using V=QVector3D;
V vector(const QJsonValue& value){auto a=value.toArray();return {float(a[0].toDouble()),float(a[1].toDouble()),float(a[2].toDouble())};}
struct Atlas {
    QMap<QString,AnatomyModel> models;
    QMap<QString,QMatrix4x4> placements;
    QString error;
    Atlas(){
        static_assert(sizeof(AnatomyVertex)==7*sizeof(float),"Atlas vertex format must match");
        const QString root=QCoreApplication::applicationDirPath()+"/anatomy/";
        QFile file(root+"atlas.json");
        if(!file.open(QIODevice::ReadOnly)){error="未找到解剖模型资源，请保留程序旁的 anatomy 文件夹。";return;}
        QJsonParseError parse;
        const auto doc=QJsonDocument::fromJson(file.readAll(),&parse);
        auto json=doc.object();
        if(parse.error!=QJsonParseError::NoError||json["format"]!="HealthGame-Anatomy-1"||json["stride"]!=7){
            error="解剖图谱清单格式错误。";return;
        }
        const auto ps=json["placements"].toObject();
        for(auto it=ps.begin();it!=ps.end();++it){
            auto p=it.value().toObject();QMatrix4x4 mat;mat.translate(vector(p["center"]));mat.scale(float(p["scale"].toDouble()));
            placements[it.key()]=mat;
        }
        const auto ms=json["models"].toObject();
        for(auto it=ms.begin();it!=ms.end();++it){
            auto m=it.value().toObject();const QString filename=m["file"].toString();
            if(filename.contains('/')||filename.contains('\\')||filename.contains("..")){error="无效图谱文件名。";return;}
            QFile binary(root+filename);
            if(!binary.open(QIODevice::ReadOnly)){error="解剖模型资源缺失："+filename;return;}
            QByteArray bytes=binary.readAll();
            if(filename.endsWith(".mesh"))bytes=qUncompress(bytes);
            if(bytes.isEmpty()){error="解剖模型为空或损坏："+filename;return;}
            AnatomyModel model;model.center=vector(m["center"]);model.radius=float(m["radius"].toDouble());
            for(const auto& pv:m["parts"].toArray()){
                auto p=pv.toObject();qint64 off=p["offset"].toVariant().toLongLong(),count=p["count"].toInt();
                if(count<0||count%3||off<0||off+count*qint64(sizeof(AnatomyVertex))>bytes.size()){
                    error="解剖模型顶点范围错误："+filename;return;
                }
                AnatomyPart part;part.vertices.resize(count);
                std::memcpy(part.vertices.data(),bytes.constData()+off,count*sizeof(AnatomyVertex));
                for(const auto& v:part.vertices){
                    if(!std::isfinite(v.position.lengthSquared())||!std::isfinite(v.normal.lengthSquared())||!std::isfinite(v.cavity)){
                        error="解剖模型包含非法顶点："+filename;return;
                    }
                }
                auto c=p["color"].toArray();part.color={float(c[0].toDouble()),float(c[1].toDouble()),float(c[2].toDouble()),float(c[3].toDouble())};
                part.layer=p["layer"].toInt();part.organ=p["organ"].toString();part.structure=p["name"].toString();part.roughness=float(p["roughness"].toDouble());
                if(part.structure=="Mucosa of stomach"){
                    // Refine the source surface for continuous game rugae relief.
                    auto midpoint=[](const AnatomyVertex& a,const AnatomyVertex& b){return AnatomyVertex{(a.position+b.position)*.5f,(a.normal+b.normal).normalized(),(a.cavity+b.cavity)*.5f};};
                    for(int pass=0;pass<2;++pass){
                        QVector<AnatomyVertex> finer;finer.reserve(part.vertices.size()*4);
                        for(int i=0;i<part.vertices.size();i+=3){
                            const auto a=part.vertices[i],b=part.vertices[i+1],c=part.vertices[i+2];
                            const auto ab=midpoint(a,b),bc=midpoint(b,c),ca=midpoint(c,a);
                            finer<<a<<ab<<ca<<ab<<b<<bc<<ca<<bc<<c<<ab<<bc<<ca;
                        }part.vertices=std::move(finer);
                    }
                }
                model.parts.append(std::move(part));
            }
            models[it.key()]=std::move(model);
            if(!it.key().isEmpty()){
                auto& item=models[it.key()];V low(1e5,1e5,1e5),high(-1e5,-1e5,-1e5);
                for(const auto& part:item.parts)if(part.layer==0)for(const auto& v:part.vertices)
                    for(int k=0;k<3;++k){low[k]=std::min(low[k],v.position[k]);high[k]=std::max(high[k],v.position[k]);}
                item.center=(low+high)*.5f;
                item.radius=std::max(1.f,(high-low).length()*.49f);
            }
        }
        for(const QString& key:QStringList{"","心脏","肺部","肝脏","胃部","皮肤"})
            if(models.value(key).parts.isEmpty()){error="解剖图谱不完整："+key;return;}
    }
};
Atlas& atlas(){static Atlas a;return a;}
}
QString AnatomyGeometry::error(){return atlas().error;}
AnatomyModel AnatomyGeometry::body(){return atlas().models.value("");}
AnatomyModel AnatomyGeometry::organ(const QString& name){return atlas().models.value(name);}
QMatrix4x4 AnatomyGeometry::bodyPlacement(const QString& name){return atlas().placements.value(name);}
QVector3D AnatomyGeometry::bodyAnchor(const QString& name){return bodyPlacement(name).map({0,0,0});}
bool AnatomyGeometry::affected(const QString& structure,const DiseaseVisual& visual){
    if(!visual.pattern)return false;
    for(const QString& target:visual.target.split('|',Qt::SkipEmptyParts))
        if(structure.contains(target,Qt::CaseInsensitive))return true;
    return false;
}
DiseaseVisual AnatomyGeometry::locatedVisual(const QString& name,const DiseaseVisual& visual){
    if(!visual.pattern)return visual;
    auto result=visual;const auto& parts=atlas().models[name].parts;
    V lo(1e6,1e6,1e6),hi(-1e6,-1e6,-1e6);bool found=false;
    for(const auto& p:parts)if(affected(p.structure,visual)){
        for(const auto& vertex:p.vertices){
            const V v=vertex.position;
            for(int k=0;k<3;++k){lo[k]=std::min(lo[k],v[k]);hi[k]=std::max(hi[k],v[k]);}
        }found=true;
    }
    if(!found){result.target.clear();result.amount=0;return result;}
    V desired=(lo+hi)*.5f;
    desired.setX(desired.x()+(hi.x()-lo.x())*.24f*std::clamp(visual.focus.x(),-1.f,1.f));
    desired.setY(desired.y()+(hi.y()-lo.y())*.25f*std::clamp(visual.focus.y(),-1.f,1.f));
    desired.setZ(hi.z());
    // Select an actual anterior surface point. All overlays and deformation use
    // source-space registration, including secondary organs such as the appendix.
    float best=std::numeric_limits<float>::max();
    for(const auto& p:parts)if(affected(p.structure,visual)){
        for(const auto& vertex:p.vertices){
            V d=vertex.position-desired;
            float score=d.x()*d.x()+d.y()*d.y()+.09f*d.z()*d.z();
            if(score<best){best=score;result.focus=vertex.position;}
        }
    }
    return result;
}
AnatomyModel AnatomyGeometry::lesions(const QString&,const DiseaseVisual&){
    // Disease relief is now deformed on the real anatomical surface in the
    // vertex shader; there are no floating ellipsoid "tumors".
    return {};
}
QVector3D AnatomyGeometry::ageParameters(float age){
    age=std::clamp(age,0.f,110.f);
    struct Key{float age,leg,trunk,head;};
    static const Key keys[]={{0,.25f,.38f,.64f},{5,.53f,.64f,.83f},{12,.83f,.85f,.96f},{20,1,1,1},{55,1,1,1},{80,.99f,.95f,1},{110,.98f,.90f,1}};
    for(int i=0;i<6;++i)if(age<=keys[i+1].age){
        const auto a=keys[i],b=keys[i+1];float t=(age-a.age)/(b.age-a.age);t=t*t*(3-2*t);
        return {a.leg+(b.leg-a.leg)*t,a.trunk+(b.trunk-a.trunk)*t,a.head+(b.head-a.head)*t};
    }return {1,1,1};
}
QVector3D AnatomyGeometry::agePosition(QVector3D p,float age){
    V a=ageParameters(age);
    float headWeight=std::clamp((p.y()-1.18f)/.20f,0.f,1.f);
    float width=(.30f+.70f*a.y())*(1-headWeight)+a.z()*headWeight;
    float y=-1.7f+std::min(p.y()+1.7f,1.7f)*a.x()
        +std::clamp(p.y(),0.f,1.24f)*a.y()+std::max(p.y()-1.24f,0.f)*a.z();
    float old=std::clamp((age-55)/55.f,0.f,1.f);
    float upper=std::clamp((p.y()-.25f)/1.1f,0.f,1.f);
    return {p.x()*width,y,p.z()*width+old*.20f*upper*upper};
}
