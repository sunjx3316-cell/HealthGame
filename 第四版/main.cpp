#include "mainwindow.h"
#include <QApplication>
#include <QSurfaceFormat>
#include <QTimer>
#include <QFile>
#include <QElapsedTimer>
int main(int argc,char* argv[]){
    QSurfaceFormat format;
    format.setVersion(3,3);format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);format.setStencilBufferSize(8);format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc,argv);
    app.setApplicationName("HealthGame");app.setOrganizationName("HealthGame");
    const auto args=app.arguments();const int check=args.indexOf("--render-check");
    MainWindow window(nullptr,check<0);window.show();
    QTimer checkTimer;QElapsedTimer timeout;
    if(check>=0){
        if(check+1>=args.size())return 2;
        timeout.start();checkTimer.setInterval(100);
        QObject::connect(&checkTimer,&QTimer::timeout,&app,[&]{
            auto* view=window.anatomyView();
            if(view->rendererReady()){
                view->setAnimationEnabled(false);window.selectOrgan("心脏");view->setAge(30);
                auto image=view->grabFramebuffer();
                app.exit(!image.isNull()&&image.save(args[check+1])?0:3);
            }else if(timeout.elapsed()>20000||!view->rendererError().isEmpty())app.exit(4);
        });checkTimer.start();
    }
    return app.exec();
}
