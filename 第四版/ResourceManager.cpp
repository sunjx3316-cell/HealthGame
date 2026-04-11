#include "ResourceManager.h"
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QDebug>

ResourceManager& ResourceManager::getInstance() {
    static ResourceManager instance;
    return instance;
}

ResourceManager::ResourceManager(QObject *parent) : QObject(parent) {
}

void ResourceManager::init() {
    // 改为使用 Qt 的内置资源系统 (Resource System)
    // 所有的资源都会被打包进可执行文件中，路径前缀为 ":/assets/textures/"
    assetsDir = ":/assets/textures";
    qDebug() << "使用 Qt 资源系统，贴图根路径：" << assetsDir;
}

QPixmap ResourceManager::getTexture(const QString& fileName) {
    if (textureCache.contains(fileName)) {
        return textureCache[fileName]; // 内存命中，直接返回
    }

    QString fullPath = assetsDir + "/" + fileName;
    QPixmap pixmap(fullPath);
    
    if (pixmap.isNull()) {
        // 如果开发者没有准备这张图片，我们返回一张带有警告文字的纯色图片作为占位符
        QPixmap placeholder(512, 512);
        placeholder.fill(Qt::gray);
        return placeholder;
    }

    // 缓存并返回
    textureCache[fileName] = pixmap;
    return pixmap;
}
