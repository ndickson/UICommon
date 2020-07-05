#pragma once

#include "../UIBox.h"
#include "../UICommon.h"
#include "../Canvas.h"

#include <Types.h>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

struct ImageButton : public UIBox {
	// This is the image if !isDisabled && !isMouseInside && !isMouseDown.
	Image upImage;

	// This is the image if !isDisabled && (isMouseInside != isMouseDown).
	Image hoverImage;

	// This is the image if !isDisabled && isMouseInside && isMouseDown.
	Image downImage;

	// This is the image if isDisabled.
	Image disabledImage;

	// This function will be called when the button is activated.
	// It seems unlikely that most buttons would need more than one listener,
	// but if that's needed, a callback can have callbackData point to an array
	// of listeners for it to call.
	void (*actionCallback)(ImageButton&);
	void* callbackData;

	bool isMouseInside;
	bool isMouseDown;
	bool isDisabled;


	UICOMMON_LIBRARY_EXPORTED static const UIBoxClass staticType;

	UICOMMON_LIBRARY_EXPORTED ImageButton(
		const char* upImageFilename,
		const char* hoverImageFilename,
		const char* downImageFilename,
		const char* disabledImageFilename = nullptr
	);

protected:
	UICOMMON_LIBRARY_EXPORTED static UIBox* construct();
	UICOMMON_LIBRARY_EXPORTED static void destruct(UIBox* box);
	UICOMMON_LIBRARY_EXPORTED static void onMouseDown(UIBox& box, size_t button, const MouseState& state);
	UICOMMON_LIBRARY_EXPORTED static void onMouseUp(UIBox& box, size_t button, const MouseState& state);
	UICOMMON_LIBRARY_EXPORTED static void onMouseEnter(UIBox& box, const MouseState& state);
	UICOMMON_LIBRARY_EXPORTED static void onMouseExit(UIBox& box, const MouseState& state);

	UICOMMON_LIBRARY_EXPORTED static void draw(const UIBox& box, const Box2f& clipRectangle, const Box2f& targetRectangle, Canvas& target);

private:
	static inline UIBoxClass initClass();
};

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
