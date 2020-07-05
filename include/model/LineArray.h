#pragma once

#include "../UICommon.h"
#include "../undo/UndoEvent.h"

#include <Array.h>
#include <ArrayDef.h>
#include <Types.h>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

struct LineArrayClass {

};

class LineArray {
public:
	const LineArrayClass*const type;
protected:
	BufArray<Array<char>, 1> lines;
public:
	struct Position {
		size_t line;
		size_t col;
	};

	UICOMMON_LIBRARY_EXPORTED void getText(
		const Position& begin,
		const Position& end,
		Array<char>& text
	) const;

	// Replaces the text from begin to end with the text between
	// newTextBegin and newTextEnd, optionally filling in a
	// TextReplacementEvent to be able to undo the replacement.
	UICOMMON_LIBRARY_EXPORTED void replace(
		const Position& begin,
		const Position& end,
		const char* newTextBegin,
		const char* newTextEnd,
		TextReplacementEvent* undoEvent = nullptr
	);

	INLINE void insert(
		const Position& position,
		const char* newTextBegin,
		const char* newTextEnd,
		TextReplacementEvent* undoEvent = nullptr
	) {
		replace(position, position, newTextBegin, newTextEnd, undoEvent);
	}
	INLINE void replaceAll(
		const char* newTextBegin,
		const char* newTextEnd,
		TextReplacementEvent* undoEvent = nullptr
	) {
		replace(Position{0,0}, Position{lines.size()-1, lines.last().size()}, newTextBegin, newTextEnd, undoEvent);
	}
	INLINE void remove(
		const Position& begin,
		const Position& end,
		TextReplacementEvent* undoEvent = nullptr
	) {
		replace(begin, end, nullptr, nullptr, undoEvent);
	}
protected:
	// Helper function for replacing text within a single line
	// with text containing no line breaks.
	UICOMMON_LIBRARY_EXPORTED static size_t replaceSingleHelper(
		Array<char>& line,
		size_t beginCol,
		size_t endCol,
		const char* beginText,
		const char* endText
	);
};

struct TextReplacementEvent : public UndoEvent {
	LineArray* lineArray;
	Array<char> previousText;
	LineArray::Position begin;
	LineArray::Position end;
};

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
