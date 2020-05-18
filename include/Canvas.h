#pragma once

// This file defines the Canvas onto which UIBox classes draw.

#include "UICommon.h"

#include <Vec.h>
#include <Types.h>

#include <memory>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

class Canvas {
	std::unique_ptr<Vec4f[]> pixels;
	Vec2<size_t> size;
};

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
