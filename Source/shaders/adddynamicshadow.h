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
// Use the shadow map and compute lighting effects.
//

#include "shader.h"

class AddDynamicShadow : public ShaderBase {
public:
	AddDynamicShadow();

	void Init(void);

	void Draw(const glm::mat4 &);
private:
	virtual void GetLocations(void);
	GLint fShadowMapMatrixIdx;
};
