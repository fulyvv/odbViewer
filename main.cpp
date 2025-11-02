#include "mainwindow.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>

int main(int argc, char *argv[])
{
    // 为 QVTKOpenGLNativeWidget 配置默认的 OpenGL 上下文格式
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
