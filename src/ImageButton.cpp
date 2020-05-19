#include "ImageButton.h"

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

const UIBoxClass ImageButton::staticType;

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
	// FIXME: Initialize images if names aren't null!!!
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
}

void ImageButton::onMouseUp(UIBox& box, size_t button, const MouseState& state) {
	assert(box.type != nullptr);
	ImageButton& imageButton = static_cast<ImageButton&>(box);
	imageButton.isMouseDown = false;
	if (imageButton.isMouseInside) {
		imageButton.actionCallback(imageButton);
	}
}

void ImageButton::onMouseEnter(UIBox& box, const MouseState& state) {
	assert(box.type != nullptr);
	ImageButton& imageButton = static_cast<ImageButton&>(box);
	imageButton.isMouseInside = true;
}

void ImageButton::onMouseExit(UIBox& box, const MouseState& state) {
	assert(box.type != nullptr);
	ImageButton& imageButton = static_cast<ImageButton&>(box);
	imageButton.isMouseInside = false;
}

void ImageButton::draw(const UIBox& box, const Box2f& clipRectangle, const Box2f& targetRectangle,Canvas& target) {
	// FIXME: Implement this!!!
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
