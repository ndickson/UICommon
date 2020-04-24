#include "UIBox.h"

OUTER_NAMESPACE_BEGIN
namespace UICommon {

bool UIContainer::isInside(UIBox* box, const Vec2f& position) {
	assert(box->type != nullptr);
	assert(box->type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(box);

	if (container->type->consumesMouse) {
		// Always inside if consumesMouse.
		return true;
	}

	Array<std::unique_ptr<UIBox>>& children = container->children;
	for (size_t i = 0, n = children.size(); i < n; ++i) {
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
				return true;
			}
		}
	}

	return false;
}

void UIContainer::onMouseEnter(UIBox* box, const MouseState& state) {
	// FIXME: Implement this!!!

}

void UIContainer::onMouseExit(UIBox* box, const MouseState& state) {
	// FIXME: Implement this!!!

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

	const Vec2f& newPos = state.position;

	// Check if the new position is inside the container.
	// If it's not, there should be no new mouse focus child.
	bool insideContainer = (
		newPos[0] >= 0.0f && newPos[0] < container->size[0] &&
		newPos[1] >= 0.0f && newPos[1] < container->size[1]
	);
	auto containerIsInside = container->type->isInside;
	if (insideContainer && containerIsInside != nullptr) {
		insideContainer = (*containerIsInside)(container, newPos);
	}

	size_t childIndex = INVALID_INDEX;
	if (insideContainer) {
		// Check if any child boxes contain the new mouse position, in reverse order,
		// since it's the topmost-drawn first order.
		for (size_t i = children.size(); i > 0; ) {
			--i;
			UIBox& child = *children[i];
			const Vec2f& c0 = child.origin;
			const Vec2f& size = child.size;
			if (newPos[0] >= c0[0] && newPos[0]-c0[0] < size[0] &&
				newPos[1] >= c0[1] && newPos[1]-c0[1] < size[1]
			) {
				bool inside = child.type->consumesMouse;
				auto childIsInside = child.type->isInside;
				if (childIsInside != nullptr) {
					inside = (*childIsInside)(&child, newPos-c0);
				}
				if (inside) {
					childIndex = i;
					break;
				}
			}
		}
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

void UIContainer::onMouseDown(UIBox* box, size_t button, const MouseState& state) {
	assert(box->type != nullptr);
	assert(box->type->isContainer);
	UIContainer* container = static_cast<UIContainer*>(box);

	Array<std::unique_ptr<UIBox>>& children = container->children;

	// NOTE: No need to set keyboard focus here, because UIBox
	// subclasses that actually want keyboard focus will request it.

	// Recurse on the mouseFocusIndex child before checking whether the
	// new position is in a different child or not.
	while (container->mouseFocusIndex != INVALID_INDEX) {
		UIBox& child = *children[container->mouseFocusIndex];
		auto childOnMouseDown = child.type->onMouseDown;
		// Avoid actual recursion if child just has regular container behaviour.
		if (childOnMouseDown == UIContainer::onMouseDown) {
			assert(child.type->isContainer);
			container = static_cast<UIContainer*>(&child);
			continue;
		}
		if (childOnMouseDown != nullptr) {
			// Transform state into the space of the child box.
			MouseState childMouseState(state);
			childMouseState.position -= child.origin;
			(*childOnMouseDown)(&child, button, childMouseState);
		}
		break;
	}

	// Pressing a mouse button down never changes mouse focus, so that's all.
}

void UIContainer::onMouseUp(UIBox* box,size_t button,const MouseState& state) {
	// FIXME: Implement this!!!

}

void UIContainer::onMouseScroll(UIBox* box, float scrollAmount, const MouseState& state) {
	// FIXME: Implement this!!!

}

} // namespace UICommon
OUTER_NAMESPACE_END
