#include "undo/UndoEvent.h"

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

const UndoEventClass UndoSequence::staticType(UndoSequence::initClass());

UndoEventClass UndoSequence::initClass() {
	UndoEventClass c;
	c.typeName = "UndoSequence";
	c.construct = &construct;
	c.destruct = &destruct;
	c.undo = &undo;
	c.getDescription = &getDescription;
	return c;
}

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
