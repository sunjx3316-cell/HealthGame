#include "DiseaseRules.h"
#include <QStringList>

namespace DiseaseRules {
bool latentProgressive(const Disease& disease) {
    // Treatment stages do not imply an asymptomatic incubation period:
    // infarction and decompensated heart disease must be visible immediately.
    return QStringList{"D34", "D35", "D44", "D45"}.contains(disease.id);
}

bool congenital(const Disease& disease) {
    return disease.id == "D01" || disease.id == "D08";
}

bool requiresProcedure(const Disease& disease) {
    // A generic drug may not remove an appendix/gallbladder, rebuild a
    // congenital structure, drain a pneumothorax or replace a valve.
    return QStringList{"D01", "D08", "D29", "D37", "D39", "D43"}.contains(disease.id);
}

bool major(const Disease& disease) {
    return disease.severity >= 4;
}

QString requiredStructure(const Disease& disease) {
    if (disease.id == "D29") return "appendix";
    if (disease.id == "D18" || disease.id == "D37") return "gallbladder";
    return {};
}

QString removedStructure(const QString& medicineId) {
    // These are complete named-structure removals. A local tumour excision
    // deliberately does not make its parent organ permanently cancer-proof.
    if (medicineId == "M29") return "appendix";
    if (medicineId == "M37") return "gallbladder";
    return {};
}

int recoveryTicks(const Disease& disease) {
    if (disease.id == "D43") return 100; // Ten game years after valve treatment.
    switch (disease.severity) {
    case 1: return 3;
    case 2: return 6;
    case 3: return 15;
    case 4: return 40;
    case 5: return 50;
    default: return 0;
    }
}

bool isChronicLung(const QString& diseaseId) {
    return QStringList{"D11", "D31", "D41"}.contains(diseaseId);
}
}

