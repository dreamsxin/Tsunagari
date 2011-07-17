/******************************
** Tsunagari Tile Engine     **
** entity.cpp                **
** Copyright 2011 OmegaSDG   **
******************************/

#include <Gosu/Image.hpp>
#include <Gosu/Math.hpp>
#include <Gosu/Timing.hpp>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "area.h"
#include "entity.h"
#include "log.h"
#include "resourcer.h"

Entity::Entity(Resourcer* rc, Area* area)
	: rc(rc),
	  redraw(true),
	  moving(false),
	  speed(240.0 / 1000),
	  area(area)
{
	c.x = c.y = c.z = 0;
}

Entity::~Entity()
{
}

bool Entity::init(const std::string& descriptor)
{
	this->descriptor = descriptor;
	return processDescriptor() && loadPhases();
}

void Entity::draw()
{
	redraw = false;
	img->draw((double)c.x, (double)c.y, (double)0);
}

bool Entity::needsRedraw() const
{
	return redraw;
}

void Entity::update(unsigned long dt)
{
	if (true && moving) {
		redraw = true;

		double destDist = Gosu::distance((double)c.x, (double)c.y,
				(double)dest.x, (double)dest.y); 
		if (destDist < speed * (double)dt) {
			c = dest;
			moving = false;
			postMove();
		}
		else {
			double angle = Gosu::angle((double)c.x, (double)c.y,
					(double)dest.x, (double)dest.y); 
			double x = Gosu::offsetX(angle, speed * (double)dt);
			double y = Gosu::offsetY(angle, speed * (double)dt);

			// Save state of partial pixels traveled in double
			rx += x;
			ry += y;

			c.x = (long)rx;
			c.y = (long)ry;
		}
	}
}

bool Entity::setPhase(const std::string& name)
{
	bool changed = false;
	boost::unordered_map<std::string, ImageRef>::iterator phase;
	phase = imgs.find(name);
	if (phase != imgs.end()) {
		ImageRef newImg = phase->second;
		changed = (img != newImg);
		img = newImg;
	}
	return changed;
}

coord_t Entity::getCoordsByPixel() const
{
	return c;
}

coord_t Entity::getCoordsByTile() const
{
	coord_t tileDim = area->getTileDimensions();
	coord_t coords;
	coords.x = c.x / tileDim.x;
	coords.y = c.y / tileDim.y;
	coords.z = c.z; // XXX: revisit when we have Z-buffers
	return coords;
}

void Entity::setCoordsByPixel(coord_t coords)
{
	c = coords;
	redraw = true;
}

void Entity::setCoordsByTile(coord_t coords)
{
	coord_t tileDim = area->getTileDimensions();
	c = coords;
	c.x *= tileDim.x;
	c.y *= tileDim.y;
	// XXX: set c.z when we have Z-buffers
	redraw = true;
}

void Entity::moveByPixel(coord_t dc)
{
	c.x += dc.x;
	c.y += dc.y;
	c.z += dc.z;
	redraw = true;
}

void Entity::moveByTile(coord_t dc)
{
	if (true && moving)
		// support queueing moves?
		return;

	coord_t newCoord = getCoordsByTile();
	newCoord.x += dc.x;
	newCoord.y += dc.y;
	newCoord.z += dc.z;

	// Can we move?
	const Area::Tile& tile = area->getTile(newCoord);
	if ((tile.flags       & Area::nowalk) != 0 ||
	    (tile.type->flags & Area::nowalk) != 0) {
		// The tile we're trying to move onto is set as nowalk.
		// Stop here.
		return;
	}

	// Move!
	redraw = true;
	const coord_t tileDim = area->getTileDimensions();
	dest.x = c.x + dc.x * tileDim.x;
	dest.y = c.y + dc.y * tileDim.y;
	dest.z = 0; // XXX: set dest.z when we have Z-buffers

	preMove(dest);

	if (false) {
		c.x = dest.x;
		c.y = dest.y;
		// XXX: set c.z when we have Z-buffers
		postMove();
	}
	else if (true) {
		moving = true;
		rx = (double)c.x;
		ry = (double)c.y;
	}
}

void Entity::setArea(Area* a)
{
	area = a;
}

SampleRef Entity::getSound(const std::string& name)
{
	boost::unordered_map<std::string, SampleRef>::iterator it;
	
	it = sounds.find(name);
	if (it != sounds.end())
		return it->second;
	else
		return SampleRef();
}

/**
 * Try to load in descriptor.
 */
bool Entity::processDescriptor()
{
	XMLDocRef doc = rc->getXMLDoc(descriptor, "dtd/entity.dtd");
	if (!doc)
		return false;
	const xmlNode* root = xmlDocGetRootElement(doc.get()); // <entity>
	if (!root)
		return false;
	xmlNode* node = root->xmlChildrenNode; // children of <entity>

	for (; node != NULL; node = node->next) {
		if (!xmlStrncmp(node->name, BAD_CAST("sprite"), 6)) {
			if (!processSprite(node))
				return false;
		}
		else if (!xmlStrncmp(node->name, BAD_CAST("sounds"), 7)) {
			if (!processSounds(node))
				return false;
		}
	}
	return true;
}

bool Entity::processSprite(const xmlNode* sprite)
{
	xmlChar* str;
	for (xmlNode* child = sprite->xmlChildrenNode; child != NULL;
			child = child->next) {
		if (!xmlStrncmp(child->name, BAD_CAST("sheet"), 6)) {
			str = xmlNodeGetContent(child);
			xml.sheet = (char*)str;

			str = xmlGetProp(child, BAD_CAST("tilewidth"));
			xml.tileSize.x = atol((char*)str); // atol

			str = xmlGetProp(child, BAD_CAST("tileheight"));
			xml.tileSize.y = atol((char*)str); // atol
		}
		else if (!xmlStrncmp(child->name, BAD_CAST("phases"), 7) &&
				!processPhases(child))
			return false;
	}
	return true;
}

bool Entity::processPhases(const xmlNode* phases)
{
	for (xmlNode* phase = phases->xmlChildrenNode; phase != NULL;
			phase = phase->next)
		if (!xmlStrncmp(phase->name, BAD_CAST("phase"), 6)) // necessary?
			if (!processPhase(phase))
				return false;
	return true;
}

bool Entity::processPhase(xmlNode* phase)
{
	/* Each phase requires a 'name'. Additionally,
	 * one of either 'pos' or 'speed' is needed.
	 * If speed is used, we have sub-elements. We
	 * can't have both pos and speed.
	 */
	const std::string name = (char*)xmlGetProp(phase, BAD_CAST("name"));

	const xmlChar* pos = xmlGetProp(phase, BAD_CAST("pos"));
	const xmlChar* speed = xmlGetProp(phase, BAD_CAST("speed"));

	// FIXME: check name + pos | speed for 0 length

	if (pos && speed) {
		Log::err(descriptor, "pos and speed attributes in "
				"element phase are mutually exclusive");
		return false;
	}
	if (!pos && !speed) {
		Log::err(descriptor, "must have pos or speed attribute "
			       "in element phase");
		return false;
	}

	if (pos) {
		const unsigned value = (unsigned)atoi((const char*)pos); // atol
		xml.phases[name] = value;
	}
	else { // speed
		// TODO: Load animated sprites
		// Load <member> subelements
	}

	return true;
}

bool Entity::processSounds(const xmlNode* sounds)
{
	for (xmlNode* sound = sounds->xmlChildrenNode; sound != NULL;
			sound = sound->next)
		if (!xmlStrncmp(sound->name, BAD_CAST("sound"), 6)) // necessary?
			if (!processSound(sound))
				return false;
	return true;
}

bool Entity::processSound(xmlNode* sound)
{
	const std::string name = (char*)xmlGetProp(sound, BAD_CAST("name"));
	const std::string filename = (char*)xmlNodeGetContent(sound);
	// FIXME: check name, filename for 0 length

	SampleRef s = rc->getSample(filename);
	if (s)
		sounds[name] = s;
	else
		Log::err(descriptor, std::string("sound ") +
				filename + " not found");
	return s;
}

bool Entity::loadPhases()
{
	TiledImage tiles;
	if (!rc->getTiledImage(tiles, xml.sheet, (unsigned)xml.tileSize.x,
			(unsigned)xml.tileSize.y, false))
		return false;

	boost::unordered_map<std::string, unsigned>::iterator it;
	for (it = xml.phases.begin(); it != xml.phases.end(); it++) {
		const std::string& name = it->first;
		unsigned idx = it->second;
		ImageRef image = tiles[idx];
		imgs[name] = img = image;
	}
	return true;
}

void Entity::preMove(coord_t)
{
}

void Entity::postMove()
{
}

