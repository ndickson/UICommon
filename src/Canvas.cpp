#include "Canvas.h"
#include <Box.h>
#include <Types.h>

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

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
