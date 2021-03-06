// Copyright 2012 The Ephenation Authors
//
// This file is part of Ephenation.
//
// Ephenation is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3.
//
// Ephenation is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Ephenation.  If not, see <http://www.gnu.org/licenses/>.
//

#pragma once

//
// Manages other players that we know of. The client will only know of
// near players, which may need to be drawn.
//
// TODO: The Monsters class is very similar, with too much duplication
//

#include <map>
#include <string>

class HealthBar;
class AnimationShader;

namespace Model {

class OtherPlayers {
private:
	struct OneOtherPlayer : public Object {
		unsigned long id;
		signed long long x;
		signed long long y;
		signed long long z;
		bool ingame;
		unsigned char hp;
		unsigned int level;
		float fDir; // The direction the player is facing, in degrees
		double fUpdateTime; // The last time when this player was updated from the server.
		double lastTimeMoved; // The time when the monster last moved
		std::string playerName;
		virtual unsigned long GetId() const { return this->id; }
		virtual int GetType() const { return ObjTypePlayer; }
		virtual int GetLevel() const { return this->level; }
		virtual glm::vec3 GetPosition() const;
		virtual glm::vec3 GetSelectionColor() const; // The color to draw on the ground when object selected
		virtual bool IsDead(void) const { return hp == 0; }
		virtual void RenderHealthBar(HealthBar *, float angle) const;
		virtual bool InGame(void) const { return ingame;}
	};
	std::map<unsigned long, OneOtherPlayer> fPlayers;
public:
	void Cleanup(void);
	void SetPlayer(unsigned long id, unsigned char hp, unsigned int level, signed long long x, signed long long y, signed long long z, float dir);
	void SetPlayerName(unsigned long uid, const char *name, int n, int adminLevel);
	// draw all near players.
	// 'selectionMode' is true when actively selecting players.
	void RenderPlayers(AnimationShader *animShader, bool selectionMode) const;
	// Render the stats of players. This has to be done after the deferred shader.
	// 'angle' is the viewing angle, used to draw player data rotated correctly to the camera.
	void RenderPlayerStats(HealthBar *hb, float angle) const;
	void RenderMinimap(const glm::mat4 &miniMap, HealthBar *hb) const;
};

extern OtherPlayers gOtherPlayers;

}

