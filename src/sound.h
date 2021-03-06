/***************************************
** Tsunagari Tile Engine              **
** sound.h                            **
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

#ifndef SOUND_H
#define SOUND_H

#include <memory>

#include <Gosu/Audio.hpp>

class SoundInstance
{
public:
	SoundInstance(Gosu::SampleInstance inst);

	bool isPlaying();
	void stop();

	bool isPaused();
	void setPaused(bool paused);

	int getVolume();
	void setVolume(int volume);

	double getPan();
	void setPan(double pan);

	double getSpeed();
	void setSpeed(double speed);

private:
	Gosu::SampleInstance inst;
	int volume;
	double pan, speed;
};

typedef std::shared_ptr<SoundInstance> SoundInstanceRef;

class Sound
{
public:
	Sound(Gosu::Sample* source);
	SoundInstance play();

private:
	Gosu::Sample* source;
};

typedef std::shared_ptr<Sound> SampleRef;

void exportSound();

#endif

