#include "widgets/ImageButton.h"
#include "MainWindow.h"
#include <bmp/BMP.h>
#include <bmp/sRGB.h>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

const UIBoxClass ImageButton::staticType(ImageButton::initClass());

ImageButton::ImageButton(
	const char* upImageFilename,
	const char* hoverImageFilename,
	const char* downImageFilename,
	const char* disabledImageFilename
) :
	UIBox(&staticType),
	actionCallback(nullptr),
	callbackData(nullptr),
	isMouseInside(false),
	isMouseDown(false),
	isDisabled(false)
{
	Array<uint32> pixelsSRGB;
	size_t width; size_t height;
	bool hasAlpha;
	if (upImageFilename != nullptr) {
		bool success = bmp::ReadBMPFile(upImageFilename, pixelsSRGB, width, height, hasAlpha);
		if (success) {
			assert(pixelsSRGB.size() == width*height);

			// Convert pixelsSRGB to linear in upImage
			upImage.setSize(width, height);
			Vec4f* pixels = upImage.pixels();
			for (size_t i = 0, n = pixelsSRGB.size(); i < n; ++i) {
				pixels[i] = bmp::sRGBToLinear(pixelsSRGB[i]);
			}
		}
	}

	if (hoverImageFilename != nullptr) {
		bool success = bmp::ReadBMPFile(hoverImageFilename, pixelsSRGB, width, height, hasAlpha);
		if (success) {
			assert(pixelsSRGB.size() == width*height);

			// Convert pixelsSRGB to linear in hoverImage
			hoverImage.setSize(width, height);
			Vec4f* pixels = hoverImage.pixels();
			for (size_t i = 0, n = pixelsSRGB.size(); i < n; ++i) {
				pixels[i] = bmp::sRGBToLinear(pixelsSRGB[i]);
			}
		}
	}

	if (downImageFilename != nullptr) {
		bool success = bmp::ReadBMPFile(downImageFilename, pixelsSRGB, width, height, hasAlpha);
		if (success) {
			assert(pixelsSRGB.size() == width*height);

			// Convert pixelsSRGB to linear in downImage
			downImage.setSize(width, height);
			Vec4f* pixels = downImage.pixels();
			for (size_t i = 0, n = pixelsSRGB.size(); i < n; ++i) {
				pixels[i] = bmp::sRGBToLinear(pixelsSRGB[i]);
			}
		}
	}

	if (disabledImageFilename != nullptr) {
		bool success = bmp::ReadBMPFile(disabledImageFilename, pixelsSRGB, width, height, hasAlpha);
		if (success) {
			assert(pixelsSRGB.size() == width*height);

			// Convert pixelsSRGB to linear in disabledImage
			disabledImage.setSize(width, height);
			Vec4f* pixels = disabledImage.pixels();
			for (size_t i = 0, n = pixelsSRGB.size(); i < n; ++i) {
				pixels[i] = bmp::sRGBToLinear(pixelsSRGB[i]);
			}
		}
	}

	// FIXME: set size based on maximum width and height!!!
	width = upImage.size()[0];
	height= upImage.size()[1];
	if (hoverImage.size()[0] > width) {
		width = hoverImage.size()[0];
	}
	if (hoverImage.size()[1] > height) {
		width = hoverImage.size()[1];
	}
	if (downImage.size()[0] > width) {
		width = downImage.size()[0];
	}
	if (disabledImage.size()[1] > height) {
		width = disabledImage.size()[1];
	}
	size = Vec2f(width, height);
}

UIBox* ImageButton::construct() {
	return new ImageButton(nullptr, nullptr, nullptr);
}

void ImageButton::destruct(UIBox* box) {
	assert(box->type != nullptr);
	ImageButton* imageButton = static_cast<ImageButton*>(box);
	imageButton->upImage.clear();
	imageButton->hoverImage.clear();
	imageButton->downImage.clear();
	imageButton->disabledImage.clear();
}

void ImageButton::onMouseDown(UIBox& box, size_t button, const MouseState& state) {
	assert(box.type != nullptr);
	ImageButton& imageButton = static_cast<ImageButton&>(box);
	imageButton.isMouseDown = true;
	setNeedRedraw();
}

void ImageButton::onMouseUp(UIBox& box, size_t button, const MouseState& state) {
	assert(box.type != nullptr);
	ImageButton& imageButton = static_cast<ImageButton&>(box);
	imageButton.isMouseDown = false;
	if (imageButton.isMouseInside && imageButton.actionCallback != nullptr) {
		imageButton.actionCallback(imageButton);
	}
	setNeedRedraw();
}

void ImageButton::onMouseEnter(UIBox& box, const MouseState& state) {
	assert(box.type != nullptr);
	ImageButton& imageButton = static_cast<ImageButton&>(box);
	imageButton.isMouseInside = true;
	setNeedRedraw();
}

void ImageButton::onMouseExit(UIBox& box, const MouseState& state) {
	assert(box.type != nullptr);
	ImageButton& imageButton = static_cast<ImageButton&>(box);
	imageButton.isMouseInside = false;
	setNeedRedraw();
}

void ImageButton::draw(const UIBox& box, const Box2f& clipRectangle, const Box2f& targetRectangle,Canvas& target) {
	const ImageButton& button = static_cast<const ImageButton&>(box);

	const Image* image;
	if (button.isDisabled && (button.disabledImage.pixels() != nullptr)) {
		image = &button.disabledImage;
	}
	else if (!button.isMouseInside && (!button.isMouseDown || (button.hoverImage.pixels() != nullptr))) {
		image = &button.upImage;
	}
	else if ((button.isMouseInside != button.isMouseDown) && (button.hoverImage.pixels() != nullptr)) {
		image = &button.hoverImage;
	}
	else {
		image = &button.downImage;
	}
	target.image.applyImage(targetRectangle, *image, clipRectangle);
}

UIBoxClass ImageButton::initClass() {
	UIBoxClass c;
	c.isContainer = false;
	c.typeName = "ImageButton";
	c.construct = &construct;
	c.destruct = &destruct;
	c.onMouseDown = &onMouseDown;
	c.onMouseUp = &onMouseUp;
	c.onMouseEnter = &onMouseEnter;
	c.onMouseExit = &onMouseExit;
	c.draw = &draw;
	return c;
}

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
