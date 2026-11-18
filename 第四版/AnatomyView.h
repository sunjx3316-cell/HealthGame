#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QElapsedTimer>
#include <QTimer>
#include <QMap>
#include "AnatomyGeometry.h"

class AnatomyView : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit AnatomyView(QWidget* parent=nullptr);
    ~AnatomyView() override;
    void setOrgan(const QString& name);
    QString selectedOrgan() const { return selected; }
    void setStates(const QMap<QString,OrganState>& states);
    void setCutaway(bool enabled);
    void setDissectionWindow(bool enabled);
    void setAutoRotate(bool enabled);
    void resetCamera();
    void playTreatment(const QString& organ,const QString& medicineId,bool success);
    bool rendererReady() const { return ready; }
    QString rendererError() const { return glError; }
    int triangleCount() const;
    QPointF organScreenPosition(const QString& organ) const;
    void setAnimationEnabled(bool enabled);
    void setAge(float years);
    float patientAge() const { return age; }
signals:
    void organSelected(const QString&);
    void cameraChanged();
protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int,int) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
private:
    struct Range { int first,count,layer; QString organ; QVector4D color; float roughness; QString structure; QVector3D low,high; };
    struct Mesh { GLuint vao=0,vbo=0; QVector<Range> ranges; QVector3D center; float radius=1; int triangles=0; };
    void upload(Mesh&,const AnatomyModel&);
    bool loadPortrait();
    void destroy(Mesh&);
    void cleanup();
    enum class RenderPass { Surface, Aperture, Lesion };
    void renderMesh(const Mesh&,RenderPass pass=RenderPass::Surface);
    void drawOverlay();
    QPointF project(QVector3D) const;
    void refreshLesions();
    QVector3D detailScale() const;
    bool visibleStructure(const Range&) const;
    QMap<QString,Mesh> meshes;
    Mesh lesionMesh;
    Mesh portrait;
    QMap<QString,OrganState> states;
    QMap<QString,DiseaseVisual> visuals;
    QMap<QString,QVector3D> treatmentSites;
    QOpenGLShaderProgram program;
    QString selected,glError;
    float yaw=-12,pitch=0,zoom=1;
    float age=20;
    bool ready=false,cutaway=false,dissection=false,autoRotate=false,lesionsDirty=true,animate=true;
    QPointF pressPoint,lastPoint;
    bool dragged=false;
    QMatrix4x4 model,view,projection;
    QTimer frameTimer;
    QElapsedTimer clock,treatmentClock;
    QString effectOrgan,effectMedicine;
    bool effectSuccess=true;
};
