#ifndef QFILECOPIER_GLOBAL_H
#define QFILECOPIER_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QFILECOPIER_LIBRARY)
#  define QFILECOPIERSHARED_EXPORT Q_DECL_EXPORT
#else
#  define QFILECOPIERSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // QFILECOPIER_GLOBAL_H
