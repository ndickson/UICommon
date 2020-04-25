#include "UIBox.h"

OUTER_NAMESPACE_BEGIN
namespace UICommon {

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
				inside = (*childIsInside)(&child, position-c0);
			}
			if (inside) {
				return i;
			}
		}
	}
	return UIContainer::INVALID_INDEX;
}

void UIContainer::updateMouseFocusIndex(UIContainer* container, const MouseState& state) {
	Array<std::unique_ptr<UIBox>>& children = container->children;

	const Vec2f& position = state.position;

	// Check if the new position is inside the container.
	// If it's not, there should be no new mouse focus child.
	bool insideContainer = (
		position[0] >= 0.0f && position[0] < container->size[0] &&
		position[1] >= 0.0f && position[1] < container->size[1]
	);
	auto containerIsInside = container->type->isInside;
	if (insideContainer && containerIsInside != nullptr &&
		(containerIsInside != UIContainer::isInside || !container->type->consumesMouse)
	) {
		insideContainer = (*containerIsInside)(container, position);
	}

	size_t childIndex = INVALID_INDEX;
	if (insideContainer) {
		childIndex = positionToChildIndex(position, children);
	}

	// If the mouse focus child index hasn't changed, there's nothing more to do.
	if (childIndex == container->mouseFocusIndex) {
		return;
	}

	// If there was a previous mouse focus child, exit it.
	if (container->mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container->mouseFocusIndex];
		auto childOnMouseExit = child.type->onMouseExit;
		if (childOnMouseExit != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseExit)(&child, childMouseState);
		}
	}
	container->mouseFocusIndex = childIndex;

	// If there is a new mouse focus child, enter it.
	if (childIndex != INVALID_INDEX) {
		UIBox& child = *children[childIndex];
		auto childOnMouseEnter = child.type->onMouseEnter;
		if (childOnMouseEnter != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseEnter)(&child, childMouseState);
		}
	}

	// NOTE: The caller will handle exiting container if !insideContainer.
}

bool UIContainer::isInside(UIBox* box, const Vec2f& position) {
	assert(box->type != nullptr);
	assert(box->type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(box);

	if (container->type->consumesMouse) {
		// Always inside if consumesMouse.
		return true;
	}

	// Otherwise, inside if inside any child box.
	// NOTE: Order doesn't matter
	Array<std::unique_ptr<UIBox>>& children = container->children;
	size_t index = positionToChildIndex(position, children);

	return (index != INVALID_INDEX);
}

void UIContainer::onMouseEnter(UIBox* box, const MouseState& state) {
	assert(box->type != nullptr);
	assert(box->type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(box);

	Array<std::unique_ptr<UIBox>>& children = container->children;

	// No child should have mouse focus in a container that hasn't been entered yet.
	assert(container->mouseFocusIndex == INVALID_INDEX);

	size_t index = positionToChildIndex(state.position, children);
	container->mouseFocusIndex = index;

	if (index != INVALID_INDEX) {
		UIBox& child = *children[index];
		if (child.type->onMouseEnter != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*child.type->onMouseEnter)(&child, childMouseState);
		}
	}
}

void UIContainer::onMouseExit(UIBox* box, const MouseState& state) {
	assert(box->type != nullptr);
	assert(box->type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(box);

	Array<std::unique_ptr<UIBox>>& children = container->children;

	// If this is ocurring due to onMouseMove, onMouseExit should have
	// already been called on any mouse focus child, and mouseFocusIndex
	// should have been cleared, but just in case, we recurse here.
	//
	// That said, there may be cases where a component is entered in a
	// region that's blocked by a higher level component, with the
	// way updateMouseFocusIndex currently handles it.
	// TODO: This may be worth changing, to avoid entering and immediately exiting.
	assert(container->mouseFocusIndex == INVALID_INDEX);

	if (container->mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container->mouseFocusIndex];
		auto childOnMouseExit = child.type->onMouseExit;
		if (childOnMouseExit != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseExit)(&child, childMouseState);
		}
		container->mouseFocusIndex = INVALID_INDEX;
	}
}

void UIContainer::onMouseMove(UIBox* box, const Vec2f& change, const MouseState& state) {
	assert(box->type != nullptr);
	assert(box->type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(box);

	Array<std::unique_ptr<UIBox>>& children = container->children;

	// Recurse on the mouseFocusIndex child before checking whether the
	// new position is in a different child or not.
	if (container->mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container->mouseFocusIndex];
		auto childOnMouseMove = child.type->onMouseMove;
		if (childOnMouseMove != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseMove)(&child, change, childMouseState);
		}
	}

	// If any buttons are down, the mouse focus stays unchanged.
	if (state.buttonsDown != 0) {
		return;
	}

	updateMouseFocusIndex(container, state);

	// NOTE: The caller will handle exiting container if !insideContainer.
}

void UIContainer::onMouseDown(UIBox* box, size_t button, const MouseState& state) {
	assert(box->type != nullptr);
	assert(box->type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(box);

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
			(*childOnMouseDown)(&child, button, childMouseState);
		}
		break;
	}

	// Pressing a mouse button down never changes mouse focus, so that's all.
}

void UIContainer::onMouseUp(UIBox* box,size_t button,const MouseState& state) {
	assert(box->type != nullptr);
	assert(box->type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(box);

	Array<std::unique_ptr<UIBox>>& children = container->children;

	// Recurse on the mouseFocusIndex child before checking whether the
	// position is in a different child or not.
	if (container->mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container->mouseFocusIndex];
		auto childOnMouseUp = child.type->onMouseUp;
		if (childOnMouseUp != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseUp)(&child, button, childMouseState);
		}
	}

	// If any buttons are down, the mouse focus stays unchanged.
	if (state.buttonsDown != 0) {
		return;
	}

	updateMouseFocusIndex(container, state);

	// NOTE: The caller will handle exiting container if !insideContainer.
}

void UIContainer::onMouseScroll(UIBox* box, float scrollAmount, const MouseState& state) {
	assert(box->type != nullptr);
	assert(box->type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(box);

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
			(*childOnMouseScroll)(&child, scrollAmount, childMouseState);
		}
		break;
	}

	// Scrolling the mouse wheel never changes mouse focus, so that's all.
}

} // namespace UICommon
OUTER_NAMESPACE_END