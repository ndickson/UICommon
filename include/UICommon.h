#pragma once

#include <Types.h>

#define UICOMMON_LIBRARY_NAMESPACE    UICommon
#define UICOMMON_LIBRARY_NAMESPACE_BEGIN namespace UICOMMON_LIBRARY_NAMESPACE {
#define UICOMMON_LIBRARY_NAMESPACE_END   }

#if BUILDING_UICOMMON_LIBRARY && HAVE_VISIBILITY
#define UICOMMON_LIBRARY_EXPORTED __attribute__((__visibility__("default")))
#elif BUILDING_UICOMMON_LIBRARY && defined(_MSC_VER)
#define UICOMMON_LIBRARY_EXPORTED __declspec(dllexport)
#elif defined(_MSC_VER)
#define UICOMMON_LIBRARY_EXPORTED __declspec(dllimport)
#else
#define UICOMMON_LIBRARY_EXPORTED
#endif

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

using namespace OUTER_NAMESPACE::COMMON_LIBRARY_NAMESPACE;

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
