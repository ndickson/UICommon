#pragma once

#include "UICommon.h"
#include <Array.h>
#include <ArrayDef.h>
#include <Types.h>

#include <memory>
#include <utility>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

struct UndoEvent;

// Class description for UndoEvent subclasses
struct UndoEventClass {
	const char* typeName = nullptr;

	UndoEvent* (*construct)() = nullptr;
	void (*destruct)(UndoEvent*) = nullptr;

	// This returns a pointer to an UndoEvent representing the inverse of the original,
	// for easy implementation of "redo".  The caller owns the returned object.
	//
	// This function can return a pointer to the original UndoEvent if it
	// is modified to represent the inverse of what it previously represented.
	// Otherwise, the original becomes owned by the returned inverse via the
	// cachedInverse member.
	std::unique_ptr<UndoEvent> (*undo)(std::unique_ptr<UndoEvent>&&) = nullptr;

	// This appends a text description of the undo event to text.
	void (*getDescription)(const UndoEvent&, Array<char>& text) = nullptr;
};

// Base class for all events that can be undone.  Any event that
// can be undone must be able to be redone after undoing.
struct UndoEvent {
	const UndoEventClass*const type;
	std::unique_ptr<UndoEvent> cachedInverse;

	~UndoEvent() {
		if (type != nullptr) {
			assert(type->destruct != nullptr);
			type->destruct(this);
			cachedInverse.reset();
		}
	}

protected:
	UndoEvent(const UndoEventClass* type_) : type(type_) {}

	// UndoEvent objects shouldn't be copied, to avoid the risk
	// of accidentally undoing the same action multiple times.
	UndoEvent(const UndoEvent&) = delete;
};

// Class representing a sequence of undo events in a single undo event,
// for batching undo events together.
struct UndoSequence : public UndoEvent {
	Array<std::unique_ptr<UndoEvent>> sequence;
	Array<char> description;

	UICOMMON_LIBRARY_EXPORTED static const UndoEventClass staticType;

	UndoSequence() : UndoEvent(&staticType) {}

	void clear() {
		sequence.setCapacity(0);
		description.setCapacity(0);
	}

	void append(std::unique_ptr<UndoEvent>&& undoEvent) {
		if (!undoEvent) {
			return;
		}
		assert(undoEvent->type != nullptr);
		// If undoEvent is a sequence, too, just append its contents,
		// instead of having nested sequences.
		if (undoEvent->type == &staticType) {
			append(std::move(static_cast<UndoSequence&>(*undoEvent)));
		}
		else {
			sequence.append(std::move(undoEvent));
		}
	}
	void append(UndoSequence&& undoSequence) {
		for (auto& undoEvent : undoSequence.sequence) {
			sequence.append(std::move(undoEvent));
		}
		undoSequence.clear();
	}

protected:
	static UndoEvent* construct() {
		return new UndoSequence();
	}
	static void destruct(UndoEvent* undoEvent) {
		// Assert that original points to an UndoSequence.
		assert(undoEvent);
		assert(undoEvent->type == staticType);
		UndoSequence* sequence = static_cast<UndoSequence*>(undoEvent);
		// Destruct the array.
		sequence->sequence.setCapacity(0);
		sequence->description.setCapacity(0);
	}

	static std::unique_ptr<UndoEvent> undo(std::unique_ptr<UndoEvent>&& original) {
		// Assert that original points to an UndoSequence.
		assert(original);
		assert(original->type == staticType);
		UndoSequence* sequence = static_cast<UndoSequence*>(original.release());
		auto& array = sequence->sequence;

		// Undo everything from the end backward, storing the inverses
		for (size_t i = array.size(); i > 0;) {
			--i;
			assert(array[i] && array[i]->type && array[i]->type->undo);
			auto* undoFunction = array[i]->type->undo;
			array[i] = undoFunction(std::move(array[i]));
		}

		// Reverse sequence, since the inverse is in the opposite order,
		// (without std::reverse, to avoid including algorithm header)
		auto* a = array.begin();
		auto* b = array.end()-1;
		for ( ; a < b; ++a, --b) {
			a->swap(*b);
		}

		// Return the same UndoEvent as the inverse.
		return std::unique_ptr<UndoEvent>(sequence);
	}

	static void getDescription(const UndoEvent& undoEvent, Array<char>& text) {
		// Assert that original points to an UndoSequence.
		assert(undoEvent);
		assert(undoEvent->type == staticType);
		const UndoSequence& sequence = static_cast<const UndoSequence&>(undoEvent);
		text.append(sequence.description.begin(), sequence.description.end());
	}
private:
	static inline UndoEventClass initClass();
};

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
