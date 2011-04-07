/******************************
** Tsunagari Tile Engine     **
** resourcer.cpp             **
** Copyright 2011 OmegaSDG   **
******************************/

#include "Gosu/Utility.hpp"

#include "resourcer.h"

Resourcer::Resourcer(GameWindow* window/*, string filename*/)
{
	this->window = window;
}

Resourcer::~Resourcer()
{
}

Gosu::Image* Resourcer::getImage(const std::string name)
{
	return new Gosu::Image(window->graphics(), Gosu::widen(name), false);
}

