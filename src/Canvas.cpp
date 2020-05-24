#include "Canvas.h"
#include <Box.h>
#include <Types.h>

#include <assert.h>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

static void applySingleLine(
	const Vec4f& colour,
	float mainOpacity, float firstOpacity, float lastOpacity,
	Vec4f* beginPixel,
	size_t step,
	size_t n
) {
	if (firstOpacity != 0) {
		Vec4f cornerColour(colour[0], colour[1], colour[2], colour[3]*mainOpacity*firstOpacity);
		Image::applyColour(*(beginPixel - step), cornerColour);
	}

	Vec4f edgeColour(colour[0], colour[1], colour[2], colour[3]*mainOpacity);
	Vec4f* pixel = beginPixel;
	for (size_t i = 0; i < n; ++i) {
		Image::applyColour(*pixel, edgeColour);
		pixel += step;
	}

	if (lastOpacity != 0) {
		Vec4f cornerColour(colour[0], colour[1], colour[2], colour[3]*mainOpacity*lastOpacity);
		Image::applyColour(*pixel, cornerColour);
	}
}

void Image::applyRectangle(const Box2f& rectangle, const Vec4f& colour) {
	if (colour[3] <= 0) {
		// Fully transparent colour, so nothing to do.
		return;
	}

	const Box2f clipped(
		Vec2f(
			(rectangle[0][0] < 0) ? 0.0f : rectangle[0][0],
			(rectangle[1][0] < 0) ? 0.0f : rectangle[1][0]
		),
		Vec2f(
			(rectangle[0][1] >= size[0]) ? size[0] : rectangle[0][1],
			(rectangle[1][1] >= size[1]) ? size[1] : rectangle[1][1]
		)
	);

	Vec2<size_t> minFloor{size_t(clipped[0][0]), size_t(clipped[1][0])};
	Vec2<size_t> maxFloor{size_t(clipped[0][1]), size_t(clipped[1][1])};
	Vec2<size_t> minCeil{
		minFloor[0] + (minFloor[0] < clipped[0][0]),
		minFloor[1] + (minFloor[1] < clipped[1][0])
	};
	const Box2<size_t> contractedRectangle(minCeil, maxFloor);

	Vec4f* beginPixels = pixels.get();
	beginPixels += contractedRectangle[1][0]*size[0] + contractedRectangle[0][0];
	const size_t midHeight = contractedRectangle[1][1] - contractedRectangle[1][0];
	const size_t midWidth = contractedRectangle[0][1] - contractedRectangle[0][0];

	float bottomOpacity = contractedRectangle[1][0] - clipped[1][0];
	float topOpacity    = clipped[1][1] - contractedRectangle[1][1];
	float leftOpacity   = contractedRectangle[0][0] - clipped[0][0];
	float rightOpacity  = clipped[0][1] - contractedRectangle[0][1];

	if (midHeight == ~size_t(0)) {
		// Strictly inside a single pixel vertically
		const float verticalOpacity = clipped[1][1] - clipped[1][0];

		if (midWidth == ~size_t(0)) {
			// Also strictly inside a single pixel horizontally
			const float areaOpacity = (clipped[0][1] - clipped[0][0])*verticalOpacity;
			Vec4f areaColour(colour[0], colour[1], colour[2], colour[3]*areaOpacity);
			applyColour(*(beginPixels - size[0] - 1), areaColour);
			return;
		}

		applySingleLine(colour, verticalOpacity, leftOpacity, rightOpacity, beginPixels - size[0], 1, midWidth);

		return;
	}
	if (midWidth == ~size_t(0)) {
		// Strictly inside a single pixel horizontally
		const float horizontalOpacity = clipped[0][1] - clipped[0][0];

		applySingleLine(colour, horizontalOpacity, bottomOpacity, topOpacity, beginPixels - 1, size[0], midHeight);

		return;
	}

	// Middle part of the rectangle
	if (colour[3] >= 1) {
		// Opaque
		Vec4f* row = beginPixels;
		for (size_t y = 0; y < midHeight; ++y) {
			Vec4f* pixel = row;
			for (size_t x = 0; x < midWidth; ++x) {
				*pixel = colour;
				++pixel;
			}
			row += size[0];
		}
	}
	else {
		// Transparent
		Vec4f* row = beginPixels;
		for (size_t y = 0; y < midHeight; ++y) {
			Vec4f* pixel = row;
			for (size_t x = 0; x < midWidth; ++x) {
				applyColour(*pixel, colour);
				++pixel;
			}
			row += size[0];
		}
	}

	// Bottom edge
	if (bottomOpacity != 0) {
		applySingleLine(colour, bottomOpacity, leftOpacity, rightOpacity, beginPixels - size[0], 1, midWidth);
	}

	// Left edge
	if (leftOpacity != 0) {
		Vec4f edgeColour(colour[0], colour[1], colour[2], colour[3]*leftOpacity);
		Vec4f* pixel = beginPixels - 1;
		for (size_t y = 0; y < midHeight; ++y) {
			applyColour(*pixel, edgeColour);
			pixel += size[0];
		}
	}

	// Right edge
	if (rightOpacity != 0) {
		Vec4f edgeColour(colour[0], colour[1], colour[2], colour[3]*rightOpacity);
		Vec4f* pixel = beginPixels + midWidth + 1;
		for (size_t y = 0; y < midHeight; ++y) {
			applyColour(*pixel, edgeColour);
			pixel += size[0];
		}
	}

	// Top edge
	if (topOpacity != 0) {
		Vec4f* endRowPixels = beginPixels + midHeight*size[0];
		applySingleLine(colour, topOpacity, leftOpacity, rightOpacity, endRowPixels, 1, midWidth);
	}
}

void Image::applyImage(const Box2f& destRectangleIn, const Image& srcImage, const Box2f& srcRectangleIn) {
	Box2f destRectangle(destRectangleIn);
	Box2f srcRectangle(srcRectangleIn);

	// Flip the rectangles such that destRectangle has only positive size in both dimensions.
	Vec2f destSize = destRectangle.size();
	for (size_t axis = 0; axis < 2; ++axis) {
		if (destSize[axis] < 0) {
			destSize[axis] = -destSize[axis];
			std::swap(destRectangle[axis][0], destRectangle[axis][1]);
			std::swap(srcRectangle[axis][0], srcRectangle[axis][1]);
		}
	}

	// Limit destRectangle to the bounds of the destination image.
	const Box2f clippedDest(
		Vec2f(
			(destRectangle[0][0] < 0) ? 0.0f : destRectangle[0][0],
			(destRectangle[1][0] < 0) ? 0.0f : destRectangle[1][0]
		),
		Vec2f(
			(destRectangle[0][1] >= size[0]) ? size[0] : destRectangle[0][1],
			(destRectangle[1][1] >= size[1]) ? size[1] : destRectangle[1][1]
		)
	);

	// Return if the destRectangle is entirely clipped away.
	// These conditions are written in this unusual way to hopefully return
	// if destRectangle contained NaN values.
	if (!(clippedDest[0][0] < clippedDest[0][1]) || !(clippedDest[1][0] < clippedDest[1][1])) {
		return;
	}

	Vec2f srcSize = srcRectangle.size();
	Vec2f srcScaleFromDest(srcSize / destSize);

	// Apply the corresponding clip to the start of srcRectangle.
	Vec2f clippedSrcStart(
		(clippedDest[0][0]-destRectangle[0][0])*srcScaleFromDest[0] + srcRectangle[0][0],
		(clippedDest[1][0]-destRectangle[1][0])*srcScaleFromDest[1] + srcRectangle[1][0]
	);

	// Contract the destination to only include complete pixels.
	Vec2<size_t> minFloor{size_t(clippedDest[0][0]), size_t(clippedDest[1][0])};
	Vec2<size_t> maxFloor{size_t(clippedDest[0][1]), size_t(clippedDest[1][1])};
	Vec2<size_t> minCeil{
		minFloor[0] + (minFloor[0] < clippedDest[0][0]),
		minFloor[1] + (minFloor[1] < clippedDest[1][0])
	};
	const Box2<size_t> contractedRectangle(minCeil, maxFloor);

	Vec4f* beginDestPixels = pixels.get();
	beginDestPixels += contractedRectangle[1][0]*size[0] + contractedRectangle[0][0];
	const size_t midHeight = contractedRectangle[1][1] - contractedRectangle[1][0];
	const size_t midWidth = contractedRectangle[0][1] - contractedRectangle[0][0];

	float bottomOpacity = contractedRectangle[1][0] - clippedDest[1][0];
	float topOpacity    = clippedDest[1][1] - contractedRectangle[1][1];
	float leftOpacity   = contractedRectangle[0][0] - clippedDest[0][0];
	float rightOpacity  = clippedDest[0][1] - contractedRectangle[0][1];

	const Vec4f*const srcPixels = srcImage.pixels.get();

	// Loop over the full destination pixels, checking bounds on the source image.
	for (size_t y = 0; y < midHeight; ++y) {
		float srcy = srcScaleFromDest[1]*(y + bottomOpacity) + clippedSrcStart[1];
		if (srcy < 0) {
			continue;
		}
		size_t srcyi = size_t(srcy);
		if (srcyi >= srcImage.size[1]) {
			break;
		}
		// FIXME: Handle 1-pixel height source image!!!
		assert(srcImage.size[1] >= 2);
		if (srcyi >= srcImage.size[1]-1) {
			srcyi = srcImage.size[1]-2;
		}
		float srcyt = srcy - srcyi;
		for (size_t x = 0; x < midWidth; ++x) {
			float srcx = srcScaleFromDest[0]*(x + leftOpacity) + clippedSrcStart[0];
			if (srcx < 0) {
				continue;
			}
			size_t srcxi = size_t(srcx);
			if (srcxi >= srcImage.size[0]) {
				break;
			}
			// FIXME: Handle 1-pixel width source image!!!
			assert(srcImage.size[0] >= 2);
			if (srcxi >= srcImage.size[0]-1) {
				srcxi = srcImage.size[0]-2;
			}
			float srcxt = srcx - srcxi;

			size_t desti = y*size[0] + x;

			// Bilinear interpolation
			size_t i00 = srcyi*srcImage.size[0] + srcxi;
			size_t i10 = i00 + 1;
			size_t i01 = i00 + srcImage.size[0];
			size_t i11 = i01 + 1;

			Vec4f v0 = srcPixels[i00] + srcxt*(srcPixels[i10] - srcPixels[i00]);
			Vec4f v1 = srcPixels[i01] + srcxt*(srcPixels[i11] - srcPixels[i01]);
			Vec4f v = v0 + srcyt*(v1 - v0);

			applyColour(beginDestPixels[desti], v);
		}
	}

	// FIXME: Apply contributions to incomplete pixels!!!
}

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
