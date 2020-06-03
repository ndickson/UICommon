#pragma once

// This file defines the MainWindow class for UIContainer objects
// that correspond with operating system windows, as well as related functions.

#include "UIBox.h"
#include "UICommon.h"

#include <Array.h>
#include <ArrayDef.h>
#include <Box.h>
#include <Vec.h>
#include <Types.h>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

class MainWindowData;

class MainWindow : public UIContainer {
	MainWindowData* data;

	friend MainWindowData;

public:
	UICOMMON_LIBRARY_EXPORTED static const UIContainerClass staticType;

	MainWindow() : UIContainer(&staticType), data(nullptr) {}
	~MainWindow() = default;

private:
	static UIBox* construct();
	static void destruct(UIBox* box);

	static inline UIContainerClass initClass();
};

UICOMMON_LIBRARY_EXPORTED MainWindow* UIInit(
	int monitorNum,
	bool fullscreen,
	bool blankOtherMonitors,
	int horizontalResolution,
	bool exclusiveMouseMode);

UICOMMON_LIBRARY_EXPORTED void UILoop();

// Pass nullptr to clear the keyboard focus.
UICOMMON_LIBRARY_EXPORTED void setKeyFocus(const UIBox* box);

UICOMMON_LIBRARY_EXPORTED void setNeedRedraw();

class UIExitListener {
public:
	// This is an opportunity for anything needing cleanup before the
	// UI thread finishes, for example, this might tell drawing threads to
	// stop and wait for them to finish before returning.
	virtual void uiExiting() = 0;
};

UICOMMON_LIBRARY_EXPORTED void registerUIExitListener(UIExitListener* listener);

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
