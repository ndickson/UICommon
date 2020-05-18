// This file contains SDL windowing code, isolating it from callers,
// so that they don't need to link against SDL directly.

#include "MainWindow.h"
#include "UIBox.h"

#include <SDL.h>
#include <Array.h>
#include <ArrayDef.h>
#include <Types.h>


OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

static Array<SDL_Rect> monitorBounds;
static SDL_Window* mainWindow;
static MainWindow* mainWindowContainer;
static Array<SDL_Window*> otherWindows;

UIBox* MainWindow::construct() {
	return new MainWindow();
}

void MainWindow::destruct(UIBox* box) {
	// This currently only calls the superclass destruct function,
	// but might do more in the future.
	UIContainer::staticType.destruct(box);
}

UIContainerClass MainWindow::initClass() {
	UIContainerClass c(UIContainer::staticType);
	c.typeName = "MainWindow";
	c.construct = &construct;
	c.destruct = &destruct;
	return c;
}


bool UIInit(
	int monitorNum,
	bool fullscreen,
	bool blankOtherMonitors,
	int horizontalResolution,
	bool exclusiveMouseMode
) {
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error initializing SDL!  Error message: \"%s\"\n", SDL_GetError());
		return false;
	}

	int numMonitors = SDL_GetNumVideoDisplays();
	if (numMonitors < 1) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error getting the number of monitors!  Error message: \"%s\"\n", SDL_GetError());
		return false;
	}

	// Limit the main monitor number
	if (monitorNum < 0) {
		monitorNum = 0;
	}
	else if (monitorNum >= numMonitors) {
		monitorNum = numMonitors - 1;
	}

	if (!fullscreen) {
		blankOtherMonitors = false;
	}

	if (!blankOtherMonitors) {
		monitorBounds.setSize(1);
		if (SDL_GetDisplayBounds(monitorNum, &monitorBounds[0]) < 0) {
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error getting the bounds of monitor %d!  Error message: \"%s\"\n", monitorNum, SDL_GetError());
			return false;
		}
	}
	else {
		monitorBounds.setCapacity(numMonitors);
		for (int i = 0; i < numMonitors; ++i) {
			SDL_Rect bounds;
			if (SDL_GetDisplayBounds(i, &bounds) < 0) {
				SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error getting the bounds of monitor %d!  Error message: \"%s\"\n", i, SDL_GetError());
				return false;
			}
			monitorBounds.append(bounds);
		}
	}

	const SDL_Rect& mainBounds = monitorBounds[blankOtherMonitors ? monitorNum : 0];
	SDL_Rect mainWindowBounds;
	if (fullscreen) {
		// Create a fullscreen window on the specified monitor.
		mainWindowBounds = mainBounds;
		mainWindow = SDL_CreateWindow(
			"Main Window",
			mainBounds.x, mainBounds.y,
			mainBounds.w, mainBounds.h,
			SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);// | SDL_WINDOW_ALWAYS_ON_TOP);
		if (mainWindow == nullptr) {
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error creating the fullscreen main window on monitor %d!  Error message: \"%s\"\n", monitorNum, SDL_GetError());
			return false;
		}

		if (blankOtherMonitors && numMonitors > 1) {
			otherWindows.setSize(numMonitors-1);
			// Create fullscreen windows on all other monitors, to be blacked out.
			for (int i = 0; i < numMonitors; ++i) {
				if (i != monitorNum) {
					const SDL_Rect& bounds = monitorBounds[i];
					otherWindows[i - (i > monitorNum)] = SDL_CreateWindow(
						"Blank Window",
						bounds.x, bounds.y,
						bounds.w, bounds.h,
						SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);// | SDL_WINDOW_ALWAYS_ON_TOP);
					if (otherWindows[i - (i > monitorNum)] == nullptr) {
						SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating a secondary window on monitor %d!  Error message: \"%s\"\n", i, SDL_GetError());
					}
					// FIXME: Blank the window!!!
				}
			}
		}
	}
	else {
		// Create a non-fullscreen window on the specified monitor.
		mainWindowBounds.w = (horizontalResolution > 100) ? horizontalResolution : 800;
		if (mainWindowBounds.w > mainBounds.w) {
			mainWindowBounds.w = mainBounds.w;
		}
		mainWindowBounds.h = (mainWindowBounds.w / 16) * 9;
		if (mainWindowBounds.h > mainBounds.h) {
			mainWindowBounds.h = mainBounds.h;
		}
		mainWindowBounds.x = mainBounds.x + (mainBounds.w - mainWindowBounds.w) / 2;
		mainWindowBounds.y = mainBounds.y + (mainBounds.h - mainWindowBounds.h) / 2;
		mainWindow = SDL_CreateWindow("Main Window",
			mainWindowBounds.x, mainWindowBounds.y,
			mainWindowBounds.w, mainWindowBounds.h,
			SDL_WINDOW_SHOWN);
		if (mainWindow == nullptr) {
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error creating the main window on monitor %d!  Error message: \"%s\"\n", monitorNum, SDL_GetError());
			return false;
		}
	}
	SDL_Surface* screen = SDL_GetWindowSurface(mainWindow);
	if (screen == nullptr) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error getting the main window surface buffer!  Error message: \"%s\"\n", SDL_GetError());
		return false;
	}
	if (screen->format->BytesPerPixel != 3 && screen->format->BytesPerPixel != 4) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error: main window surface buffer has unsupported %d bytes per pixel!  Error message: \"%s\"\n", int(screen->format->BytesPerPixel), SDL_GetError());
		return false;
	}

	if (exclusiveMouseMode) {
		// Try to set mouse mode such that the mouse position doesn't move,
		// but still reports the motion, so that rotation isn't limited
		// by the window size.
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}

	mainWindowContainer = new MainWindow();
	mainWindowContainer->origin = Vec2f(mainWindowBounds.x, mainWindowBounds.y);
	mainWindowContainer->size = Vec2f(mainWindowBounds.w, mainWindowBounds.h);
}

static Array<UIExitListener*> exitListeners;

void registerUIExitListener(UIExitListener* listener) {
	exitListeners.append(listener);
}

struct KeyState {
	const uint8*const keys;
	const size_t numKeys;
};

void UILoop() {
	uint64 mouseButtonState = 0;

	int numKeys;
	const uint8* sdlKeyState = SDL_GetKeyboardState(&numKeys);
	KeyState keyState{sdlKeyState, size_t(numKeys)};

	bool isExiting = false;
	while (!isExiting) {
		SDL_Event event;
		// NOTE: SDL_WaitEvent does not wake up for events manually pushed with SDL_PushEvent.
		int eventCount = SDL_WaitEvent(&event);
		if (eventCount == 0) {
			continue;
		}
		switch (event.type) {
			case SDL_QUIT: {
				// Let everything know that the program is ending.
				for (UIExitListener* listener : exitListeners) {
					listener->uiExiting();
				}
				exitListeners.setCapacity(0);
				isExiting = true;
				break;
			}
			case SDL_KEYDOWN: {
				SDL_Keycode key = event.key.keysym.sym;
				//SDL_GetKeyboardState()
				MainWindow::staticType.onKeyDown(*mainWindowContainer, key, keyState);
			}
			case SDL_KEYUP: {
				SDL_Keycode key = event.key.keysym.sym;
				//SDL_GetKeyboardState()
				MainWindow::staticType.onKeyUp(*mainWindowContainer, key, keyState);
				break;
			}
			case SDL_MOUSEMOTION: {
				// TODO: Handle entry and exit from main window, if mouse isn't in exclusive mode.
				const Vec2f change(event.motion.xrel, event.motion.yrel);
				const MouseState mouseState{Vec2f(event.motion.x, event.motion.y), event.motion.state};
				mouseButtonState = event.motion.state;
				MainWindow::staticType.onMouseMove(*mainWindowContainer, change, mouseState);
				break;
			}
			case SDL_MOUSEBUTTONDOWN: {
				const MouseState mouseState{Vec2f(event.button.x, event.button.y), event.button.state};
				mouseButtonState = event.motion.state;
				MainWindow::staticType.onMouseDown(*mainWindowContainer, event.button.button, mouseState);
				break;
			}
			case SDL_MOUSEBUTTONUP: {
				const MouseState mouseState{Vec2f(event.button.x, event.button.y), event.button.state};
				mouseButtonState = event.motion.state;
				MainWindow::staticType.onMouseUp(*mainWindowContainer, event.button.button, mouseState);
				break;
			}
			case SDL_MOUSEWHEEL: {
				float amount = event.wheel.y / 120.0f;
				if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
					amount = -amount;
				}
				const MouseState mouseState{Vec2f(event.wheel.x, event.wheel.y), mouseButtonState};
				MainWindow::staticType.onMouseScroll(*mainWindowContainer, amount, mouseState);
				break;
			}
		}
	}
}

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
