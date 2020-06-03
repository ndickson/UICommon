#pragma once

// This file defines the Canvas onto which UIBox classes draw.

#include "UICommon.h"

#include <Vec.h>
#include <Types.h>

#include <memory>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

class Image {
	std::unique_ptr<Vec4f[]> pixels_;
	Vec2<size_t> size_;
public:
	INLINE Image() : pixels_(nullptr), size_(0,0) {}

	INLINE const Vec2<size_t>& size() const {
		return size_;
	}

	inline void setSize(size_t width, size_t height) {
		size_t newNumPixels = width*height;
		size_t oldNumPixels = (pixels_.get() != nullptr) ? (size_[0]*size_[1]) : 0;
		if (newNumPixels != oldNumPixels) {
			if (newNumPixels == 0) {
				pixels_.reset();
			}
			else {
				pixels_.reset(new Vec4f[newNumPixels]);
			}
		}
		size_[0] = width;
		size_[1] = height;
	}

	INLINE Vec4f* pixels() {
		return pixels_.get();
	}
	INLINE const Vec4f* pixels() const {
		return pixels_.get();
	}

	inline void clear() {
		pixels_.reset();
		size_ = Vec2<size_t>(0,0);
	}

	static inline void applyColour(Vec4f& colourBelow, const Vec4f& colourAbove) {
		// multiplied colour = above*aboveAlpha + below*belowAlpha*(1-aboveAlpha)
		// alpha = aboveAlpha + belowAlpha*(1-aboveAlpha)
		// unmultiplied colour = above + (below-above)*t, where
		// t = belowAlpha*(1-aboveAlpha) / alpha
		float aboveAlpha = colourAbove[3];
		if (aboveAlpha == 0) {
			return;
		}
		float belowAlpha = colourBelow[3];
		float extraAlpha = belowAlpha*(1-aboveAlpha);
		if (extraAlpha == 0) {
			colourBelow = colourAbove;
			return;
		}
		float alpha = aboveAlpha + extraAlpha;
		if (alpha == 0) {
			return;
		}
		float t = extraAlpha / alpha;
		Vec4f colour = colourAbove + (colourBelow-colourAbove)*t;
		colour[3] = alpha;
		colourBelow = colour;
	}

	UICOMMON_LIBRARY_EXPORTED void applyRectangle(const Box2f& rectangle, const Vec4f& colour);
	UICOMMON_LIBRARY_EXPORTED void applyImage(const Box2f& destRectangle, const Image& srcImage, const Box2f& srcRectangle);
};

class Canvas {
public:
	Image image;
};

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
