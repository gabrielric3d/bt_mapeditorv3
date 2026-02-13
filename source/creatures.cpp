//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "main.h"

#include "gui.h"
#include "materials.h"
#include "brush.h"
#include "creatures.h"
#include "creature_brush.h"

#include <nlohmann/json.hpp>
#include <fstream>
using json = nlohmann::json;

CreatureDatabase g_creatures;

CreatureType::CreatureType() :
	isNpc(false),
	missing(false),
	in_other_tileset(false),
	standard(false),
	name(""),
	brush(nullptr),
	wander_radius(0),
	walk_speed(0),
	rest_ticks(120),
	walk_steps(2)
{
	////
}

CreatureType::CreatureType(const CreatureType& ct) :
	isNpc(ct.isNpc),
	missing(ct.missing),
	in_other_tileset(ct.in_other_tileset),
	standard(ct.standard),
	name(ct.name),
	outfit(ct.outfit),
	brush(ct.brush),
	wander_radius(ct.wander_radius),
	walk_speed(ct.walk_speed),
	rest_ticks(ct.rest_ticks),
	walk_steps(ct.walk_steps)
{
	////
}

CreatureType& CreatureType::operator=(const CreatureType& ct)
{
	isNpc = ct.isNpc;
	missing = ct.missing;
	in_other_tileset = ct.in_other_tileset;
	standard = ct.standard;
	name = ct.name;
	outfit = ct.outfit;
	brush = ct.brush;
	wander_radius = ct.wander_radius;
	walk_speed = ct.walk_speed;
	rest_ticks = ct.rest_ticks;
	walk_steps = ct.walk_steps;
	return *this;
}

CreatureType::~CreatureType()
{
	////
}

CreatureType* CreatureType::loadFromXML(pugi::xml_node node, wxArrayString& warnings)
{
	pugi::xml_attribute attribute;
	if(!(attribute = node.attribute("type"))) {
		warnings.push_back("Couldn't read type tag of creature node.");
		return nullptr;
	}

	const std::string& tmpType = attribute.as_string();
	if(tmpType != "monster" && tmpType != "npc") {
		warnings.push_back("Invalid type tag of creature node \"" + wxstr(tmpType) + "\"");
		return nullptr;
	}

	if(!(attribute = node.attribute("name"))) {
		warnings.push_back("Couldn't read name tag of creature node.");
		return nullptr;
	}

	CreatureType* ct = newd CreatureType();
	ct->name = attribute.as_string();
	ct->isNpc = tmpType == "npc";

	if((attribute = node.attribute("looktype"))) {
		ct->outfit.lookType = attribute.as_int();
		if(g_gui.gfx.getCreatureSprite(ct->outfit.lookType) == nullptr) {
			warnings.push_back("Invalid creature \"" + wxstr(ct->name) + "\" look type #" + std::to_string(ct->outfit.lookType));
		}
	}

	if((attribute = node.attribute("lookitem"))) {
		ct->outfit.lookItem = attribute.as_int();
	}

	if((attribute = node.attribute("lookmount"))) {
		ct->outfit.lookMount = attribute.as_int();
	}

	if((attribute = node.attribute("lookaddon"))) {
		ct->outfit.lookAddon = attribute.as_int();
	}

	if((attribute = node.attribute("lookhead"))) {
		ct->outfit.lookHead = attribute.as_int();
	}

	if((attribute = node.attribute("lookbody"))) {
		ct->outfit.lookBody = attribute.as_int();
	}

	if((attribute = node.attribute("looklegs"))) {
		ct->outfit.lookLegs = attribute.as_int();
	}

	if((attribute = node.attribute("lookfeet"))) {
		ct->outfit.lookFeet = attribute.as_int();
	}
	return ct;
}

CreatureType* CreatureType::loadFromOTXML(const FileName& filename, pugi::xml_document& doc, wxArrayString& warnings)
{
	ASSERT(doc != nullptr);

	bool isNpc;
	pugi::xml_node node;
	if((node = doc.child("monster"))) {
		isNpc = false;
	} else if((node = doc.child("npc"))) {
		isNpc = true;
	} else {
		warnings.push_back("This file is not a monster/npc file");
		return nullptr;
	}

	pugi::xml_attribute attribute;
	if(!(attribute = node.attribute("name"))) {
		warnings.push_back("Couldn't read name tag of creature node.");
		return nullptr;
	}

	CreatureType* ct = newd CreatureType();
	if(isNpc) {
		ct->name = nstr(filename.GetName());
	} else {
		ct->name = attribute.as_string();
	}
	ct->isNpc = isNpc;

	for(pugi::xml_node optionNode = node.first_child(); optionNode; optionNode = optionNode.next_sibling()) {
		if(as_lower_str(optionNode.name()) != "look") {
			continue;
		}

		if((attribute = optionNode.attribute("type"))) {
			ct->outfit.lookType = attribute.as_int();
		}

		if((attribute = optionNode.attribute("item")) || (attribute = optionNode.attribute("lookex")) || (attribute = optionNode.attribute("typeex"))) {
			ct->outfit.lookItem = attribute.as_int();
		}

		if((attribute = optionNode.attribute("mount"))) {
			ct->outfit.lookMount = attribute.as_int();
		}

		if((attribute = optionNode.attribute("addon"))) {
			ct->outfit.lookAddon = attribute.as_int();
		}

		if((attribute = optionNode.attribute("head"))) {
			ct->outfit.lookHead = attribute.as_int();
		}

		if((attribute = optionNode.attribute("body"))) {
			ct->outfit.lookBody = attribute.as_int();
		}

		if((attribute = optionNode.attribute("legs"))) {
			ct->outfit.lookLegs = attribute.as_int();
		}

		if((attribute = optionNode.attribute("feet"))) {
			ct->outfit.lookFeet = attribute.as_int();
		}
	}
	return ct;
}

CreatureDatabase::CreatureDatabase()
{
	////
}

CreatureDatabase::~CreatureDatabase()
{
	clear();
}

void CreatureDatabase::clear()
{
	for(CreatureMap::iterator iter = creature_map.begin(); iter != creature_map.end(); ++iter) {
		delete iter->second;
	}
	creature_map.clear();
}

CreatureType* CreatureDatabase::operator[](const std::string& name)
{
	CreatureMap::iterator iter = creature_map.find(as_lower_str(name));
	if(iter != creature_map.end()) {
		return iter->second;
	}
	return nullptr;
}

CreatureType* CreatureDatabase::addMissingCreatureType(const std::string& name, bool isNpc)
{
	assert((*this)[name] == nullptr);

	CreatureType* ct = newd CreatureType();
	ct->name = name;
	ct->isNpc = isNpc;
	ct->missing = true;
	ct->outfit.lookType = 130;

	creature_map.insert(std::make_pair(as_lower_str(name), ct));
	return ct;
}

CreatureType* CreatureDatabase::addCreatureType(const std::string& name, bool isNpc, const Outfit& outfit)
{
	assert((*this)[name] == nullptr);

	CreatureType* ct = newd CreatureType();
	ct->name = name;
	ct->isNpc = isNpc;
	ct->missing = false;
	ct->outfit = outfit;

	creature_map.insert(std::make_pair(as_lower_str(name), ct));
	return ct;
}

bool CreatureDatabase::hasMissing() const
{
	for(CreatureMap::const_iterator iter = creature_map.begin(); iter != creature_map.end(); ++iter) {
		if(iter->second->missing) {
			return true;
		}
	}
	return false;
}

bool CreatureDatabase::loadFromXML(const FileName& filename, bool standard, wxString& error, wxArrayString& warnings)
{
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.GetFullPath().mb_str());
	if(!result) {
		error = "Couldn't open file \"" + filename.GetFullName() + "\", invalid format?";
		return false;
	}

	pugi::xml_node node = doc.child("creatures");
	if(!node) {
		error = "Invalid file signature, this file is not a valid creatures file.";
		return false;
	}

	for(pugi::xml_node creatureNode = node.first_child(); creatureNode; creatureNode = creatureNode.next_sibling()) {
		if(as_lower_str(creatureNode.name()) != "creature") {
			continue;
		}

		CreatureType* creatureType = CreatureType::loadFromXML(creatureNode, warnings);
		if(creatureType) {
			creatureType->standard = standard;
			if((*this)[creatureType->name]) {
				warnings.push_back("Duplicate creature type name \"" + wxstr(creatureType->name) + "\"! Discarding...");
				delete creatureType;
			} else {
				creature_map[as_lower_str(creatureType->name)] = creatureType;
			}
		}
	}
	return true;
}

bool CreatureDatabase::importXMLFromOT(const FileName& filename, wxString& error, wxArrayString& warnings)
{
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.GetFullPath().mb_str());
	if(!result) {
		error = "Couldn't open file \"" + filename.GetFullName() + "\", invalid format?";
		return false;
	}

	pugi::xml_node node;
	if((node = doc.child("monsters"))) {
		for(pugi::xml_node monsterNode = node.first_child(); monsterNode; monsterNode = monsterNode.next_sibling()) {
			if(as_lower_str(monsterNode.name()) != "monster") {
				continue;
			}

			pugi::xml_attribute attribute;
			if(!(attribute = monsterNode.attribute("file"))) {
				continue;
			}

			FileName monsterFile(filename);
			monsterFile.SetFullName(wxString(attribute.as_string(), wxConvUTF8));

			pugi::xml_document monsterDoc;
			pugi::xml_parse_result monsterResult = monsterDoc.load_file(monsterFile.GetFullPath().mb_str());
			if(!monsterResult) {
				continue;
			}

			CreatureType* creatureType = CreatureType::loadFromOTXML(monsterFile, monsterDoc, warnings);
			if(creatureType) {
				CreatureType* current = (*this)[creatureType->name];
				if(current) {
					*current = *creatureType;
					delete creatureType;
				} else {
					creature_map[as_lower_str(creatureType->name)] = creatureType;

					Tileset* tileSet = nullptr;
					if(creatureType->isNpc) {
						tileSet = g_materials.tilesets["NPCs"];
					} else {
						tileSet = g_materials.tilesets["Others"];
					}
					ASSERT(tileSet != nullptr);

					Brush* brush = newd CreatureBrush(creatureType);
					g_brushes.addBrush(brush);

					TilesetCategory* tileSetCategory = tileSet->getCategory(TILESET_CREATURE);
					tileSetCategory->brushlist.push_back(brush);
				}
			}
		}
	} else if((node = doc.child("monster")) || (node = doc.child("npc"))) {
		CreatureType* creatureType = CreatureType::loadFromOTXML(filename, doc, warnings);
		if(creatureType) {
			CreatureType* current = (*this)[creatureType->name];

			if(current) {
				*current = *creatureType;
				delete creatureType;
			} else {
				creature_map[as_lower_str(creatureType->name)] = creatureType;

				Tileset* tileSet = nullptr;
				if(creatureType->isNpc) {
					tileSet = g_materials.tilesets["NPCs"];
				} else {
					tileSet = g_materials.tilesets["Others"];
				}
				ASSERT(tileSet != nullptr);

				Brush* brush = newd CreatureBrush(creatureType);
				g_brushes.addBrush(brush);

				TilesetCategory* tileSetCategory = tileSet->getCategory(TILESET_CREATURE);
				tileSetCategory->brushlist.push_back(brush);
			}
		}
	} else {
		error = "This is not valid OT npc/monster data file.";
		return false;
	}
	return true;
}

bool CreatureDatabase::saveToXML(const FileName& filename)
{
	pugi::xml_document doc;

	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";

	pugi::xml_node creatureNodes = doc.append_child("creatures");
	for(const auto& creatureEntry : creature_map) {
		CreatureType* creatureType = creatureEntry.second;
		if(!creatureType->standard) {
			pugi::xml_node creatureNode = creatureNodes.append_child("creature");

			creatureNode.append_attribute("name") = creatureType->name.c_str();
			creatureNode.append_attribute("type") = creatureType->isNpc ? "npc" : "monster";

			const Outfit& outfit = creatureType->outfit;
			creatureNode.append_attribute("looktype") = outfit.lookType;
			creatureNode.append_attribute("lookitem") = outfit.lookItem;
			creatureNode.append_attribute("lookmount") = outfit.lookMount;
			creatureNode.append_attribute("lookaddon") = outfit.lookAddon;
			creatureNode.append_attribute("lookhead") = outfit.lookHead;
			creatureNode.append_attribute("lookbody") = outfit.lookBody;
			creatureNode.append_attribute("looklegs") = outfit.lookLegs;
			creatureNode.append_attribute("lookfeet") = outfit.lookFeet;
		}
	}
	return doc.save_file(filename.GetFullPath().mb_str(), "\t", pugi::format_default, pugi::encoding_utf8);
}

bool CreatureDatabase::loadBehaviors(const FileName& filename, wxString& error, wxArrayString& warnings)
{
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.GetFullPath().mb_str());
	if(!result) {
		// File doesn't exist yet - that's OK, no behaviors configured
		return true;
	}

	pugi::xml_node root = doc.child("creature_behaviors");
	if(!root) {
		error = "Invalid creature_behaviors.xml format.";
		return false;
	}

	for(pugi::xml_node node = root.first_child(); node; node = node.next_sibling()) {
		if(as_lower_str(node.name()) != "creature")
			continue;

		std::string name = node.attribute("name").as_string();
		if(name.empty())
			continue;

		CreatureType* type = (*this)[name];
		if(!type) {
			warnings.push_back("Creature behavior for unknown creature: " + wxString(name));
			continue;
		}

		type->wander_radius = node.attribute("wanderradius").as_int(0);
		type->walk_speed = node.attribute("walkspeed").as_int(0);
		type->rest_ticks = node.attribute("restticks").as_int(120);
		type->walk_steps = node.attribute("walksteps").as_int(2);
	}
	return true;
}

bool CreatureDatabase::saveBehaviors(const FileName& filename, wxString& error)
{
	pugi::xml_document doc;
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";
	decl.append_attribute("encoding") = "UTF-8";

	pugi::xml_node root = doc.append_child("creature_behaviors");

	for(const auto& pair : creature_map) {
		CreatureType* type = pair.second;
		if(!type->hasWanderBehavior() && type->walk_speed == 0 && type->rest_ticks == 120 && type->walk_steps == 2)
			continue;

		pugi::xml_node node = root.append_child("creature");
		node.append_attribute("name") = type->name.c_str();
		node.append_attribute("wanderradius") = type->wander_radius;
		node.append_attribute("walkspeed") = type->walk_speed;
		node.append_attribute("restticks") = type->rest_ticks;
		node.append_attribute("walksteps") = type->walk_steps;
	}

	if(!doc.save_file(filename.GetFullPath().mb_str())) {
		error = "Failed to save creature_behaviors.xml";
		return false;
	}
	return true;
}

bool CreatureDatabase::loadFromJSON(const FileName& filename, bool standard, wxString& error, wxArrayString& warnings)
{
	std::ifstream file(filename.GetFullPath().mb_str());
	if(!file.is_open()) {
		error = "Couldn't open file \"" + filename.GetFullName() + "\".";
		return false;
	}

	json jsonData;
	try {
		file >> jsonData;
	} catch (const std::exception& e) {
		error = "Invalid JSON format: " + wxString(e.what());
		return false;
	}

	if(!jsonData.is_object()) {
		error = "Invalid monster JSON format";
		return false;
	}

	for(auto it = jsonData.begin(); it != jsonData.end(); ++it) {
		std::string creatureName = it.key();
		const json& creatureData = it.value();

		CreatureType* creatureType = newd CreatureType();
		creatureType->name = creatureName;
		creatureType->isNpc = false;
		creatureType->standard = standard;

		if(creatureData.contains("outfit") && creatureData["outfit"].is_object()) {
			const json& outfit = creatureData["outfit"];
			if(outfit.contains("lookType")) creatureType->outfit.lookType = outfit["lookType"];
			if(outfit.contains("lookItem")) creatureType->outfit.lookItem = outfit["lookItem"];
			if(outfit.contains("lookMount")) creatureType->outfit.lookMount = outfit["lookMount"];
			if(outfit.contains("lookAddon")) creatureType->outfit.lookAddon = outfit["lookAddon"];
			if(outfit.contains("lookHead")) creatureType->outfit.lookHead = outfit["lookHead"];
			if(outfit.contains("lookBody")) creatureType->outfit.lookBody = outfit["lookBody"];
			if(outfit.contains("lookLegs")) creatureType->outfit.lookLegs = outfit["lookLegs"];
			if(outfit.contains("lookFeet")) creatureType->outfit.lookFeet = outfit["lookFeet"];
		}

		if((*this)[creatureType->name]) {
			warnings.push_back("Duplicate creature name \"" + wxstr(creatureType->name) + "\"! Discarding...");
			delete creatureType;
			continue;
		}

		creature_map[as_lower_str(creatureType->name)] = creatureType;
		Tileset* tileSet = g_materials.tilesets["Others"];
		ASSERT(tileSet != nullptr);

		Brush* brush = newd CreatureBrush(creatureType);
		g_brushes.addBrush(brush);

		TilesetCategory* tileSetCategory = tileSet->getCategory(TILESET_CREATURE);
		tileSetCategory->brushlist.push_back(brush);
	}
	return true;
}

