/*********************************
** Tsunagari Tile Engine        **
** window.cpp                   **
** Copyright 2011-2012 OmegaSDG **
*********************************/

// **********
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to 
// deal in the Software without restriction, including without limitation the 
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
// sell copies of the Software, and to permit persons to whom the Software is 
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
// IN THE SOFTWARE.
// **********

#include <boost/foreach.hpp>
#include <Gosu/Graphics.hpp> // for Gosu::Graphics
#include <Gosu/Timing.hpp>
#include <Gosu/Utility.hpp>

#include "reader.h"
#include "world.h"
#include "window.h"

#define ASSERT(x)  if (!(x)) { return false; }

// Garbage collection called every X milliseconds
#define GC_CALL_PERIOD 10 * 1000

namespace Gosu {
	/**
	 * Enable 1980s-style graphics scaling: nearest-neighbor filtering.
	 * Call this function before creating any Gosu::Image.
	 */
	void enableUndocumentedRetrofication() {
		extern bool undocumentedRetrofication;
		undocumentedRetrofication = true;
	}
}

static GameWindow* globalWindow = NULL;

GameWindow& GameWindow::instance()
{
	return *globalWindow;
}

GameWindow::GameWindow()
	// Gosu emulates the requested screen resolution on fullscreen,
	// but this breaks our aspect ratio-correcting letterbox.
	// Ergo we just make a window the size of the screen.
	: Gosu::Window(
	    conf.fullscreen ? Gosu::screenWidth() :
	                      (unsigned)conf.windowSize.x,
	    conf.fullscreen ? Gosu::screenHeight() :
	                      (unsigned)conf.windowSize.y,
	    conf.fullscreen
	  ),
	  now(Gosu::milliseconds()),
	  lastGCtime(0)
{
	globalWindow = this;
	Gosu::enableUndocumentedRetrofication();

	setIcon(Gosu::widen("tsunagari_logo.png"));
}

GameWindow::~GameWindow()
{
}

bool GameWindow::init()
{
	world.reset(new World());
	return world->init();
}

int GameWindow::width() const
{
	return (int)graphics().width();
}

int GameWindow::height() const
{
	return (int)graphics().height();
}

void GameWindow::buttonDown(const Gosu::Button btn)
{
	now = (int)Gosu::milliseconds();
	if (btn == Gosu::kbEscape &&
			(input().down(Gosu::kbLeftShift) ||
			 input().down(Gosu::kbRightShift))) {
		exit(0);
	}
	else {
		if (keystates.find(btn) == keystates.end()) {
			keystate& state = keystates[btn];
			state.since = now;
			state.initiallyResolved = false;
			state.consecutive = false;

			// We process the initial buttonDown here so that it
			// gets handled even if we receive a buttonUp before an
			// update.
			world->buttonDown(btn);
		}
	}
}

void GameWindow::buttonUp(const Gosu::Button btn)
{
	keystates.erase(btn);
	world->buttonUp(btn);
}

void GameWindow::draw()
{
	world->draw();
}

bool GameWindow::needsRedraw() const
{
	return world->needsRedraw();
}

void GameWindow::update()
{
	now = Gosu::milliseconds();

	if (conf.moveMode == TURN)
		handleKeyboardInput(now);
	world->update(now);

	if (now > lastGCtime + GC_CALL_PERIOD) {
		lastGCtime = now;
		Reader::garbageCollect();
	}
}

time_t GameWindow::time() const
{
	return now;
}

void GameWindow::handleKeyboardInput(time_t now)
{
	std::map<Gosu::Button, keystate>::iterator it;

	// Persistent input handling code
	for (it = keystates.begin(); it != keystates.end(); it++) {
		Gosu::Button btn = it->first;
		keystate& state = it->second;

		// If there is persistCons milliseconds of latency
		// between when a button is depressed and when we first look at
		// it here, we'll incorrectly try to fire off a second round of
		// input.
		// This can happen if an intermediary function blocks the thread
		// for a while.
		if (!state.initiallyResolved) {
			state.initiallyResolved = true;
			continue;
		}

		time_t delay = state.consecutive ?
		    conf.persistCons : conf.persistInit;
		if (now >= state.since + delay) {
			state.since = now;
			world->buttonDown(btn);
			state.consecutive = true;
		}
	}
}

