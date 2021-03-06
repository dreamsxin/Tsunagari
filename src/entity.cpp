/***************************************
** Tsunagari Tile Engine              **
** entity.cpp                         **
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

#include <math.h>

#include <Gosu/Image.hpp>
#include <Gosu/Math.hpp>
#include <Gosu/Timing.hpp>

#include "area.h"
#include "client-conf.h"
#include "entity.h"
#include "log.h"
#include "python.h"
#include "python-bindings-template.cpp"
#include "reader.h"
#include "string.h"
#include "world.h"
#include "xml.h"

#define ASSERT(x)  if (!(x)) { return false; }

static std::string directions[][3] = {
	{"up-left",   "up",     "up-right"},
	{"left",      "stance", "right"},
	{"down-left", "down",   "down-right"},
};


Entity::Entity()
	: redraw(true),
	  area(NULL),
	  r(0.0, 0.0, 0.0),
	  frozen(false),
	  speedMul(1.0),
	  moving(false),
	  stillMoving(false),
	  nowalkFlags(TILE_NOWALK | TILE_NOWALK_NPC),
	  nowalkExempt(0),
	  phase(NULL),
	  phaseName("")
{
}

Entity::~Entity()
{
	if (deleteScript) {
		pythonSetGlobal("Area", area);
		pythonSetGlobal("Entity", this);
		deleteScript->invoke();
	}
}

bool Entity::init(const std::string& descriptor)
{
	this->descriptor = descriptor;
	return processDescriptor();
}

void Entity::destroy()
{
	leaveTile();
	if (area) {
		erase();
		area->requestRedraw();
	}
	area = NULL;
	delete this;
}

void Entity::draw()
{
	redraw = false;
	if (!phase)
		return;

	time_t now = World::instance()->time();
	Image* img = phase->frame(now);

	img->draw(
		doff.x + r.x,
		doff.y + r.y,
		r.z + area->isometricZOff(rvec2(r.x, r.y))
	);
}

bool Entity::needsRedraw() const
{
	time_t now = World::instance()->time();
	return redraw || (phase && phase->needsRedraw(now));
}


void Entity::tick(time_t dt)
{
	runTickScript();
	switch (conf.moveMode) {
	case TURN:
		tickTurn(dt);
		break;
	case TILE:
		tickTile(dt);
		break;
	case NOTILE:
		tickNoTile(dt);
		break;
	}
}

void Entity::tickTurn(time_t)
{
	// FIXME Characters (!!) don't do anything in TILE mode.
}

void Entity::tickTile(time_t dt)
{
	if (!moving)
		return;

	redraw = true;
	double traveled = speed * (double)dt;
	double destDist = Gosu::distance(r.x, r.y, destCoord.x, destCoord.y);
	if (destDist <= traveled) {
		r = destCoord;
		moving = false;
		postMove();
		if (moving) {
			// Time rollover.
			double perc = 1.0 - destDist/traveled;
			time_t remt = (time_t)(perc * (double)dt);
			tick(remt);
		}
	}
	else {
		double angle = atan2(destCoord.y - r.y, destCoord.x - r.x);
		double dx = cos(angle);
		double dy = sin(angle);

		r.x += dx * traveled;
		r.y += dy * traveled;
	}
}

void Entity::tickNoTile(time_t)
{
	// TODO
}

void Entity::turn()
{
	runTurnScript();
}

const std::string Entity::getFacing() const
{
	return directionStr(facing);
}

bool Entity::setPhase(const std::string& name)
{
	enum SetPhaseResult res;
	res = _setPhase(name);
	if (res == PHASE_NOTFOUND) {
		res = _setPhase("stance");
		if (res == PHASE_NOTFOUND)
			Log::err(descriptor, "phase '" + name + "' not found");
	}
	return res == PHASE_CHANGED;
}

std::string Entity::getPhase() const
{
	return phaseName;
}

rcoord Entity::getPixelCoord() const
{
	return r;
}

icoord Entity::getTileCoords_i() const
{
	return area->virt2phys(r);
}

vicoord Entity::getTileCoords_vi() const
{
	return area->virt2virt(r);
}

void Entity::setTileCoords(int x, int y)
{
	leaveTile();
	vicoord virt(x, y, r.z);
	redraw = true;
	r = area->virt2virt(virt);
	enterTile();
}

void Entity::setTileCoords(int x, int y, double z)
{
	leaveTile();
	vicoord virt(x, y, z);
	redraw = true;
	r = area->virt2virt(virt);
	enterTile();
}

void Entity::setTileCoords(icoord phys)
{
	leaveTile();
	redraw = true;
	r = area->phys2virt_r(phys);
	enterTile();
}

void Entity::setTileCoords(vicoord virt)
{
	leaveTile();
	redraw = true;
	r = area->virt2virt(virt);
	enterTile();
}

void Entity::setTileCoords(rcoord virt)
{
	leaveTile();
	redraw = true;
	r = virt;
	enterTile();
}

void Entity::teleport(int, int)
{
	throw "pure virtual function";
}

icoord Entity::moveDest(ivec2 facing)
{
	Tile* tile = getTile();
	icoord here = getTileCoords_i();

	if (tile)
		// Handle layermod.
		return tile->moveDest(here, facing);
	else
		return here + icoord(facing.x, facing.y, 0);
}

// Python API.
vicoord Entity::moveDest(Tile* t, int dx, int dy)
{
	icoord here = getTileCoords_i();
	icoord dest;

	if (t) {
		// Handle layermod.
		dest = t->moveDest(here, ivec2(dx, dy));
		return t->area->phys2virt_vi(dest);
	}
	else {
		dest = here + icoord(dx, dy, 0);
		return area->phys2virt_vi(dest);
	}
}

// Python API.
bool Entity::canMove(int x, int y, double z)
{
	vicoord virt(x, y, z);
	return canMove(area->virt2phys(virt));
}

bool Entity::canMove(icoord dest)
{
	icoord dxyz = dest - getTileCoords_i();
	ivec2 dxy(dxyz.x, dxyz.y);

	Tile* curTile = getTile();
	this->destTile = area->getTile(dest);
	this->destCoord = area->phys2virt_r(dest);

	if ((curTile && curTile->exitAt(dxy)) ||
	    (destTile && destTile->exits[EXIT_NORMAL])) {
		// We can always take exits as long as we can take exits.
		// (Even if they would cause us to be out of bounds.)
		if (nowalkExempt & TILE_NOWALK_EXIT)
			return true;
	}

	bool inBounds = area->inBounds(dest);
	if (destTile && inBounds) {
		// Tile is inside map. Can we move?
		if (nowalked(*destTile))
			return false;
		if (destTile->entCnt)
			// Space is occupied by another Entity.
			return false;

		return true;
	}

	// The tile is legitimately off the map.
	return nowalkExempt & TILE_NOWALK_AREA_BOUND;
}

bool Entity::canMove(vicoord dest)
{
	return canMove(area->virt2phys(dest));
}

bool Entity::isMoving() const
{
	return moving || stillMoving;
}

void Entity::moveByTile(int x, int y)
{
	moveByTile(ivec2(x, y));
}

void Entity::moveByTile(ivec2 delta)
{
	if (moving)
		return;
	setFacing(delta);

	if (canMove(moveDest(facing)))
		preMove();
	else
		setPhase(directionStr(facing));
}

void Entity::move(int, int)
{
	throw "pure virtual function";
}

Area* Entity::getArea()
{
	return area;
}

void Entity::setArea(Area* a)
{
	leaveTile();
	area = a;
	calcDraw();
	setSpeed(speedMul); // Calculate new speed based on tile size.
	enterTile();
}

double Entity::getSpeed() const
{
	return speedMul;
}

void Entity::setSpeed(double multiplier)
{
	speedMul = multiplier;
	if (area) {
		double tilesPerSecond = area->getTileDimensions().x / 1000.0;
		speed = baseSpeed * speedMul * tilesPerSecond;
	}
}

Tile* Entity::getTile() const
{
	return area ? area->getTile(r) : NULL;
}

Tile* Entity::getTile()
{
	return area ? area->getTile(r) : NULL;
}

void Entity::setFrozen(bool b)
{
	frozen = b;
}

bool Entity::getFrozen()
{
	return frozen;
}

FlagManip Entity::exemptManip()
{
	return FlagManip(&nowalkExempt);
}

void Entity::erase()
{
	throw "pure virtual function";
}

void Entity::calcDraw()
{
	ivec2 tile = area->getTileDimensions();

	// X-axis is centered on tile.
	doff.x = (tile.x - imgsz.x) / 2;
	// Y-axis is aligned with bottom of tile.
	doff.y = tile.y - imgsz.y;
}

SampleRef Entity::getSound(const std::string& name) const
{
	SampleMap::const_iterator it;
	it = sounds.find(name);
	if (it != sounds.end())
		return it->second;
	else
		return SampleRef();
}

ivec2 Entity::setFacing(ivec2 facing)
{
	this->facing = ivec2(
		Gosu::clamp(facing.x, -1, 1),
		Gosu::clamp(facing.y, -1, 1)
	);
	return this->facing;
}

const std::string& Entity::directionStr(ivec2 facing) const
{
	return directions[facing.y+1][facing.x+1];
}

enum SetPhaseResult Entity::_setPhase(const std::string& name)
{
	AnimationMap::iterator it;
	it = phases.find(name);
	if (it == phases.end()) {
		return PHASE_NOTFOUND;
	}
	Animation* newPhase = &it->second;
	if (phase != newPhase) {
		time_t now = World::instance()->time();
		phase = newPhase;
		phase->startOver(now, ANIM_INFINITE_CYCLES);
		phaseName = name;
		redraw = true;
		return PHASE_CHANGED;
	}
	return PHASE_NOTCHANGED;
}

bool Entity::nowalked(Tile& t)
{
	unsigned flags = nowalkFlags & ~nowalkExempt;
	return t.hasFlag(flags);
}

void Entity::preMove()
{
	fromCoord = r;
	fromTile = getTile();

	rcoord d = destCoord - fromCoord;
	deltaCoord = area->virt2virt(d);

	moving = true;

	// Start moving animation.
	switch (conf.moveMode) {
	case TURN:
		break;
	case TILE:
	case NOTILE:
		setPhase("moving " + getFacing());
		break;
	}

	// Process triggers.
	runTileExitScript();
	if (fromTile)
		fromTile->runLeaveScript(this);

	// Modify tile's entity count.
	leaveTile();
	enterTile(destTile);

	SampleRef step = getSound("step");
	if (step)
		step->play();

	// Set z right away so that we're on-level with the square we're
	// entering.
	r.z = destCoord.z;

	if (conf.moveMode == TURN) {
		// Movement is instantaneous.
		redraw = true;
		r = destCoord;
		postMove();
	}
	else {
		// Movement happens over time. See tickTile().
	}
}

void Entity::postMove()
{
	moving = false;

	if (destTile) {
		double* layermod = destTile->layermods[EXIT_NORMAL];
		if (layermod)
			r.z = *layermod;
	}

	// Stop moving animation.
	if (!stillMoving)
		setPhase(getFacing());

	// Process triggers.
	if (destTile)
		destTile->runEnterScript(this);

	runTileEntryScript();

	// TODO: move teleportation here
	/*
	 * if (onExit()) {
	 * 	leaveTile();
	 * 	moveArea(getExit());
	 * 	postMoveScript();
	 * 	enterTile();
	 * }
	 */
}

void Entity::leaveTile()
{
	Tile* t = getTile();
	if (t)
		t->entCnt--;
}

void Entity::enterTile()
{
	Tile* t = getTile();
	if (t)
		enterTile(getTile());
}

void Entity::enterTile(Tile* t)
{
	if (t)
		t->entCnt++;
}

void Entity::runTickScript()
{
	if (!tickScript)
		return;
	pythonSetGlobal("Area", area);
	pythonSetGlobal("Entity", this);
	pythonSetGlobal("Tile", getTile());
	tickScript->invoke();
}

void Entity::runTurnScript()
{
	if (!turnScript)
		return;
	pythonSetGlobal("Area", area);
	pythonSetGlobal("Entity", this);
	pythonSetGlobal("Tile", getTile());
	turnScript->invoke();
}

void Entity::runTileExitScript()
{
	if (!tileExitScript)
		return;
	pythonSetGlobal("Area", area);
	pythonSetGlobal("Entity", this);
	pythonSetGlobal("Tile", getTile());
	tileExitScript->invoke();
}

void Entity::runTileEntryScript()
{
	if (!tileEntryScript)
		return;
	pythonSetGlobal("Area", area);
	pythonSetGlobal("Entity", this);
	pythonSetGlobal("Tile", getTile());
	tileEntryScript->invoke();
}


/*
 * DESCRIPTOR CODE BELOW
 */

bool Entity::processDescriptor()
{
	XMLRef doc = Reader::getXMLDoc(descriptor, "dtd/entity.dtd");
	if (!doc)
		return false;
	const XMLNode root = doc->root(); // <entity>
	if (!root)
		return false;

	for (XMLNode node = root.childrenNode(); node; node = node.next()) {
		if (node.is("speed")) {
			ASSERT(node.doubleContent(&baseSpeed));
			setSpeed(speedMul); // Calculate speed from tile size.
		} else if (node.is("sprite")) {
			ASSERT(processSprite(node.childrenNode()));
		} else if (node.is("sounds")) {
			ASSERT(processSounds(node.childrenNode()));
		} else if (node.is("scripts")) {
			ASSERT(processScripts(node.childrenNode()));
		}
	}
	return true;
}

bool Entity::processSprite(XMLNode node)
{
	TiledImageRef tiles;
	for (; node; node = node.next()) {
		if (node.is("sheet")) {
			std::string imageSheet = node.content();
			ASSERT(node.intAttr("tile_width",  &imgsz.x) &&
			       node.intAttr("tile_height", &imgsz.y));
			tiles = Reader::getTiledImage(imageSheet, imgsz.x, imgsz.y);
			ASSERT(tiles);
		} else if (node.is("phases")) {
			ASSERT(processPhases(node.childrenNode(), tiles));
		}
	}
	return true;
}

bool Entity::processPhases(XMLNode node, const TiledImageRef& tiles)
{
	for (; node; node = node.next())
		if (node.is("phase"))
			ASSERT(processPhase(node, tiles));
	return true;
}

bool Entity::processPhase(const XMLNode node, const TiledImageRef& tiles)
{
	/* Each phase requires a 'name' and 'frames'. Additionally,
	 * 'speed' is required if 'frames' has more than one member.
	 */
	const std::string name = node.attr("name");
	if (name.empty()) {
		Log::err(descriptor, "<phase> name attribute is empty");
		return false;
	}

	const std::string framesStr = node.attr("frames");
	const std::string speedStr = node.attr("speed");

	if (framesStr.empty()) {
		Log::err(descriptor, "<phase> frames attribute empty");
		return false;
	}

	if (isInteger(framesStr)) {
		int frame = atoi(framesStr.c_str());
		if (frame < 0 || (int)tiles->size() < frame) {
			Log::err(descriptor,
				"<phase> frames attribute index out of bounds");
			return false;
		}
		const ImageRef& image = (*tiles.get())[frame];
		phases[name] = Animation(image);
	}
	else if (isRanges(framesStr)) {
		if (!isDecimal(speedStr)) {
			Log::err(descriptor,
				"<phase> speed attribute must be present and "
				"must be decimal");
		}
		double fps = atof(speedStr.c_str());

		typedef std::vector<int> IntVector;
		IntVector frames = parseRanges(framesStr);
		std::vector<ImageRef> images;
		for (IntVector::iterator it = frames.begin(); it != frames.end(); it++) {
			int i = *it;
			if (i < 0 || (int)tiles->size() < i) {
				Log::err(descriptor,
					"<phase> frames attribute index out of bounds");
				return false;
			}
			images.push_back((*tiles.get())[i]);
		}

		phases[name] = Animation(images, (time_t)(1000.0 / fps));
	}
	else {
		Log::err(descriptor,
			"<phase> frames attribute not an int or int ranges");
		return false;
	}

	return true;
}

bool Entity::processSounds(XMLNode node)
{
	for (; node; node = node.next())
		if (node.is("sound"))
			ASSERT(processSound(node));
	return true;
}

bool Entity::processSound(const XMLNode node)
{
	const std::string name = node.attr("name");
	const std::string filename = node.content();
	if (name.empty()) {
		Log::err(descriptor, "<sound> name attribute is empty");
		return false;
	} else if (filename.empty()) {
		Log::err(descriptor, "<sound></sound> is empty");
		return false;
	}

	SampleRef s = Reader::getSample(filename);
	if (s)
		sounds[name] = s;
	return true;
}

bool Entity::processScripts(XMLNode node)
{
	for (; node; node = node.next())
		if (node.is("script"))
			ASSERT(processScript(node));
	return true;
}

bool Entity::processScript(const XMLNode node)
{
	const std::string trigger = node.attr("trigger");
	const std::string filename = node.content();
	if (trigger.empty()) {
		Log::err(descriptor, "<script> trigger attribute is empty");
		return false;
	} else if (filename.empty()) {
		Log::err(descriptor, "<script></script> is empty");
		return false;
	}

	ScriptRef script = Script::create(filename);
	if (!script || !script->validate())
		return false;

	if (!setScript(trigger, script)) {
		Log::err(descriptor,
			"unrecognized script trigger: " + trigger);
		return false;
	}

	return true;
}

bool Entity::setScript(const std::string& trigger, ScriptRef& script)
{
	if (trigger == "on_tick") {
		tickScript = script;
		return true;
	}
	if (trigger == "on_turn") {
		turnScript = script;
		return true;
	}
	if (trigger == "on_tile_entry") {
		tileEntryScript = script;
		return true;
	}
	if (trigger == "on_tile_exit") {
		tileExitScript = script;
		return true;
	}
	if (trigger == "on_delete") {
		deleteScript = script;
		return true;
	}
	return false;
}


void exportEntity()
{
	using namespace boost::python;

	class_<Entity>("Entity", no_init)
		.def("init", &Entity::init)
		.def("delete", &Entity::destroy)
		.add_property("frozen", &Entity::getFrozen, &Entity::setFrozen)
		.add_property("phase", &Entity::getPhase, &Entity::setPhase)
		.add_property("area",
		    make_function(&Entity::getArea,
		      return_value_policy<reference_existing_object>()),
		    &Entity::setArea)
		.add_property("tile", make_function(
		    static_cast<Tile* (Entity::*) ()> (&Entity::getTile),
		    return_value_policy<reference_existing_object>()))
		.add_property("speed", &Entity::getSpeed, &Entity::setSpeed)
		.add_property("moving", &Entity::isMoving)
		.add_property("exempt", &Entity::exemptManip)
		.add_property("coords", &Entity::getTileCoords_vi)
		.def("set_coords",
		    static_cast<void (Entity::*) (int,int,double)>
		      (&Entity::setTileCoords))
		.def("teleport", &Entity::teleport)
		.def("move", &Entity::move)
		.def("move_dest",
		    static_cast<vicoord (Entity::*) (Tile*,int,int)>
		      (&Entity::moveDest))
		.def("can_move",
		    static_cast<bool (Entity::*) (int,int,double)>
		      (&Entity::canMove))
//		.def_readwrite("on_tick", &Entity::tickScript)
//		.def_readwrite("on_turn", &Entity::turnScript)
//		.def_readwrite("on_tile_entry", &Entity::tileEntryScript)
//		.def_readwrite("on_tile_exit", &Entity::tileExitScript)
//		.def_readwrite("on_delete", &Entity::deleteScript)
		;
}

