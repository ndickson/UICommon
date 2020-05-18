#pragma once

// This file defines the base class for UI elements, UIBox,
// as well as UIContainer.

#include "UICommon.h"

#include <Array.h>
#include <ArrayDef.h>
#include <Box.h>
#include <Vec.h>
#include <Types.h>

#include <memory>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

class Canvas;
struct UIBox;
struct UIContainer;

struct MouseState {
	Vec2f position;
	uint64 buttonsDown;

	constexpr static uint64 LEFT_BIT = 1;
	constexpr static uint64 MIDDLE_BIT = 2;
	constexpr static uint64 RIGHT_BIT = 4;
	// Other bits can be used for applyng custom mouse modifiers.
};

struct KeyState;

// This struct acts like a virtual table, except able to have data
// that's accessible without calling a function via a pointer.
struct UIBoxClass {
	bool isContainer = false;

	// This being true means that a null isInside will act
	// as if it always returns true, i.e. the full box counts as inside the
	// UI component, else the null function will act as if it always returns
	// false, i.e. none of the box counts as inside the UI component.
	// It's also used by the default UIContainer::isInside, such that if
	// the mouse is outside any child boxes, the mouse will be considered inside
	// the container if this is true and outside the container if this is false.
	bool consumesMouse = true;

	const char* typeName = nullptr;

	UIBox* (*construct)() = nullptr;
	void (*destruct)(UIBox*) = nullptr;

	// Checks whether a position, that is already known to be inside the box,
	// should be considered inside the component or not.  This is for non-rectangular
	// UI components.
	//
	// If this is null, consumesMouse determines whether all points of the box are
	// considered inside (true), or no points of the box are considered inside (false).
	bool (*isInside)(UIBox& box, const Vec2f& position) = nullptr;

	// No other mouse functions will be called on a box until either onMouseEnter
	// has been called, (unless onMouseEnter is null).  After onMouseExit is called,
	// no other mouse functions will be called on a box until the next onMouseEnter.
	void (*onMouseEnter)(UIBox& box, const MouseState& state) = nullptr;

	// onMouseExit is called whenever a UIBox loses mouse focus.
	//
	// If a mouse button is down, mouse focus will not transfer to another box,
	// so onMouseExit will not be called in that case, until all mouse buttons
	// are released.  A possible exception is if the root window itself loses mouse
	// focus from the operating system, in which case, all boxes below it must
	// also lose mouse focus, so onMouseUp will be called for any mouse buttons
	// that are down, and then onMouseExit will be called, too.
	void (*onMouseExit)(UIBox& box, const MouseState& state) = nullptr;

	// In the case of a change in the mouse focus, onMouseMove is called on the
	// previous UIBox and *not* the UIBox that gained mouse focus.
	void (*onMouseMove)(UIBox& box, const Vec2f& change, const MouseState& state) = nullptr;

	// If onMouseDown is called, onMouseUp will be called on the same UIBox, for
	// the same mouse button, before onMouseExit is called on the UIBox.
	void (*onMouseDown)(UIBox& box, size_t button, const MouseState& state) = nullptr;
	void (*onMouseUp)(UIBox& box, size_t button, const MouseState& state) = nullptr;
	void (*onMouseScroll)(UIBox& box, float scrollAmount, const MouseState& state) = nullptr;

	// onKeyDown is called recursively down to the UIBox with keyboard focus,
	// so that higher-level shortcut keys, like Ctrl+S for saving or Esc to close
	// a dialog box.
	void (*onKeyDown)(UIBox& box, size_t key, const KeyState& state) = nullptr;
	void (*onKeyUp)(UIBox& box, size_t key, const KeyState& state) = nullptr;

	void (*onResize)(UIBox& box, const Vec2f& prevOrigin, const Vec2f& prevSize) = nullptr;

	// clipRectangle is in the space of this box, so unclipped would be from (0,0) to box->size.
	// targetRectangle is the rectangle of target that clipRectangle fits into.
	void (*draw)(const UIBox& box, const Box2f& clipRectangle, const Box2f& targetRectangle, Canvas& target) = nullptr;

	const char* (*getTitle)(const UIBox&) = nullptr;
};

// This is the base class of all UI elements.
// Check the type for how to interact with it.
struct UIBox {
	const UIBoxClass*const type;
	const UIContainer* parent;
	Vec2f origin;
	Vec2f size;

	~UIBox() {
		if (type != nullptr && type->destruct != nullptr) {
			type->destruct(this);
		}
	}

	UICOMMON_LIBRARY_EXPORTED static const UIBoxClass staticType;

	inline const UIContainer* getRoot() const;

protected:
	UIBox(const UIBoxClass* c) : type(c), parent(nullptr), origin(0,0), size(0,0) {
		assert(c != nullptr);
		assert(c->construct != construct);
	}
	UIBox() : UIBox(&staticType) {}

private:
	UICOMMON_LIBRARY_EXPORTED static UIBox* construct();
	static inline UIBoxClass initClass();
};

struct UIContainerClass : public UIBoxClass {
	// TODO: Add container-specific functions and class values here.
};

struct UIContainer : public UIBox {
	Array<std::unique_ptr<UIBox>> children;

	size_t keyFocusIndex;
	size_t mouseFocusIndex;

	Vec4f backgroundColour;

	constexpr static size_t INVALID_INDEX = ~size_t(0);

	UICOMMON_LIBRARY_EXPORTED static const UIContainerClass staticType;

	UIContainer() : UIContainer(&staticType) {}
	~UIContainer() = default;

protected:
	UIContainer(const UIContainerClass* c) : UIBox(c), keyFocusIndex(INVALID_INDEX), mouseFocusIndex(INVALID_INDEX), backgroundColour(0,0,0,0) {}

	UICOMMON_LIBRARY_EXPORTED static UIBox* construct();
	UICOMMON_LIBRARY_EXPORTED static void destruct(UIBox* box);

	UICOMMON_LIBRARY_EXPORTED static bool isInside(UIBox& box, const Vec2f& position);
	UICOMMON_LIBRARY_EXPORTED static void onMouseEnter(UIBox& box, const MouseState& state);
	UICOMMON_LIBRARY_EXPORTED static void onMouseExit(UIBox& box, const MouseState& state);
	UICOMMON_LIBRARY_EXPORTED static void onMouseMove(UIBox& box, const Vec2f& change, const MouseState& state);
	UICOMMON_LIBRARY_EXPORTED static void onMouseDown(UIBox& box, size_t button, const MouseState& state);
	UICOMMON_LIBRARY_EXPORTED static void onMouseUp(UIBox& box, size_t button, const MouseState& state);
	UICOMMON_LIBRARY_EXPORTED static void onMouseScroll(UIBox& box, float scrollAmount, const MouseState& state);

	UICOMMON_LIBRARY_EXPORTED static void updateMouseFocusIndex(UIContainer& container, const MouseState& state);

	UICOMMON_LIBRARY_EXPORTED static void draw(const UIBox& box, const Box2f& clipRectangle, const Box2f& targetRectangle, Canvas& target);

private:
	static inline UIContainerClass initClass();
};

const UIContainer* UIBox::getRoot() const {
	const UIContainer* root = nullptr;
	if (parent != nullptr) {
		root = parent;
		while (root->parent != nullptr) {
			root = root->parent;
		}
	}
	else {
		root = (type != nullptr && type->isContainer) ? static_cast<const UIContainer*>(this) : nullptr;
	}
	return root;
}

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
