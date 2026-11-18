#pragma once
#include <QVector3D>
#include <QVector4D>

// Artist-authored silhouettes, in atlas coordinates. The four components
// within each group are interpolated independently over the life course.
struct LifeShape {
    QVector4D length; // leg, trunk, cranium, overall girth
    QVector4D build;  // shoulder, pelvis, abdominal convexity, limb fullness
    QVector4D face;   // cheek fullness, lower face length, cranium, posture
    static LifeShape at(float years);
    QVector3D map(QVector3D point) const;
};
