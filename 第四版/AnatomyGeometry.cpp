#include "AnatomyGeometry.h"
#include "LifeShape.h"
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
bool AnatomyGeometry::prefersSection(const QString& name,const DiseaseVisual& v){
    if(!v.pattern || name.isEmpty())return false;
    if(v.id=="D37")return true;
    for(const auto& part:atlas().models[name].parts)
        if(part.layer==1&&affected(part.structure,v))return true;
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
    const bool innerWall=visual.target=="Mucosa of stomach";
    if(innerWall)desired.setZ(std::min(-.04f,hi.z()));
    // Select an actual anterior surface point. All overlays and deformation use
    // source-space registration, including secondary organs such as the appendix.
    float best=std::numeric_limits<float>::max();
    for(const auto& p:parts)if(affected(p.structure,visual)){
        for(const auto& vertex:p.vertices){
            if(innerWall&&lo.z()<.0f&&vertex.position.z()>.0f)continue;
            V d=vertex.position-desired;
            float score=d.x()*d.x()+d.y()*d.y()+.09f*d.z()*d.z();
            if(score<best){best=score;result.focus=vertex.position;}
        }
    }
    return result;
}
AnatomyModel AnatomyGeometry::lesions(const QString& name,const DiseaseVisual& v){
    AnatomyModel result;
    if(!v.pattern || v.amount<=0)return result;
    // Tissue-attached nodules, vesicles, bullae, plaques and intraluminal
    // stones. Diffuse and functional diseases use their own surface/beat pass.
    struct Form { float radius,height,spread,lobes; int count; };
    static const QMap<QString,Form> shapes{
        {"D05",{.036f,.024f,.31f,.08f,9}}, {"D07",{.074f,.013f,.25f,.22f,7}},
        {"D13",{.058f,.049f,.21f,.08f,10}},{"D19",{.10f,.028f,.44f,.19f,9}},
        {"D26",{.056f,.043f,.37f,.18f,7}}, {"D33",{.069f,.041f,.65f,.16f,24}},
        {"D34",{.20f,.15f,.28f,.21f,6}},   {"D35",{.20f,.18f,.32f,.25f,7}},
        {"D37",{.052f,.052f,.12f,.18f,5}}, {"D41",{.14f,.11f,.52f,.06f,7}},
        {"D43",{.047f,.036f,.17f,.18f,6}}, {"D44",{.21f,.155f,.35f,.21f,7}},
        {"D52",{.073f,.046f,.68f,.22f,26}}
    };
    const auto spec=shapes.constFind(v.id);if(spec==shapes.cend())return result;
    const float strength=v.amount, scale=.44f+.56f*strength;
    const int count=std::max(1,int(std::ceil(spec->count*strength)));
    const auto& parts=atlas().models[name].parts;
    struct Site { const AnatomyVertex* vertex=nullptr; float distance=1e9f; QString structure; };
    QVector<QVector3D> placed;
    for(int i=0;i<count;++i){
        float angle=i*2.399963f+.7f,spread=spec->spread*scale*std::sqrt(float(i)/std::max(1,count-1));
        V goal=v.focus+V(std::cos(angle)*spread,std::sin(angle)*spread,0);
        Site best;
        for(const auto& p:parts)if(affected(p.structure,v)){
            // Triangle lists repeat source vertices. A bounded deterministic
            // sample is sufficient for satellite sites; the primary uses the registered anchor.
            int stride=std::max(1,int(p.vertices.size()/6500));
            for(int j=0;j<p.vertices.size();j+=i?stride:1){
                const auto& pt=p.vertices[j];float d=(pt.position-goal).lengthSquared();
                if(d<best.distance){best={&pt,d,p.structure};if(d<1e-12f)break;}
            }
        }
        if(!best.vertex)continue;
        const auto origin=best.vertex->position;
        float radius=spec->radius*scale*(i==0?1.f:(v.pattern==2?.39f:.82f)+.13f*std::sin(angle));
        if(std::any_of(placed.begin(),placed.end(),[&](V point){return (point-origin).length()<radius*.82f;}))continue;
        placed<<origin;
        V normal=best.vertex->normal.normalized();
        if(normal.lengthSquared()<.5f)continue;
        V axis=V::crossProduct(normal,std::abs(normal.y())<.90f?V(0,1,0):V(1,0,0)).normalized();
        V side=V::crossProduct(normal,axis);
        bool stone=v.id=="D37";
        float height=radius*spec->height/spec->radius;
        QColor tint=v.color;
        if(v.id=="D13"||v.id=="D05"||v.id=="D41")tint=QColor("#dfaba1");
        if(v.id=="D43")tint=QColor("#dfcba6");
        AnatomyPart part;part.organ=name;part.structure=best.structure;part.layer=4;
        part.roughness=stone?.76f:v.pattern==6?.26f:.48f;
        part.color={float(tint.redF()),float(tint.greenF()),float(tint.blueF()),1};
        auto surface=[&](float u,float t){
            float s=std::sin(t),c=std::cos(t);
            float bulge=1+spec->lobes*(.58f*std::cos(3*u+angle)+.42f*std::sin(5*u-t))*s*s;
            return origin+axis*(radius*bulge*s*std::cos(u))+side*(radius*.83f*bulge*s*std::sin(u))
                +normal*(height*(c*(1+.18f*(bulge-1))-(stone?1.06f:.20f)));
        };
        constexpr int rings=12,sectors=24;
        auto vertex=[&](int j,int k){
            const float u=6.2831853f*(k%sectors)/sectors,t=3.14159265f*j/rings;
            V n=j==0?normal:j==rings?-normal:V::crossProduct(
                    (surface(u,t+.02f)-surface(u,t-.02f)).normalized(),
                    (surface(u+.02f,t)-surface(u-.02f,t)).normalized()).normalized();
            return AnatomyVertex{surface(u,t),n,.92f};
        };
        QVector<AnatomyVertex> grid;grid.reserve((rings+1)*sectors);
        for(int j=0;j<=rings;++j)for(int k=0;k<sectors;++k)grid<<vertex(j,k);
        for(int j=0;j<rings;++j)for(int k=0;k<sectors;++k){
            int a=j*sectors+k,b=j*sectors+(k+1)%sectors;
            if(j!=0)part.vertices<<grid[a]<<grid[a+sectors]<<grid[b];
            if(j!=rings-1)part.vertices<<grid[b]<<grid[a+sectors]<<grid[b+sectors];
        }
        result.parts<<std::move(part);
    }
    return result;
}
QVector3D AnatomyGeometry::ageParameters(float age){
    return LifeShape::at(age).length.toVector3D();
}
QVector3D AnatomyGeometry::agePosition(QVector3D p,float age){
    return LifeShape::at(age).map(p);
}
