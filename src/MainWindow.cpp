// This file contains SDL windowing code, isolating it from callers,
// so that they don't need to link against SDL directly.

#include "MainWindow.h"
#include "Canvas.h"
#include "UIBox.h"

#include <SDL.h>
#include <Array.h>
#include <ArrayDef.h>
#include <bmp/sRGB.h>
#include <Types.h>

#include <emmintrin.h> // For _mm_pause

OUTER_NAMESPACE_BEGIN
UICOMMON_LIBRARY_NAMESPACE_BEGIN

static Array<SDL_Rect> monitorBounds;
static SDL_Window* mainWindow;
static Canvas mainCanvas;
static MainWindow* mainWindowContainer;
static Array<SDL_Window*> otherWindows;

static Array<UIExitListener*> exitListeners;

static SDL_TimerID uiTimerID;
static volatile bool isInsideTimerCallback;

static volatile bool isExiting = false;

static SDL_Thread* drawThread;
static SDL_cond* drawThreadCond;
static SDL_mutex* drawThreadCondLock;

static uint64 lastDrawUIModCount = 0;
// TODO: Make this atomic for increment, though reading can be relaxed.
static uint64 uiStateModCount = 1;

constexpr static Vec4f defaultBackgroundColour(0.5f,0.5f,0.5f,1.0f);

UIBox* MainWindow::construct() {
	return new MainWindow();
}

void MainWindow::destruct(UIBox* box) {
	// This currently only calls the superclass destruct function,
	// but might do more in the future.
	UIContainer::staticType.destruct(box);
}

UIContainerClass MainWindow::initClass() {
	UIContainerClass c(UIContainer::initClass());
	c.typeName = "MainWindow";
	c.construct = &construct;
	c.destruct = &destruct;
	return c;
}

const UIContainerClass MainWindow::staticType(MainWindow::initClass());

static void convertToSRGB(const Vec4f* inputColours, uint8* outputData, size_t bytesPerPixel, size_t width, size_t height) {
	// FIXME: Parallelize this!!!
	if (bytesPerPixel == 4) {
		uint32* outputColours = reinterpret_cast<uint32*>(outputData);
		outputColours += width*(height-1);
		for (size_t y = 0; y < height; ++y) {
			for (size_t x = 0; x < width; ++x) {
				// FIXME: Use a fast approximation, instead of the exact calculation!!!
				uint32 outputColour = bmp::linearToSRGB(*inputColours);
				++inputColours;
				*outputColours = outputColour;
				++outputColours;
			}
			outputColours -= 2*width;
		}
	}
	else {
		outputData += 3*width*(height-1);
		for (size_t y = 0; y < height; ++y) {
			for (size_t x = 0; x < width; ++x) {
				// FIXME: Use a fast approximation, instead of the exact calculation!!!
				uint32 outputColour = bmp::linearToSRGB(*inputColours);
				++inputColours;
				outputData[0] = uint8(outputColour);
				outputData[1] = uint8(outputColour >> 8);
				outputData[2] = uint8(outputColour >> 16);
				outputData += 3;
			}
			outputData -= 6*width;
		}
	}
}

static int drawThreadFunction(void* data) {
	// SDL_CondWait needs a lock that is locked, so we lock.
	// We need to acquire uiStateLock anyway to access uiState.
	SDL_LockMutex(drawThreadCondLock);
	// Wait for a signal that there's drawing to be done.
	SDL_CondWait(drawThreadCond, drawThreadCondLock);

	while (!isExiting) {
		const uint64 uiStateModCountCopy = uiStateModCount;
		SDL_UnlockMutex(drawThreadCondLock);

		// Draw to screen buffer.
		// We reacquire screen each time, just in case window resized and screen was replaced.
		SDL_Surface* screen = SDL_GetWindowSurface(mainWindow);

		//printf("Drawing at %d\n", SDL_GetTicks());
		if (mainCanvas.image.size()[0] != screen->w || mainCanvas.image.size()[1] != screen->h) {
			// FIXME: The mainWindowContainer should have already been resized and had any necessary layout changes done!!!
			mainWindowContainer->size = Vec2f(screen->w, screen->h);
			mainCanvas.image.setSize(screen->w, screen->h);
		}

		Box2f bounds(Vec2f(0,0), mainWindowContainer->size);
		MainWindow::staticType.draw(*mainWindowContainer, bounds, bounds, mainCanvas);

		SDL_LockSurface(screen);

		Uint8 bytesPerPixel = screen->format->BytesPerPixel;
		if (bytesPerPixel != 3 && bytesPerPixel != 4) {
			// Only 3 and 4 bytes supported
			return 0;
		}

		convertToSRGB(mainCanvas.image.pixels(), (uint8*)(screen->pixels), bytesPerPixel, size_t(screen->w), size_t(screen->h));

		SDL_UnlockSurface(screen);

		lastDrawUIModCount = uiStateModCountCopy;

		// Swap screen buffer contents with window buffer.
		SDL_UpdateWindowSurface(mainWindow);

		// FIXME: Handle exiting in a more robust way without the race conditions!!!
		if (isExiting) {
			return 0;
		}

		SDL_LockMutex(drawThreadCondLock);
		// Wait for a signal that there's drawing to be done.
		SDL_CondWait(drawThreadCond, drawThreadCondLock);
	}

	SDL_UnlockMutex(drawThreadCondLock);

	return 0;
}

static Uint32 uiTimerCallbackFunction(Uint32 interval, void* data) {
	isInsideTimerCallback = true;

	if (uiTimerID == -1) {
		isInsideTimerCallback = false;
		return 0;
	}

	// NOTE: It didn't work to push an event for the timer, because SDL_WaitEvent
	// doesn't respond to manually pushed events.

	if (uiStateModCount != lastDrawUIModCount) {
		//printf("Timer at %d\n", SDL_GetTicks());

		SDL_CondSignal(drawThreadCond);
	}

	isInsideTimerCallback = false;
	return interval;
}

class UITimerExitListener : public UIExitListener {
	virtual void uiExiting() {
		SDL_RemoveTimer(uiTimerID);
		uiTimerID = -1;
		SDL_Delay(1);
		while (isInsideTimerCallback) {
			// This is a spin-wait loop.
			SDL_Delay(1);
		}
	}
};

static UITimerExitListener uiTimerExitListener;

MainWindow* UIInit(
	int monitorNum,
	bool fullscreen,
	bool blankOtherMonitors,
	int horizontalResolution,
	bool exclusiveMouseMode
) {
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error initializing SDL!  Error message: \"%s\"\n", SDL_GetError());
		return nullptr;
	}

	int numMonitors = SDL_GetNumVideoDisplays();
	if (numMonitors < 1) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error getting the number of monitors!  Error message: \"%s\"\n", SDL_GetError());
		return nullptr;
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
			return nullptr;
		}
	}
	else {
		monitorBounds.setCapacity(numMonitors);
		for (int i = 0; i < numMonitors; ++i) {
			SDL_Rect bounds;
			if (SDL_GetDisplayBounds(i, &bounds) < 0) {
				SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error getting the bounds of monitor %d!  Error message: \"%s\"\n", i, SDL_GetError());
				return nullptr;
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
			return nullptr;
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
			return nullptr;
		}
	}
	SDL_Surface* screen = SDL_GetWindowSurface(mainWindow);
	if (screen == nullptr) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error getting the main window surface buffer!  Error message: \"%s\"\n", SDL_GetError());
		return nullptr;
	}
	if (screen->format->BytesPerPixel != 3 && screen->format->BytesPerPixel != 4) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error: main window surface buffer has unsupported %d bytes per pixel!  Error message: \"%s\"\n", int(screen->format->BytesPerPixel), SDL_GetError());
		return nullptr;
	}

	if (exclusiveMouseMode) {
		// Try to set mouse mode such that the mouse position doesn't move,
		// but still reports the motion, so that rotation isn't limited
		// by the window size.
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}

	drawThreadCondLock = SDL_CreateMutex();
	if (drawThreadCondLock == nullptr) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error creating the draw thread lock!  Error message: \"%s\"\n", SDL_GetError());
		return nullptr;
	}
	drawThreadCond = SDL_CreateCond();
	if (drawThreadCond == nullptr) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error creating the drawing thread condition variable!  Error message: \"%s\"\n", SDL_GetError());
		return nullptr;
	}
	drawThread = SDL_CreateThread(drawThreadFunction, "Draw Thread", nullptr);
	if (drawThread == nullptr) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Error creating the drawing thread!  Error message: \"%s\"\n", SDL_GetError());
		return nullptr;
	}

	mainWindowContainer = new MainWindow();
	mainWindowContainer->origin = Vec2f(mainWindowBounds.x, mainWindowBounds.y);
	mainWindowContainer->size = Vec2f(mainWindowBounds.w, mainWindowBounds.h);
	mainWindowContainer->backgroundColour = defaultBackgroundColour;

	mainCanvas.image.setSize(mainWindowBounds.w, mainWindowBounds.h);

	uiTimerID = SDL_AddTimer(30u, uiTimerCallbackFunction, nullptr);

	exitListeners.append(&uiTimerExitListener);

	return mainWindowContainer;
}

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
				isExiting = true;
				for (UIExitListener* listener : exitListeners) {
					listener->uiExiting();
				}
				exitListeners.setCapacity(0);
				if (mainWindowContainer != nullptr) {
					MainWindow::staticType.destruct(mainWindowContainer);
					mainWindowContainer = nullptr;
				}
				break;
			}
			case SDL_KEYDOWN: {
				SDL_Keycode key = event.key.keysym.sym;
				//SDL_GetKeyboardState()
				if (MainWindow::staticType.onKeyDown != nullptr) {
					MainWindow::staticType.onKeyDown(*mainWindowContainer, key, keyState);
				}
			}
			case SDL_KEYUP: {
				SDL_Keycode key = event.key.keysym.sym;
				//SDL_GetKeyboardState()
				if (MainWindow::staticType.onKeyUp != nullptr) {
					MainWindow::staticType.onKeyUp(*mainWindowContainer, key, keyState);
				}
				break;
			}
			case SDL_MOUSEMOTION: {
				// TODO: Handle entry and exit from main window, if mouse isn't in exclusive mode.
				const Vec2f change(event.motion.xrel, -event.motion.yrel);
				// FIXME: Consider applying translation by (0.5,0.5), since that's the middle of the pixel.
				const MouseState mouseState{Vec2f(float(event.motion.x), mainWindowContainer->size[1] - event.motion.y - 1), event.motion.state};
				mouseButtonState = event.motion.state;
				MainWindow::staticType.onMouseMove(*mainWindowContainer, change, mouseState);
				break;
			}
			case SDL_MOUSEBUTTONDOWN: {
				// FIXME: Consider applying translation by (0.5,0.5), since that's the middle of the pixel.
				const MouseState mouseState{Vec2f(float(event.button.x), mainWindowContainer->size[1] - event.button.y - 1), event.button.state};
				mouseButtonState = event.motion.state;
				MainWindow::staticType.onMouseDown(*mainWindowContainer, event.button.button, mouseState);
				break;
			}
			case SDL_MOUSEBUTTONUP: {
				// FIXME: Consider applying translation by (0.5,0.5), since that's the middle of the pixel.
				const MouseState mouseState{Vec2f(float(event.button.x), mainWindowContainer->size[1] - event.button.y - 1), event.button.state};
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
			case SDL_WINDOWEVENT: {
				switch (event.window.event) {
					case SDL_WINDOWEVENT_SHOWN: {
						setNeedRedraw();
						break;
					}
					case SDL_WINDOWEVENT_HIDDEN:
					case SDL_WINDOWEVENT_MINIMIZED: {
						// FIXME: Call onMouseExit for mainWindowContainer if mouse was currently inside!!!
						break;
					}
					case SDL_WINDOWEVENT_EXPOSED: {
						setNeedRedraw();
						break;
					}
					case SDL_WINDOWEVENT_RESIZED:
					case SDL_WINDOWEVENT_SIZE_CHANGED:
					case SDL_WINDOWEVENT_MAXIMIZED:
					case SDL_WINDOWEVENT_RESTORED: {
						// FIXME: Check whether the redraw has already been initiated, in case more than one of these is called!!!
						setNeedRedraw();
						break;
					}
					case SDL_WINDOWEVENT_ENTER: {
						// FIXME: Call onMouseEnter for mainWindowContainer if mouse was currently outside!!!
						// Consider debug warning if mouse was already inside?
						break;
					}
					case SDL_WINDOWEVENT_LEAVE: {
						// FIXME: Call onMouseExit for mainWindowContainer if mouse was currently inside!!!
						// Consider debug warning if mouse was already outside?
						break;
					}
					case SDL_WINDOWEVENT_CLOSE: {
						// FIXME: If this is the main window, call window close callback,
						// e.g. to check if the user wants to save the open file!!!
						// Let everything know that the program is ending.
						isExiting = true;
						for (UIExitListener* listener : exitListeners) {
							listener->uiExiting();
						}
						exitListeners.setCapacity(0);
						if (mainWindowContainer != nullptr) {
							MainWindow::staticType.destruct(mainWindowContainer);
							mainWindowContainer = nullptr;
						}
						break;
					}
					// FIXME: Add any others that apply!!!
				}

				break;
			}
		}
	}
}

void setNeedRedraw() {
	++uiStateModCount;
}

UICOMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
