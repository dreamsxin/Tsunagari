/*********************************
** Tsunagari Tile Engine        **
** viewport.cpp                 **
** Copyright 2011-2012 OmegaSDG **
*********************************/

#include <Gosu/Graphics.hpp> // for Gosu::screenWidth/Height()
#include <Gosu/Math.hpp>

#include "area.h"
#include "vec.h"
#include "viewport.h"
#include "window.h"


Viewport::Viewport(const GameWindow& window, icoord vsize)
	: window(window),
	  conf(conf),
	  off(0, 0),
	  virtRes(vsize.x, vsize.y),
	  mode(TM_MANUAL),
	  area(NULL)
{
	double width = (double)Gosu::screenWidth();
	double height = (double)Gosu::screenHeight();
	aspectRatio = width / height;
}

Viewport::~Viewport()
{
}


void Viewport::update(unsigned long)
{
	switch (mode) {
	case TM_MANUAL:
		// Do nothing.
		break;
	case TM_FOLLOW_ENTITY:
		_jumpToEntity(targete);
		break;
	};
}

rvec2 Viewport::getMapOffset() const
{
	return off;
}

rvec2 Viewport::getLetterboxOffset() const
{
	return addLetterboxOffset(rvec2(0.0, 0.0));
}

rvec2 Viewport::getScale() const
{
	rvec2 letterbox = getLetterbox();
	rvec2 physRes = rvec2(
		(double)window.width(),
		(double)window.height()
	);

	return rvec2(
		physRes.x / virtRes.x * (1 - letterbox.x),
		physRes.y / virtRes.y * (1 - letterbox.y)
	);
}

rvec2 Viewport::getPhysRes() const
{
	return rvec2(
		(double)window.width(),
		(double)window.height()
	);
}

rvec2 Viewport::getVirtRes() const
{
	return virtRes;
}

// Immediatly center render offset. Stop any tracking.
void Viewport::jumpToPt(ivec2 pt)
{
	jumpToPt(rvec2((double)pt.x, (double)pt.y));
}

void Viewport::jumpToPt(rvec2 pt)
{
	mode = TM_MANUAL;
	off = offsetForPt(pt);
}

void Viewport::jumpToEntity(const Entity* e)
{
	mode = TM_MANUAL; // API implies mode change.
	_jumpToEntity(e);
}


// Continuously follow.
void Viewport::trackEntity(const Entity* e)
{
	mode = TM_FOLLOW_ENTITY;
	targete = e;
}


void Viewport::setArea(const Area* a)
{
	area = a;
}


void Viewport::_jumpToEntity(const Entity* e)
{
	rcoord pos = e->getPixelCoord();
	ivec2 td = area->getTileDimensions();
	rvec2 center = rvec2(
		pos.x + td.x/2,
		pos.y + td.y/2
	);
	off = offsetForPt(center);
}


rvec2 Viewport::getLetterbox() const
{
	rvec2 physRes = getPhysRes();
	double physAspect = physRes.x / physRes.y;
	double virtAspect = virtRes.x / virtRes.y;

	if (physAspect > virtAspect) {
		// Letterbox cuts off left-right.
		double cut = 1 - virtAspect / physAspect;
		return rvec2(cut, 0);
	}
	else {
		// Letterbox cuts off top-bottom.
		double cut = 1 - physAspect / virtAspect;
		return rvec2(0, cut);
	}
}

rvec2 Viewport::offsetForPt(rvec2 pt) const
{
	return boundToArea(centerOn(pt));
}

rvec2 Viewport::centerOn(rvec2 pt) const
{
	return rvec2(
		pt.x - virtRes.x/2,
		pt.y - virtRes.y/2
	);
}

rvec2 Viewport::boundToArea(rvec2 pt) const
{
	icoord ad = area->getDimensions();
	ivec2 td = area->getTileDimensions();
	double areaWidth = ad.x * td.x;
	double areaHeight = ad.y * td.y;
	bool loopX = area->loopsInX();
	bool loopY = area->loopsInY();

	return rvec2(
		boundDimension(virtRes.x, areaWidth,  pt.x, loopX),
		boundDimension(virtRes.y, areaHeight, pt.y, loopY)
	);
}

double Viewport::boundDimension(double screen, double area, double pt,
                                bool loop) const
{
	// Since looping areas continue without bound, this is a no-op.
	if (loop)
		return pt;

	// If the Area is smaller than the screen, center the Area. Otherwise,
	// allow the screen to move to the edge of the Area, but not past.
	double wiggleRoom = area - screen;
	return wiggleRoom <= 0 ?
	       wiggleRoom/2 :
	       Gosu::boundBy(pt, 0.0, wiggleRoom);
}

rvec2 Viewport::addLetterboxOffset(rvec2 pt) const
{
	rvec2 physRes = getPhysRes();
	rvec2 letterbox = getLetterbox();
	rvec2 lbOff = letterbox;
	lbOff /= 2;
	lbOff *= physRes;
	pt -= lbOff;
	return pt;
}

