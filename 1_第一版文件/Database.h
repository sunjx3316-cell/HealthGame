#pragma once

#include "Models.h"
#include <QMap>
#include <QVector>

class Database {
public:
    static Database& getInstance();
    
    // 初始化数据库 (硬编码50种疾病和对应药物)
    void init();
    
    // 查询
    QVector<Disease> getDiseasesByAgeAndOrgan(int age, const QString& organ);
    Disease getDiseaseById(const QString& id);
    QVector<Medicine> getMedicinesForDisease(const QString& diseaseId);
    QVector<Medicine> getAllMedicines();
    Medicine getMedicineById(const QString& id); // 新增

private:
    Database() = default;
    ~Database() = default;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    QMap<QString, Disease> diseases;
    QMap<QString, Medicine> medicines;
};
