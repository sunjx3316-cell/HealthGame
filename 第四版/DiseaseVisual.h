#pragma once
#include "Models.h"
#include <QVector3D>
#include <QColor>

// A semantic visual phenotype, never a diagnosis. Hidden state must not enter it.
struct DiseaseVisual {
    QString id, title, description, target;
    int catalogNumber = 0;
    int pattern = 0; // 0 normal, 1 inflammation, 2 tumor, 3 fibrosis, 4 fatty,
                     // 5 ischemia, 6 rash, 7 ulcer, 8 obstruction, 9 electrical,
                     // 10 edema, 11 jaundice, 12 infection, 13 structural.
    QColor color = QColor("#ef826c");
    QVector3D focus = {0.2f, 0.1f, 0.8f};
    float amount = 0, expansion = 0, pulse = 1;
    bool systemic = false;
};
class DiseaseVisuals {
public:
    static DiseaseVisual forDisease(const QString& id, int stage = 0, int completed = 0, int total = 0);
    static DiseaseVisual forState(const OrganState& state);
    static bool contains(const QString& id);
};
