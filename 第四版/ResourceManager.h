#pragma once

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QMap>

class ResourceManager : public QObject {
    Q_OBJECT
public:
    static ResourceManager& getInstance();
    
    // 初始化，设置本地资源目录
    void init();
    
    // 同步获取本地贴图，如果不存在返回一个空的/默认的 QPixmap
    QPixmap getTexture(const QString& fileName);

private:
    ResourceManager(QObject *parent = nullptr);
    ~ResourceManager() = default;
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    QString assetsDir;
    QMap<QString, QPixmap> textureCache; // 内存缓存，避免重复读取硬盘
};
