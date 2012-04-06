/*********************************
** Tsunagari Tile Engine        **
** player.cpp                   **
** Copyright 2011-2012 OmegaSDG **
*********************************/

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <Gosu/Audio.hpp>
#include <Gosu/Input.hpp>
#include <Gosu/Math.hpp>

#include "area.h"
#include "config.h"
#include "entity.h"
#include "log.h"
#include "player.h"
#include "world.h"
#include "window.h"

template<class Cont, class ValueType>
void removeValue(Cont* c, ValueType v)
{
	typename Cont::iterator it;
	for (it = c->begin(); it != c->end(); ++it) {
		if (*it == v) {
			c->erase(it);
			return;
		}
	}
}


Player::Player(Area* area)
	: Entity(area), velocity(0, 0)
{
}

void Player::startMovement(ivec2 delta)
{
	switch (conf.moveMode) {
	case TURN:
		moveByTile(delta);
		break;
	case TILE:
		movements.push_back(delta);
		velocity = delta;
		moveByTile(velocity);
		break;
	case NOTILE:
		// TODO
		break;
	}
	stillMoving = velocity;
}

void Player::stopMovement(ivec2 delta)
{
	switch (conf.moveMode) {
	case TURN:
		break;
	case TILE:
		removeValue(&movements, delta);
		velocity = movements.size() ?
		           movements.back() :
			   ivec2(0, 0);
		if (velocity)
			moveByTile(velocity);
		break;
	case NOTILE:
		// TODO
		break;
	}
	stillMoving = velocity;
}

void Player::moveByTile(ivec2 delta)
{
	if (moving)
		// Support queueing moves?
		return;

	// Left CTRL allows changing facing, but disallows movement.
	const GameWindow& window = GameWindow::getWindow();
	if (window.input().down(Gosu::kbLeftControl)) {
		setPhase(directionStr(setFacing(delta)));
		redraw = true;
		return;
	}

	// FIXME: use frontTiles()
	icoord newCoord = getTileCoords_i();
	newCoord += icoord(delta.x, delta.y, 0);

	/*
	// The tile is off the map. Turn to face the direction, but don't move.
	if (!area->inBounds(newCoord)) {
		setPhase(directionStr(setFacing(delta)));
		redraw = true;
		return;
	}
	*/

//	destTile = &area->getTile(newCoord);

	/*
	// Is anything player-specific preventing us from moving?
	if (destTile->hasFlag(TILE_NOWALK_PLAYER)) {
		// The tile we're trying to move onto is set as
		// TILE_NOWALK_PLAYER. Turn to face the direction, but don't
		// move.
		setPhase(directionStr(setFacing(delta)));
		redraw = true;
		return;
	}
	*/

	Entity::moveByTile(delta);
}

void Player::useTile()
{
	std::vector<icoord> tiles = frontTiles();
	BOOST_FOREACH(icoord& c, tiles) {
		Tile& t = area->getTile(c);
		t.onUseScripts(this);
	}
}

void Player::preMove()
{
	Entity::preMove();

	SampleRef step = getSound("step");
	if (step)
		step->play();

}

void Player::postMove()
{
	Entity::postMove();

	// Exits
	if (destTile) {
		const Exit* exit = destTile->exits[EXIT_NORMAL];
		if (exit) {
			World* world = World::instance();
			AreaPtr newArea = world->getArea(exit->area);
			if (newArea) {
				world->focusArea(newArea, exit->coord);
			}
			else {
				// Roll back movement if exit failed to open.
				r = fromCoord;
				Log::err("Exit",
					 exit->area + ": failed to load properly");
			}
		}
	}
	if (fromTile) {
		icoord delta = area->virt2phys(destCoord);
		delta -= area->virt2phys(fromCoord);
		if (delta.z == 0) {
			Exit* exit = area->getTile(area->virt2phys(fromCoord)).exitAt(ivec2(delta.x, delta.y));
			if (exit) {
				World* world = World::instance();
				AreaPtr newArea = world->getArea(exit->area);
				if (newArea) {
					world->focusArea(newArea, exit->coord);
				}
				else {
					// Roll back movement if exit failed to open.
					r = fromCoord;
					Log::err("Exit",
						 exit->area + ": failed to load properly");
				}
			}
		}
	}

	vicoord tile = getTileCoords_vi();
	Log::info("Player", boost::str(
		boost::format("location x:%d y:%d z:%.1f")
		  % tile.x % tile.y % r.z)
	);

	// If we have a velocity, keep moving.
	if (conf.moveMode == TILE && velocity)
		moveByTile(velocity);
}

