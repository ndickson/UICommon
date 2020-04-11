#pragma once

// This file defines the base class for UI elements, UIBox,
// as well as UIContainer.

#include <Array.h>
#include <ArrayDef.h>
#include <Box.h>
#include <Vec.h>
#include <Types.h>

#include <memory>

OUTER_NAMESPACE_BEGIN
namespace UICommon {

using namespace OUTER_NAMESPACE::Common;

class Canvas;
struct UIBox;
struct UIContainer;

// This struct acts like a virtual table, except able to have data
// that's accessible without calling a function via a pointer.
struct UIBoxClass {
	bool isContainer = false;
	const char* typeName = nullptr;

	UIBox* (*construct)() = nullptr;
	void (*destruct)(UIBox*) = nullptr;

	void (*onMouseMove)(UIBox*) = nullptr;
	void (*onMouseDown)(UIBox*) = nullptr;
	void (*onMouseUp)(UIBox*) = nullptr;
	void (*onMouseScroll)(UIBox*) = nullptr;
	void (*onKeyDown)(UIBox*) = nullptr;
	void (*onKeyUp)(UIBox*) = nullptr;
	void (*onMouseEnter)(UIBox*) = nullptr;
	void (*onMouseExit)(UIBox*) = nullptr;
	void (*onResize)(UIBox*) = nullptr;
	void (*draw)(const UIBox*, Box2f&, Canvas&) = nullptr;
	const char* (*getTitle)(const UIBox*) = nullptr;
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

	static inline const UIBoxClass* getClass() {
		static UIBoxClass boxClass(initClass());
		return &boxClass;
	}

protected:
	UIBox(const UIBoxClass* c) : type(c), parent(nullptr), origin(0,0), size(0,0) {}
	UIBox() : UIBox(getClass()) {}

	static UIBox* construct() {
		return new UIBox();
	}

private:
	static UIBoxClass initClass() {
		UIBoxClass c;
		c.isContainer = false;
		c.typeName = "UIBox";
		c.construct = &construct;
		// No data to destruct, so destruct doesn't need to be set.
		return c;
	}
};

struct UIContainerClass : public UIBoxClass {
	// TODO: Add container-specific functions and class values here.
};

struct UIContainer : public UIBox {
	Array<std::unique_ptr<UIBox>> children;

	static inline const UIContainerClass* getClass() {
		static UIContainerClass containerClass(initClass());
		return &containerClass;
	}

	UIContainer() : UIBox(getClass()) {}

protected:
	static UIBox* construct() {
		return new UIContainer();
	}
	static void destruct(UIBox* box) {
		assert(box->type != nullptr);
		assert(box->type->isContainer);
		static_cast<UIContainer*>(box)->children.setCapacity(0);
	}

private:
	static UIContainerClass initClass() {
		UIContainerClass c;
		c.isContainer = true;
		c.typeName = "UIContainer";
		c.construct = &construct;
		c.destruct = &destruct;
		return c;
	}
};

} // namespace UICommon
OUTER_NAMESPACE_END

