#include "UICommon.h"
#include "model/LineArray.h"

#include <Array.h>
#include <ArrayDef.h>
#include <Span.h>
#include <Types.h>
#include <text/TextFunctions.h>

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

void LineArray::getText(const Position& begin, const Position& end, Array<char>& text) const {
	if (end.line == begin.line) {
		// Single line, possibly partial
		if (end.col > begin.col) {
			const Array<char>& line = lines[begin.line];
			text.append(line.data()+begin.col, line.data()+end.col);
		}
	}
	else if (end.line > begin.line) {
		// Multiple lines
		size_t linei = begin.line;
		if (begin.col > 0) {
			// First partial line
			const Array<char>& line = lines[begin.line];
			text.append(line.data()+begin.col, line.end());
			text.append('\n');
			++linei;
		}
		// Middle full lines
		for (; linei < end.line; ++linei) {
			const Array<char>& line = lines[begin.line];
			text.append(line.begin(), line.end());
			text.append('\n');
		}
		if (end.col > 0) {
			// Final partial line
			const Array<char>& line = lines[end.line];
			text.append(line.begin(), line.data()+end.col);
		}
	}
}

void LineArray::replace(
	const Position& begin,
	const Position& end,
	const char* newTextBegin,
	const char* newTextEnd,
	TextReplacementEvent* undoEvent
) {
	if (undoEvent != nullptr) {
		// Save the previous text before replacing it, so that it can be undone.
		undoEvent->lineArray = this;
		undoEvent->previousText.setSize(0);
		getText(begin, end, undoEvent->previousText);
		undoEvent->begin = begin;
		// End must be set below.
	}

	Array<char>* currentLine = &lines[begin.line];

	// Split new text into lines, keeping empty lines.
	BufArray<Span<size_t>, 8> newTextLines;
	text::splitLines(newTextBegin, newTextEnd, newTextLines, false);
	assert(newTextLines.size() >= 1);
	assert(newTextLines[0][0] == 0 && newTextLines.last()[1] == (newTextEnd - newTextBegin));

	const size_t numOriginalLines = end.line - begin.line + 1;
	const size_t numNewLines = newTextLines.size();

	if (numNewLines == 1) {
		// Single line in new text, which is a very common case, e.g. for a single letter.
		if (numOriginalLines == 1) {
			// Editing a single line, so can just use replaceSingleHelper.
			const size_t newEndCol = replaceSingleHelper(lines[begin.line], begin.col, end.col, newTextBegin, newTextEnd);
			if (undoEvent != nullptr) {
				undoEvent->end = Position{end.line, newEndCol};
			}
			return;
		}

		// Don't need to keep the end of currentLine, so can just truncate and append.
		assert(begin.col <= currentLine->size());
		currentLine->setSize(begin.col);
		currentLine->append(newTextBegin, newTextEnd);
		if (undoEvent != nullptr) {
			const size_t newEndCol = begin.col + (newTextEnd - newTextBegin);
			undoEvent->end = Position{begin.line, newEndCol};
		}

		// Append the end of the line at the end of the caret, if it's not empty.
		Array<char>& caretEndLine = lines[end.line];
		if (end.col < caretEndLine.size()) {
			currentLine->append(caretEndLine.data()+end.col, caretEndLine.end());
		}

		// Remove lines after begin.line, up to and including end.line
		size_t desti = begin.line + 1;
		for (size_t srci = end.line, numLines = lines.size(); srci < numLines; ++srci, ++desti) {
			lines[desti] = std::move(lines[srci]);
		}
		lines.setSize(desti);
		return;
	}

	// Multiple lines in new text.

	Array<char> endOfCaretEndLine;
	Array<char>* caretEndLine = &lines[end.line];
	if (end.col < caretEndLine->size()) {
		// Need to save the last part of the line, since it may be overwritten.
		endOfCaretEndLine.append(caretEndLine->data()+end.col, caretEndLine->end());
	}

	// Already save the end of currentLine if needed, so can just truncate and append.
	assert(begin.col <= currentLine->size());
	currentLine->setSize(begin.col);
	currentLine->append(newTextBegin, newTextBegin + newTextLines[0][1]);

	// Shift lines if needed.
	if (numOriginalLines > numNewLines) {
		size_t desti = begin.line + numNewLines;
		for (size_t srci = end.line + 1, numLines = lines.size(); srci < numLines; ++srci, ++desti) {
			lines[desti] = std::move(lines[srci]);
		}
		lines.setSize(desti);
	}
	else if (numOriginalLines < numNewLines) {
		const size_t oldSize = lines.size();
		const size_t sizeDiff = (numNewLines - numOriginalLines);
		const size_t newSize = oldSize + sizeDiff;
		lines.setSize(newSize);

		for (size_t srci = oldSize-1, desti = newSize-1; srci >= end.line + 1; --srci, --desti) {
			lines[desti] = std::move(lines[srci]);
		}
	}

	// Deal with full lines in middle
	currentLine = &lines[begin.line + 1];
	for (size_t newTexti = 1; newTexti < numNewLines-1; ++newTexti, ++currentLine) {
		currentLine->setSize(0);
		currentLine->append(newTextBegin + newTextLines[newTexti][0], newTextBegin + newTextLines[newTexti][1]);
	}

	// Handle partial last line
	currentLine->setSize(0);
	currentLine->append(newTextBegin + newTextLines.last()[0], newTextBegin + newTextLines.last()[1]);
	if (endOfCaretEndLine.size() != 0) {
		currentLine->append(endOfCaretEndLine.begin(), endOfCaretEndLine.end());
	}

	if (undoEvent != nullptr) {
		const size_t newEndCol = newTextLines.last()[1];
		undoEvent->end = Position{begin.line + numNewLines-1, newEndCol};
	}
}

size_t LineArray::replaceSingleHelper(
	Array<char>& line,
	size_t beginCol,
	size_t endCol,
	const char* beginText,
	const char* endText
) {
	assert(beginCol <= endCol);
	assert(endCol <= line.size());
	assert(beginText <= endText);
	const size_t newTextLength = endText-beginText;
	const size_t oldTextLength = endCol-beginCol;
	if (newTextLength > oldTextLength) {
		const size_t diff = newTextLength - oldTextLength;
		const size_t oldSize = line.size();
		line.setSize(oldSize + diff);
		// Shift text forward.
		for (size_t i = oldSize; i > endCol; ) {
			--i;
			line[i + diff] = line[i];
		}
	}
	else if (oldTextLength > newTextLength) {
		const size_t diff = oldTextLength - newTextLength;
		const size_t oldSize = line.size();
		// Shift text backward.
		for (size_t i = endCol; i < oldSize; ++i) {
			line[i - diff] = line[i];
		}
		line.setSize(oldSize - diff);
	}

	// Copy text into place.
	size_t i = beginCol;
	for (const char* text = beginText; text != endText; ++text, ++i) {
		line[i] = *text;
	}

	// Return index at end of new text.
	return i;
}

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
