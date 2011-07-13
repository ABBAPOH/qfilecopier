#ifndef QFILECOPIER_H
#define QFILECOPIER_H

#include "QFileCopier_global.h"

#include <QtCore/QObject>

class QFILECOPIERSHARED_EXPORT QFileCopier : public QObject
{
    Q_OBJECT
public:
    explicit QFileCopier(QObject *parent = 0);
};

#endif // QFILECOPIER_H
