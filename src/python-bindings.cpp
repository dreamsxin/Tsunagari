/***************************************
** Tsunagari Tile Engine              **
** python-bindings.cpp                **
** Copyright 2011-2013 PariahSoft LLC **
***************************************/

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

#include <boost/python/module.hpp>

#include "area.h"
#include "entity.h"
#include "log.h"
#include "music.h"
#include "random.h"
#include "reader.h"
#include "scriptinst.h"
#include "sound.h"
#include "tile.h"
#include "timer.h"
#include "timeout.h"
#include "vec.h"
#include "world.h"

BOOST_PYTHON_MODULE(tsunagari)
{
	exportArea();
	exportEntity();
	exportLog();
	exportMusic();
	exportRandom();
	exportReader();
	exportScriptInst();
	exportSound();
	exportTile();
	exportTimeout();
	exportTimer();
	exportVecs();
	exportWorld();
}

// *sigh* If only it weren't static.
void pythonInitBindings()
{
	inittsunagari();
}

