#include "calibgui.h"
#include <QApplication>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    CalibGui w;
    w.show();
    QApplication::setWindowIcon((QIcon("calib_ico.png")));


    return QApplication::exec();
}
