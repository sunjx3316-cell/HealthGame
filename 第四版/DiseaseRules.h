#pragma once

#include "Models.h"

// Explicit game-design metadata. These rules describe this simulation, not
// clinical recurrence rates, diagnosis criteria or real treatment advice.
namespace DiseaseRules {
bool latentProgressive(const Disease& disease);
bool congenital(const Disease& disease);
bool requiresProcedure(const Disease& disease);
bool major(const Disease& disease);
QString requiredStructure(const Disease& disease);
QString removedStructure(const QString& medicineId);
int recoveryTicks(const Disease& disease);
bool isChronicLung(const QString& diseaseId);
}

