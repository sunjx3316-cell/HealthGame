#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // 创建 QApplication 对象，管理应用的控制流和主要设置
    QApplication a(argc, argv);
    
    // 实例化主窗口
    MainWindow w;
    w.show();
    
    // 进入主事件循环
    return a.exec();
}
