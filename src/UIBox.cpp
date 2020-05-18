#include "UIBox.h"

#include <Box.h>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

UIBox* UIBox::construct() {
	return new UIBox();
}

UIBoxClass UIBox::initClass() {
	UIBoxClass c;
	c.isContainer = false;
	c.typeName = "UIBox";
	c.construct = &construct;
	// No data to destruct, so destruct doesn't need to be set.
	return c;
}

const UIBoxClass UIBox::staticType(UIBox::initClass());

static size_t positionToChildIndex(const Vec2f& position, const Array<std::unique_ptr<UIBox>>& children) {
	// Check if any child boxes contain the mouse position, in reverse order,
	// since it's the topmost-drawn first order.
	for (size_t i = children.size(); i > 0; ) {
		--i;
		UIBox& child = *children[i];
		const Vec2f& c0 = child.origin;
		const Vec2f& size = child.size;
		if (position[0] >= c0[0] && position[0]-c0[0] < size[0] &&
			position[1] >= c0[1] && position[1]-c0[1] < size[1]
		) {
			bool inside = child.type->consumesMouse;
			auto childIsInside = child.type->isInside;
			if (childIsInside != nullptr) {
				inside = (*childIsInside)(child, position-c0);
			}
			if (inside) {
				return i;
			}
		}
	}
	return UIContainer::INVALID_INDEX;
}

UIContainerClass UIContainer::initClass() {
	UIContainerClass c;
	c.isContainer = true;
	c.typeName = "UIContainer";
	c.construct = &construct;
	c.destruct = &destruct;

	c.isInside = &isInside;
	c.onMouseEnter = &onMouseEnter;
	c.onMouseExit = &onMouseExit;
	c.onMouseMove = &onMouseMove;
	c.onMouseDown = &onMouseDown;
	c.onMouseUp = &onMouseUp;
	c.onMouseScroll = &onMouseScroll;

	c.draw = &draw;

	return c;
}

const UIContainerClass UIContainer::staticType(UIContainer::initClass());

void UIContainer::updateMouseFocusIndex(UIContainer& container, const MouseState& state) {
	Array<std::unique_ptr<UIBox>>& children = container.children;

	const Vec2f& position = state.position;

	// Check if the new position is inside the container.
	// If it's not, there should be no new mouse focus child.
	bool insideContainer = (
		position[0] >= 0.0f && position[0] < container.size[0] &&
		position[1] >= 0.0f && position[1] < container.size[1]
	);
	auto containerIsInside = container.type->isInside;
	if (insideContainer && containerIsInside != nullptr &&
		(containerIsInside != UIContainer::isInside || !container.type->consumesMouse)
	) {
		insideContainer = (*containerIsInside)(container, position);
	}

	size_t childIndex = INVALID_INDEX;
	if (insideContainer) {
		childIndex = positionToChildIndex(position, children);
	}

	// If the mouse focus child index hasn't changed, there's nothing more to do.
	if (childIndex == container.mouseFocusIndex) {
		return;
	}

	// If there was a previous mouse focus child, exit it.
	if (container.mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container.mouseFocusIndex];
		auto childOnMouseExit = child.type->onMouseExit;
		if (childOnMouseExit != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseExit)(child, childMouseState);
		}
	}
	container.mouseFocusIndex = childIndex;

	// If there is a new mouse focus child, enter it.
	if (childIndex != INVALID_INDEX) {
		UIBox& child = *children[childIndex];
		auto childOnMouseEnter = child.type->onMouseEnter;
		if (childOnMouseEnter != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseEnter)(child, childMouseState);
		}
	}

	// NOTE: The caller will handle exiting container if !insideContainer.
}

UIBox* UIContainer::construct() {
	return new UIContainer();
}
void UIContainer::destruct(UIBox* box) {
	assert(box->type != nullptr);
	assert(box->type->isContainer);
	static_cast<UIContainer*>(box)->children.setCapacity(0);
}

bool UIContainer::isInside(UIBox& box, const Vec2f& position) {
	assert(box.type != nullptr);
	assert(box.type->isContainer);
	UIContainer& container = static_cast<UIContainer&>(box);

	if (container.type->consumesMouse) {
		// Always inside if consumesMouse.
		return true;
	}

	// Otherwise, inside if inside any child box.
	// NOTE: Order doesn't matter
	Array<std::unique_ptr<UIBox>>& children = container.children;
	size_t index = positionToChildIndex(position, children);

	return (index != INVALID_INDEX);
}

void UIContainer::onMouseEnter(UIBox& box, const MouseState& state) {
	assert(box.type != nullptr);
	assert(box.type->isContainer);
	UIContainer& container = static_cast<UIContainer&>(box);

	Array<std::unique_ptr<UIBox>>& children = container.children;

	// No child should have mouse focus in a container that hasn't been entered yet.
	assert(container.mouseFocusIndex == INVALID_INDEX);

	size_t index = positionToChildIndex(state.position, children);
	container.mouseFocusIndex = index;

	if (index != INVALID_INDEX) {
		UIBox& child = *children[index];
		if (child.type->onMouseEnter != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*child.type->onMouseEnter)(child, childMouseState);
		}
	}
}

void UIContainer::onMouseExit(UIBox& box, const MouseState& state) {
	assert(box.type != nullptr);
	assert(box.type->isContainer);
	UIContainer& container = static_cast<UIContainer&>(box);

	Array<std::unique_ptr<UIBox>>& children = container.children;

	// If this is ocurring due to onMouseMove, onMouseExit should have
	// already been called on any mouse focus child, and mouseFocusIndex
	// should have been cleared, but just in case, we recurse here.
	//
	// That said, there may be cases where a component is entered in a
	// region that's blocked by a higher level component, with the
	// way updateMouseFocusIndex currently handles it.
	// TODO: This may be worth changing, to avoid entering and immediately exiting.
	assert(container.mouseFocusIndex == INVALID_INDEX);

	if (container.mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container.mouseFocusIndex];
		auto childOnMouseExit = child.type->onMouseExit;
		if (childOnMouseExit != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseExit)(child, childMouseState);
		}
		container.mouseFocusIndex = INVALID_INDEX;
	}
}

void UIContainer::onMouseMove(UIBox& box, const Vec2f& change, const MouseState& state) {
	assert(box.type != nullptr);
	assert(box.type->isContainer);
	UIContainer& container = static_cast<UIContainer&>(box);

	Array<std::unique_ptr<UIBox>>& children = container.children;

	// Recurse on the mouseFocusIndex child before checking whether the
	// new position is in a different child or not.
	if (container.mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container.mouseFocusIndex];
		auto childOnMouseMove = child.type->onMouseMove;
		if (childOnMouseMove != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseMove)(child, change, childMouseState);
		}
	}

	// If any buttons are down, the mouse focus stays unchanged.
	if (state.buttonsDown != 0) {
		return;
	}

	updateMouseFocusIndex(container, state);

	// NOTE: The caller will handle exiting container if !insideContainer.
}

void UIContainer::onMouseDown(UIBox& box, size_t button, const MouseState& state) {
	assert(box.type != nullptr);
	assert(box.type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(&box);

	Array<std::unique_ptr<UIBox>>& children = container->children;

	// NOTE: No need to set keyboard focus here, because UIBox
	// subclasses that actually want keyboard focus will request it.

	// Recurse on the mouseFocusIndex child.
	Vec2f position = state.position;
	while (container->mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container->mouseFocusIndex];
		auto childOnMouseDown = child.type->onMouseDown;
		// Avoid actual recursion if child just has regular container behaviour.
		if (childOnMouseDown == UIContainer::onMouseDown) {
			assert(child.type->isContainer);
			position -= child.origin;
			container = static_cast<UIContainer*>(&child);
			continue;
		}
		if (childOnMouseDown != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position = position - child.origin;
			(*childOnMouseDown)(child, button, childMouseState);
		}
		break;
	}

	// Pressing a mouse button down never changes mouse focus, so that's all.
}

void UIContainer::onMouseUp(UIBox& box,size_t button,const MouseState& state) {
	assert(box.type != nullptr);
	assert(box.type->isContainer);
	UIContainer& container = static_cast<UIContainer&>(box);

	Array<std::unique_ptr<UIBox>>& children = container.children;

	// Recurse on the mouseFocusIndex child before checking whether the
	// position is in a different child or not.
	if (container.mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container.mouseFocusIndex];
		auto childOnMouseUp = child.type->onMouseUp;
		if (childOnMouseUp != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseUp)(child, button, childMouseState);
		}
	}

	// If any buttons are down, the mouse focus stays unchanged.
	if (state.buttonsDown != 0) {
		return;
	}

	updateMouseFocusIndex(container, state);

	// NOTE: The caller will handle exiting container if !insideContainer.
}

void UIContainer::onMouseScroll(UIBox& box, float scrollAmount, const MouseState& state) {
	assert(box.type != nullptr);
	assert(box.type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(&box);

	Array<std::unique_ptr<UIBox>>& children = container->children;

	// Recurse on the mouseFocusIndex child.
	Vec2f position = state.position;
	while (container->mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container->mouseFocusIndex];
		auto childOnMouseScroll = child.type->onMouseScroll;
		// Avoid actual recursion if child just has regular container behaviour.
		if (childOnMouseScroll == UIContainer::onMouseScroll) {
			assert(child.type->isContainer);
			position -= child.origin;
			container = static_cast<UIContainer*>(&child);
			continue;
		}
		if (childOnMouseScroll != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position = position - child.origin;
			(*childOnMouseScroll)(child, scrollAmount, childMouseState);
		}
		break;
	}

	// Scrolling the mouse wheel never changes mouse focus, so that's all.
}

void UIContainer::draw(const UIBox& box, const Box2f& clipRectangle, const Box2f& targetRectangle, Canvas& target) {
	assert(box.type != nullptr);
	assert(box.type->isContainer);
	const UIContainer& container = static_cast<const UIContainer&>(box);

	if (container.backgroundColour[3] != 0) {
		// Fill the targetRectangle with the background colour.
		// FIXME: Implement this!!!
	}

	const Array<std::unique_ptr<UIBox>>& children = container.children;

	Vec2f scale(1.0f, 1.0f);
	const Vec2f clipSize = clipRectangle.size();
	const Vec2f targetSize = targetRectangle.size();
	if (clipSize != targetSize) {
		scale = (targetRectangle.size() / clipRectangle.size());
	}

	for (size_t i = 0, n = children.size(); i < n; ++i) {
		const UIBox& child = *children[i];

		auto childDraw = child.type->draw;
		if (childDraw == nullptr) {
			continue;
		}
		
		Box2f childClipRectangle(clipRectangle);
		bool empty = false;
		// Clip the rectangle.
		for (size_t axis = 0; axis < 2; ++axis) {
			// The min of the parent clip rectangle in the child's space is usually
			// negative, so it must be forced up to zero.
			if (childClipRectangle[axis][0] < child.origin[axis]) {
				childClipRectangle[axis][0] = child.origin[axis];
			}
			// The max of the parent clip rectangle in the child's space is usually
			// past the max of the child, so it must be forced down to that.
			if (childClipRectangle[axis][1] > child.origin[axis]+child.size[axis]) {
				childClipRectangle[axis][1] = child.origin[axis]+child.size[axis];
			}

			if (childClipRectangle[axis][1] <= childClipRectangle[axis][0]) {
				empty = true;
				break;
			}
		}

		// Clip rectangle is empty, so there's nothing to draw.
		if (empty) {
			continue;
		}

		// Compute corresponding child target rectangle based on
		// childClipRectangle's relation to clipRectangle and targetRectangle.
		// This takes into account if there's been a simple scale along the way.
		Box2f childTargetRectangle(
			targetRectangle.min() + (childClipRectangle.min() - clipRectangle.min())*scale,
			targetRectangle.max() + (childClipRectangle.max() - clipRectangle.max())*scale
		);

		// Shift the rectangle.
		childClipRectangle -= child.origin;

		childDraw(child, childClipRectangle, childTargetRectangle, target);
	}
}

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
