#pragma once
#include <QVector>
#include <QVector3D>
#include <QVector4D>
#include <QString>
#include <QMatrix4x4>
#include "DiseaseVisual.h"

struct AnatomyVertex { QVector3D position, normal; float cavity = 1; };
struct AnatomyPart {
    QVector<AnatomyVertex> vertices;
    QVector4D color;
    QString organ;
    QString structure;
    int layer = 0; // 0 organ exterior, 1 internal structure, 2 skin, 3 skeleton, 4 lesion
    float roughness = 0.48f;
};
struct AnatomyModel {
    QVector<AnatomyPart> parts;
    QVector3D center;
    float radius = 1;
};
class AnatomyGeometry {
public:
    static AnatomyModel body();
    static AnatomyModel organ(const QString& name);
    static AnatomyModel lesions(const QString& organ, const DiseaseVisual& visual);
    static QMatrix4x4 bodyPlacement(const QString& organ);
    static QVector3D bodyAnchor(const QString& organ);
    static DiseaseVisual locatedVisual(const QString& organ, const DiseaseVisual& visual);
    static bool affected(const QString& structure, const DiseaseVisual& visual);
    static bool prefersSection(const QString& organ,const DiseaseVisual& visual);
    static QString error();
    static QVector3D agePosition(QVector3D position, float age);
    static QVector3D ageParameters(float age);
};
