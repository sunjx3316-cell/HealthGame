#include "AnatomyView.h"
#include "Database.h"
#include "AnatomyShaders.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLContext>
#include <QLinearGradient>
#include <QPainterPath>
#include <cmath>
#include <algorithm>

AnatomyView::AnatomyView(QWidget* parent):QOpenGLWidget(parent) {
    setMinimumSize(420,380);setFocusPolicy(Qt::StrongFocus);setMouseTracking(true);
    setObjectName("anatomyView");
    clock.start();
    connect(&frameTimer,&QTimer::timeout,this,[this] {
        if(autoRotate&&animate)yaw+=0.16f;
        if(isVisible())update();
    });
    frameTimer.start(33);
}
AnatomyView::~AnatomyView(){
    if(context())disconnect(context(),nullptr,this,nullptr);
    cleanup();
}
void AnatomyView::cleanup(){
    if(!context())return;
    makeCurrent();
    for(auto& mesh:meshes)destroy(mesh);
    destroy(lesionMesh);program.removeAllShaders();ready=false;doneCurrent();
}
void AnatomyView::destroy(Mesh& m){
    if(m.vbo)glDeleteBuffers(1,&m.vbo);
    if(m.vao)glDeleteVertexArrays(1,&m.vao);
    m.vbo=m.vao=0;m.ranges.clear();
}
void AnatomyView::upload(Mesh& mesh,const AnatomyModel& source){
    destroy(mesh);
    QVector<AnatomyVertex> vertices;
    for(const auto& p:source.parts){
        QVector3D low(1e5,1e5,1e5),high(-1e5,-1e5,-1e5);
        for(const auto& v:p.vertices)for(int k=0;k<3;++k){low[k]=std::min(low[k],v.position[k]);high[k]=std::max(high[k],v.position[k]);}
        mesh.ranges.append({int(vertices.size()),int(p.vertices.size()),p.layer,p.organ,p.color,p.roughness,p.structure,low,high});
        vertices+=p.vertices;
    }
    mesh.triangles=vertices.size()/3;mesh.center=source.center;mesh.radius=source.radius;
    glGenVertexArrays(1,&mesh.vao);glGenBuffers(1,&mesh.vbo);
    glBindVertexArray(mesh.vao);glBindBuffer(GL_ARRAY_BUFFER,mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(AnatomyVertex),vertices.constData(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(AnatomyVertex),reinterpret_cast<void*>(offsetof(AnatomyVertex,position)));
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(AnatomyVertex),reinterpret_cast<void*>(offsetof(AnatomyVertex,normal)));
    glEnableVertexAttribArray(2);glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(AnatomyVertex),reinterpret_cast<void*>(offsetof(AnatomyVertex,cavity)));
    glBindVertexArray(0);
}
void AnatomyView::initializeGL(){
    if(!initializeOpenGLFunctions()){glError="无法初始化 OpenGL 3.3，请更新显卡驱动。";return;}
    connect(context(),&QOpenGLContext::aboutToBeDestroyed,this,&AnatomyView::cleanup,Qt::DirectConnection);
    if(!program.addShaderFromSourceCode(QOpenGLShader::Vertex,AnatomyShaders::vertex)
        || !program.addShaderFromSourceCode(QOpenGLShader::Fragment,AnatomyShaders::fragment)||!program.link()){
        glError=program.log();return;
    }
    auto body=AnatomyGeometry::body();
    if(!AnatomyGeometry::error().isEmpty()){glError=AnatomyGeometry::error();return;}
    upload(meshes[""],body);
    for(const auto& name:{"心脏","肺部","肝脏","胃部","皮肤"})
        upload(meshes[QString::fromUtf8(name)],AnatomyGeometry::organ(QString::fromUtf8(name)));
    ready=true;lesionsDirty=true;
}
void AnatomyView::resizeGL(int,int){}
void AnatomyView::setOrgan(const QString& name){
    if(name==selected)return;
    selected=name;cutaway=false;resetCamera();lesionsDirty=true;update();
}
void AnatomyView::setStates(const QMap<QString,OrganState>& value){
    states=value;
    for(auto it=states.cbegin();it!=states.cend();++it){
        auto v=DiseaseVisuals::forState(it.value());
        if(!visuals.contains(it.key())||v.id!=visuals[it.key()].id||v.amount!=visuals[it.key()].amount)lesionsDirty=true;
        if(!visuals.contains(it.key())||v.id!=visuals[it.key()].id||v.amount!=visuals[it.key()].amount)
            visuals[it.key()]=AnatomyGeometry::locatedVisual(it.key(),v);
        if(v.pattern)treatmentSites[it.key()]=visuals[it.key()].focus;
    }
    update();
}
void AnatomyView::setAge(float years){age=std::clamp(years,0.f,110.f);update();}
void AnatomyView::setCutaway(bool e){cutaway=e;update();}
void AnatomyView::setSkinVisible(bool e){skinVisible=e;update();}
void AnatomyView::setAutoRotate(bool e){autoRotate=e;update();}
void AnatomyView::setAnimationEnabled(bool e){animate=e;update();}
void AnatomyView::resetCamera(){yaw=selected.isEmpty()?-8:-18;pitch=selected.isEmpty()?0:8;zoom=1;emit cameraChanged();update();}
int AnatomyView::triangleCount()const{return meshes.contains(selected)?meshes[selected].triangles:0;}
void AnatomyView::playTreatment(const QString& organ,const QString& id,bool success){
    effectOrgan=organ;effectMedicine=id;effectSuccess=success;treatmentClock.restart();update();
}
void AnatomyView::refreshLesions(){
    AnatomyModel combined;
    if(selected.isEmpty()){
        for(auto it=visuals.cbegin();it!=visuals.cend();++it){
            if(it.key()=="皮肤")continue;
            auto model=AnatomyGeometry::lesions(it.key(),it.value());auto mat=AnatomyGeometry::bodyPlacement(it.key());
            for(auto p:model.parts){
                for(auto& v:p.vertices){v.position=mat.map(v.position);v.normal=mat.mapVector(v.normal).normalized();}
                combined.parts.append(std::move(p));
            }
        }
    }else combined=AnatomyGeometry::lesions(selected,visuals.value(selected));
    upload(lesionMesh,combined);lesionsDirty=false;
}
void AnatomyView::renderMesh(const Mesh& mesh,bool lesionPass){
    glBindVertexArray(mesh.vao);
    const float seconds=animate?clock.elapsed()/1000.0f:0;
    for(int pass=0;pass<3;++pass){
        for(const auto& range:mesh.ranges){
            if(!visibleStructure(range))continue;
            bool shell=range.layer==2;
            if(shell!=(pass>0))continue;
            if(shell&&!skinVisible)continue;
            auto v=visuals.value(range.organ);
            bool affected=AnatomyGeometry::affected(range.structure,v);
            const auto organState=states.value(range.organ);
            if(range.structure.contains("Vermiform appendix")&&organState.removedDiseases.contains("D29"))continue;
            if(range.structure=="Gallbladder"&&organState.removedDiseases.contains("D37"))continue;
            if(selected.isEmpty()&&v.id=="D05"&&range.organ=="皮肤"){
                affected=range.structure.contains("digits",Qt::CaseInsensitive)||range.structure.contains("palm",Qt::CaseInsensitive)
                    ||range.structure.contains("sole",Qt::CaseInsensitive)||range.structure.contains("dorsum",Qt::CaseInsensitive);
                v.systemic=true;
            }
            QMatrix4x4 local;
            if(selected.isEmpty()&&!range.organ.isEmpty())local=AnatomyGeometry::bodyPlacement(range.organ).inverted();
            program.setUniformValue("localMatrix",local);
            program.setUniformValue("baseColor",range.color);
            program.setUniformValue("layer",range.layer);
            program.setUniformValue("skin",shell);
            program.setUniformValue("gastric",range.structure=="Mucosa of stomach");
            program.setUniformValue("cuttable",range.layer==0||range.structure=="Mucosa of stomach"||range.structure.contains("segment of liver",Qt::CaseInsensitive));
            program.setUniformValue("shellPass",pass==2?1:0);
            program.setUniformValue("affected",affected);
            program.setUniformValue("physicalScale",selected.isEmpty()?AnatomyGeometry::bodyPlacement(range.organ)(0,0):1.f);
            program.setUniformValue("expansion",v.expansion);
            program.setUniformValue("cutaway",cutaway&&!selected.isEmpty()&&!lesionPass);
            program.setUniformValue("systemic",v.systemic);
            program.setUniformValue("pattern",range.layer==3||lesionPass?0:v.pattern);
            program.setUniformValue("amount",v.amount);
            program.setUniformValue("roughness",range.roughness);
            program.setUniformValue("focus",v.focus);
            program.setUniformValue("diseaseColor",QVector3D(float(v.color.redF()),float(v.color.greenF()),float(v.color.blueF())));
            float beat=1;
            // In overview keep shared world coordinates stable; animate detailed anatomy.
            if(!selected.isEmpty()&&!lesionPass){
                if(selected=="心脏"){
                    float rate=v.pattern?v.pulse:1;
                    float wave=rate==0?0:std::pow(std::max(0.0f,std::sin(seconds*7.2f*rate)),8);
                    if(v.pattern==9&&rate>0)wave*=0.65f+0.35f*std::sin(seconds*13);
                    beat+=wave*0.025f;
                }else if(selected=="肺部")beat+=0.011f*std::sin(seconds*2.1f)*(v.pattern?v.pulse:1);

            }
            program.setUniformValue("beat",beat);
            glDepthMask(pass==2?GL_FALSE:GL_TRUE);
            // Two passes on translucent shell: back then front, no opaque self-occlusion.
            if(shell&&pass==2){
                glEnable(GL_CULL_FACE);glCullFace(GL_FRONT);
                glDrawArrays(GL_TRIANGLES,range.first,range.count);
                glCullFace(GL_BACK);glDrawArrays(GL_TRIANGLES,range.first,range.count);
                glDisable(GL_CULL_FACE);
            }else glDrawArrays(GL_TRIANGLES,range.first,range.count);
        }
    }
    glDepthMask(GL_TRUE);glBindVertexArray(0);
}
void AnatomyView::paintGL(){
    if(ready){
        glDisable(GL_SCISSOR_TEST);glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glClearColor(0.06f,0.12f,0.15f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRadialGradient bg(QPointF(width()*0.52,height()*0.43),width()*0.75);
    bg.setColorAt(0,QColor("#233c42"));bg.setColorAt(0.55,QColor("#172b32"));bg.setColorAt(1,QColor("#101f27"));
    painter.fillRect(rect(),bg);
    painter.setPen(QPen(QColor(104,161,159,18),1));
    for(int x=0;x<width();x+=40)painter.drawLine(x,0,x,height());
    for(int y=0;y<height();y+=40)painter.drawLine(0,y,width(),y);
    if(!ready){
        painter.setPen(QColor("#e0c1b5"));painter.drawText(rect().adjusted(30,30,-30,-30),Qt::AlignCenter|Qt::TextWordWrap,
            glError.isEmpty()?"正在构建三维解剖模型…":glError);return;
    }
    const auto& mesh=meshes[selected];
    float framing=1;
    QVector3D center=mesh.center*detailScale();
    if(selected.isEmpty()){
        auto feet=AnatomyGeometry::agePosition({0,-1.7f,0},age);
        auto head=AnatomyGeometry::agePosition({0,1.75f,0},age);
        center=(feet+head)*.5f;framing=(head.y()-feet.y())/3.45f;
    }else{
        QVector3D low(1e5,1e5,1e5),high(-1e5,-1e5,-1e5);
        for(const auto& range:mesh.ranges)if(visibleStructure(range)){
            for(int k=0;k<3;++k){low[k]=std::min(low[k],range.low[k]);high[k]=std::max(high[k],range.high[k]);}
        }
        center=(low+high)*.5f*detailScale();
        framing=std::max(.40f,(high-low).length()*.49f/mesh.radius);
    }
    float distance=mesh.radius*(selected.isEmpty()?3.4f:3.7f)*framing/zoom;
    projection.setToIdentity();projection.perspective(34,float(width())/std::max(1,height()),0.1f,60);
    view.setToIdentity();view.lookAt({0,0,distance},{0,0,0},{0,1,0});
    model.setToIdentity();model.rotate(pitch,1,0,0);model.rotate(yaw,0,1,0);model.translate(-center);
    painter.beginNativePainting();
    glViewport(0,0,int(width()*devicePixelRatioF()),int(height()*devicePixelRatioF()));
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);
    if(lesionsDirty)refreshLesions();
    program.bind();
    program.setUniformValue("overview",selected.isEmpty());
    program.setUniformValue("age",age);
    program.setUniformValue("ageParameters",AnatomyGeometry::ageParameters(age));
    program.setUniformValue("detailScale",detailScale());
    program.setUniformValue("model",model);program.setUniformValue("view",view);program.setUniformValue("projection",projection);
    program.setUniformValue("normalMatrix",model.normalMatrix());program.setUniformValue("cameraPosition",QVector3D(0,0,distance));
    renderMesh(mesh);renderMesh(lesionMesh,true);
    program.release();glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);
    painter.endNativePainting();
    painter.end();
    drawOverlay();
}
QPointF AnatomyView::project(QVector3D pos)const {
    if(selected.isEmpty())pos=AnatomyGeometry::agePosition(pos,age);
    else pos*=detailScale();
    auto p=projection*view*model*QVector4D(pos,1);
    if(p.w()<=0)return {-10000,-10000};
    return {(p.x()/p.w()+1)*0.5*width(),(1-p.y()/p.w())*0.5*height()};
}
QVector3D AnatomyView::detailScale()const{
    if(selected.isEmpty())return {1,1,1};
    float juvenile=1-AnatomyGeometry::ageParameters(age).y();
    float senescent=std::clamp((age-60)/50.f,0.f,1.f);
    float growth=1-juvenile*.45f-senescent*.035f;
    return {growth*(1+juvenile*(selected=="心脏"?.18f:.06f)),growth*(1-juvenile*(selected=="肺部"?.12f:0.f)),growth};
}
bool AnatomyView::visibleStructure(const Range& r)const{
    const auto v=visuals.value(r.organ);
    const bool affected=AnatomyGeometry::affected(r.structure,v);
    if(selected.isEmpty())return r.layer!=1;
    if(v.id=="D02")return r.structure.contains("Oropharynx")||r.structure.contains("Palatine tonsil");
    if(v.id=="D07")return r.structure=="Tongue";
    if(v.id=="D29")return r.structure.contains("colon",Qt::CaseInsensitive)||r.structure=="Vermiform appendix"||r.structure=="Jejunum";
    if(v.id=="D40")return r.structure=="Pancreas"||r.structure=="Duodenum"||r.structure=="Stomach";
    if(r.layer==5)return affected;
    if(r.layer==1)return cutaway||affected;
    return true;
}
QPointF AnatomyView::organScreenPosition(const QString& organ)const{
    const auto visual=visuals.value(organ);
    return project(visual.pattern?AnatomyGeometry::bodyPlacement(organ).map(visual.focus):AnatomyGeometry::bodyAnchor(organ));
}
void AnatomyView::drawOverlay(){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    p.setFont(QFont("Microsoft YaHei UI",9));
    p.setPen(QColor("#7eacae"));
    p.drawText(QRect(22,17,width()-44,22),Qt::AlignLeft,selected.isEmpty()?"ANATOMY / 全身解剖":"ANATOMY / "+selected+" · 三维详情");
    p.setPen(QColor("#bad1cf"));
    p.drawText(QRect(22,height()-35,width()-44,22),Qt::AlignLeft,"拖动旋转  ·  滚轮缩放  ·  双击复位");
    p.setPen(QColor("#5b8c92"));p.drawText(QRect(22,height()-35,width()-44,22),Qt::AlignRight,
        cutaway?"前侧剖视":skinVisible&&selected.isEmpty()?"解剖窗口":"表面结构");
    p.setPen(QColor("#688b8e"));p.setFont(QFont("Microsoft YaHei UI",8));
    p.drawText(QRect(22,39,width()-44,20),Qt::AlignLeft,QString("Z-Anatomy / BodyParts3D   ·   %1 万三角面   ·   %2 岁").arg(triangleCount()/10000.,0,'f',1).arg(age,0,'f',0));
    p.drawText(QRect(22,height()-57,width()-44,18),Qt::AlignRight,"解剖图谱来源 · 年龄形态与病灶为游戏示意");
    p.setFont(QFont("Microsoft YaHei UI",9));
    if(selected.isEmpty()){
        const QStringList names{"肺部","心脏","肝脏","胃部","皮肤"};
        const QList<float> yy{0.24f,0.38f,0.50f,0.62f,0.75f};
        for(int i=0;i<names.size();++i){
            bool left=i%2==0;QPointF anchor=organScreenPosition(names[i]);
            float y=height()*yy[i],x=left?22:width()-118;
            QColor c=visuals.value(names[i]).pattern?QColor("#f29d90"):QColor("#91c9c5");
            p.setPen(QPen(c,1));p.drawLine(anchor,QPointF(left?x+90:x,y+9));
            p.setBrush(c);p.drawEllipse(anchor,3,3);
            p.setPen(QColor("#dcece8"));p.drawText(QRectF(x,y-3,100,23),Qt::AlignLeft,names[i]);
            p.setPen(c);p.setFont(QFont("Microsoft YaHei UI",8));
            p.drawText(QRectF(x,y+18,100,20),Qt::AlignLeft,visuals.value(names[i]).pattern?"已确诊病变":"未见明显异常");
            p.setFont(QFont("Microsoft YaHei UI",9));
        }
    }else{
        const auto v=visuals.value(selected);
        if(v.pattern){
            auto anchor=project(v.focus);
            p.setPen(QPen(QColor("#efb693"),1));
            p.setBrush(Qt::NoBrush);p.drawEllipse(anchor,14+v.amount*8,14+v.amount*8);
            p.drawLine(anchor+QPointF(22,0),QPointF(width()-170,86));
            p.setPen(QColor("#efc6b3"));p.drawText(QRect(width()-190,54,170,28),Qt::AlignRight,v.title);
        }
        // Functional conditions get a rhythm trace, instead of a fictitious visible tumor.
        if(selected=="心脏"){
            QRectF box(22,height()-111,145,52);p.fillRect(box,QColor(10,27,32,120));
            QPainterPath trace;
            float seconds=animate?clock.elapsed()/1000.f:0;
            float rate=v.pattern?v.pulse:1;
            for(int i=0;i<140;++i){
                float phase=std::fmod(i/30.0f+seconds*rate,1.0f);
                float value=rate==0?0:std::exp(-std::pow((phase-0.42f)*35,2))*22
                    -std::exp(-std::pow((phase-0.49f)*40,2))*11+std::sin(phase*6.28f)*2;
                if(v.pattern==9&&rate>0)value+=std::sin(i*0.6f+seconds*8)*3;
                QPointF q(box.left()+i+3,box.center().y()-value);
                if(i==0)trace.moveTo(q);else trace.lineTo(q);
            }
            p.setPen(QPen(QColor("#82d8bc"),1.6));p.drawPath(trace);
        }
    }
    if(treatmentClock.isValid()&&treatmentClock.elapsed()<2300){
        float t=treatmentClock.elapsed()/2300.0f,fade=std::sin(t*3.14159f);
        if(!selected.isEmpty()&&selected!=effectOrgan)return;
        QVector3D target=treatmentSites.value(effectOrgan,meshes[effectOrgan].center);
        if(selected.isEmpty())target=AnatomyGeometry::bodyPlacement(effectOrgan).map(target);
        QPointF center=project(target);
        const bool surgical=Database::getInstance().getMedicineById(effectMedicine).type=="手术";
        QColor c=!effectSuccess?QColor("#f08189"):surgical?QColor("#a5e7ee"):effectMedicine.contains("CANCER")?QColor("#d4aff2"):QColor("#8fe1b4");
        c.setAlphaF(std::clamp(fade,0.f,1.f));
        p.setPen(QPen(c,2));p.setBrush(Qt::NoBrush);
        if(surgical){
            for(int ring=0;ring<3;++ring)p.drawEllipse(center,20+t*75+ring*13,8+t*25+ring*5);
            p.drawLine(center+QPointF(-45+90*t,-28),center+QPointF(-45+90*t,28));
        }else{
            for(int i=0;i<36;++i){
                float q=std::fmod(t*1.8f+i/36.f,1.f),a=i*2.39996f+t*3;
                float rad=(1-q)*145;
                // Project 3D spiral paths so the effect rotates with the anatomy.
                auto point=project(target+QVector3D(std::cos(a)*rad/160,std::sin(a)*rad/180,(1-q)*1.5f));
                p.setPen(Qt::NoPen);p.setBrush(c);p.drawEllipse(point,2.2+q*1.7,2.2+q*1.7);
            }
        }
        p.setPen(c);p.setFont(QFont("Microsoft YaHei UI",11,QFont::DemiBold));
        QString text=effectSuccess?(surgical?"手术完成 · 组织状态更新":"治疗生效 · 病变状态更新"):"治疗未成功 · 请检查当前方案";
        p.drawText(QRect(20,44,width()-40,35),Qt::AlignCenter,text);
    }
}
void AnatomyView::mousePressEvent(QMouseEvent* e){pressPoint=lastPoint=e->position();dragged=false;}
void AnatomyView::mouseMoveEvent(QMouseEvent* e){
    if(e->buttons()&Qt::LeftButton){
        QPointF d=e->position()-lastPoint;
        if((e->position()-pressPoint).manhattanLength()>5)dragged=true;
        if(dragged){yaw+=d.x()*0.48;pitch=std::clamp(pitch+float(d.y()*0.4),-75.f,75.f);emit cameraChanged();update();}
        lastPoint=e->position();
    }
}
void AnatomyView::mouseReleaseEvent(QMouseEvent* e){
    if(dragged||e->button()!=Qt::LeftButton||!selected.isEmpty())return;
    // First pick explicit callouts; then the nearest projected organ center.
    const QStringList names{"肺部","心脏","肝脏","胃部","皮肤"};
    const QList<float> yy{0.24f,0.38f,0.50f,0.62f,0.75f};
    for(int i=0;i<names.size();++i){
        QRectF label(i%2==0?18:width()-124,height()*yy[i]-5,112,46);
        if(label.contains(e->position())){emit organSelected(names[i]);return;}
    }
    for(const auto& name:names)if(QLineF(e->position(),organScreenPosition(name)).length()<9){emit organSelected(name);return;}
    // Pick actual deformed triangles, so an organ's edge is clickable as well
    // as its callout. Skin/bones do not steal clicks from the dissection window.
    auto inverse=(projection*view*model).inverted();
    const float nx=2*e->position().x()/width()-1,ny=1-2*e->position().y()/height();
    auto near4=inverse*QVector4D(nx,ny,-1,1),far4=inverse*QVector4D(nx,ny,1,1);
    const QVector3D origin=near4.toVector3DAffine(),direction=(far4.toVector3DAffine()-origin).normalized();
    float nearest=1e6f;QString hit;
    for(const auto& part:AnatomyGeometry::body().parts){
        if(part.organ.isEmpty()||part.layer!=0)continue;
        for(int i=0;i+2<part.vertices.size();i+=3){
            const auto a=AnatomyGeometry::agePosition(part.vertices[i].position,age);
            const auto b=AnatomyGeometry::agePosition(part.vertices[i+1].position,age);
            const auto c=AnatomyGeometry::agePosition(part.vertices[i+2].position,age);
            const auto edge1=b-a,edge2=c-a,p=QVector3D::crossProduct(direction,edge2);
            const float determinant=QVector3D::dotProduct(edge1,p);if(std::abs(determinant)<1e-8f)continue;
            const float inv=1/determinant;const auto delta=origin-a;
            const float u=QVector3D::dotProduct(delta,p)*inv;if(u<0||u>1)continue;
            const auto q=QVector3D::crossProduct(delta,edge1);
            const float v=QVector3D::dotProduct(direction,q)*inv;if(v<0||u+v>1)continue;
            const float distance=QVector3D::dotProduct(edge2,q)*inv;
            if(distance>0&&distance<nearest){nearest=distance;hit=part.organ;}
        }
    }
    if(!hit.isEmpty()){emit organSelected(hit);return;}
    QString closest;double best=32;
    for(const auto& name:names){
        double dist=QLineF(e->position(),organScreenPosition(name)).length();
        if(dist<best){best=dist;closest=name;}
    }
    if(!closest.isEmpty())emit organSelected(closest);
}
void AnatomyView::wheelEvent(QWheelEvent* e){zoom=std::clamp(zoom*float(std::pow(1.0015,e->angleDelta().y())),0.55f,2.8f);update();e->accept();}
void AnatomyView::mouseDoubleClickEvent(QMouseEvent*){resetCamera();}
