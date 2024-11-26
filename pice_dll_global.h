#ifndef PICE_DLL_GLOBAL_H
#define PICE_DLL_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(PICE_DLL_LIBRARY)
#define PICE_DLL_EXPORT Q_DECL_EXPORT
#else
#define PICE_DLL_EXPORT Q_DECL_IMPORT
#endif

#endif // PICE_DLL_GLOBAL_H
