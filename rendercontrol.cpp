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

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "primitives.h"
#include "rendercontrol.h"
#include "ui/Error.h"
#include "Options.h"
#include "uniformbuffer.h"
#include "shadowconfig.h"
#include "shaders/ChunkShader.h"
#include "shaders/adddynamicshadow.h"
#include "shaders/DeferredLighting.h"
#include "shaders/addpointlight.h"
#include "shaders/AnimationShader.h"
#include "shaders/TranspShader.h"
#include "shaders/addpointshadow.h"
#include "shaders/addlocalfog.h"
#include "shaders/addssao.h"
#include "render.h"
#include "Options.h"
#include "shadowrender.h"
#include "chunk.h"
#include "player.h"
#include "monsters.h"
#include "timemeasure.h"
#include "shaders/skybox.h"
#include "shapes/quadstage1.h"
#include "otherplayers.h"
#include "textures.h"
#include "Map.h"
#include "DrawTexture.h"
#include "worsttime.h"

#define NELEM(x) (sizeof x / sizeof x[0])

RenderControl::RenderControl() {
	fboName = 0;
	fDepthBuffer = 0;
	fDiffuseTexture = 0; fPositionTexture = 0; fNormalsTexture = 0; fBlendTexture = 0; fLightsTexture = 0;
	fAnimation = 0; fShader = 0;
	fSkyBox = 0;
}

RenderControl::~RenderControl() {
	this->FreeFBO();
	if (fDepthBuffer != 0) {
		glDeleteTextures(1, &fDepthBuffer);
		glDeleteTextures(1, &fDiffuseTexture);
		glDeleteTextures(1, &fPositionTexture);
		glDeleteTextures(1, &fNormalsTexture);
		glDeleteTextures(1, &fBlendTexture);
		glDeleteTextures(1, &fLightsTexture);
	}
}

void RenderControl::FreeFBO() {
	if (fboName != 0) {
		glDeleteFramebuffers(1, &fboName);
	}
	fboName = 0;
}

void RenderControl::Init() {
	fAddDynamicShadow.reset(new AddDynamicShadow);
	fAddDynamicShadow->Init();
	fShader = ChunkShader::Make(); // Singleton
	fAnimation = AnimationShader::Make(); // Singleton
	fSkyBox.reset(new SkyBox);
	fSkyBox->Init();
	fDeferredLighting.reset(new DeferredLighting);
	fDeferredLighting->Init();
	fAddPointLight.reset(new AddPointLight);
	fAddPointLight->Init();
	fAddPointShadow.reset(new AddPointShadow);
	fAddPointShadow->Init();
	fAddLocalFog.reset(new AddLocalFog);
	fAddLocalFog->Init();
	fAddSSAO.reset(new AddSSAO);
	fAddSSAO->Init();

	fMainUserInterface.Init();

	if (Options::fgOptions.fDynamicShadows) {
		fShadowRender.reset(new ShadowRender(DYNAMIC_SHADOW_MAP_SIZE,DYNAMIC_SHADOW_MAP_SIZE));
		fShadowRender->Init();
	} else if (Options::fgOptions.fStaticShadows) {
		fShadowRender.reset(new ShadowRender(STATIC_SHADOW_MAP_SIZE,STATIC_SHADOW_MAP_SIZE));
		fShadowRender->Init();
	}
}

void RenderControl::Resize(GLsizei width, GLsizei height) {
	fMainUserInterface.Resize(width, height);
	fWidth = width; fHeight = height;
	// This function can be called repeatedly, when window size changes.
	this->FreeFBO();

	glGenFramebuffers(1, &fboName);

	if (fDepthBuffer == 0) {
		glGenRenderbuffers(1, &fDepthBuffer);
		glGenTextures(1, &fDiffuseTexture); gDebugTextures.push_back(fDiffuseTexture); // Add this texture to the debugging list of textures
		glGenTextures(1, &fPositionTexture); gDebugTextures.push_back(fPositionTexture);
		glGenTextures(1, &fNormalsTexture); gDebugTextures.push_back(fNormalsTexture);
		glGenTextures(1, &fBlendTexture); gDebugTextures.push_back(fBlendTexture);
		glGenTextures(1, &fLightsTexture); gDebugTextures.push_back(fLightsTexture);
	}

	// Generate and bind the texture depth information
	glBindRenderbuffer(GL_RENDERBUFFER, fDepthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

	// Generate and bind the texture for diffuse
	glBindTexture(GL_TEXTURE_2D, fDiffuseTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Generate and bind the texture for positions
	glBindTexture(GL_TEXTURE_2D, fPositionTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Generate and bind the texture for normals
	glBindTexture(GL_TEXTURE_2D, fNormalsTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Generate and bind the texture for blending data
	glBindTexture(GL_TEXTURE_2D, fBlendTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Generate and bind the texture for lights
	glBindTexture(GL_TEXTURE_2D, fLightsTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Bind the FBO so that the next operations will be bound to it.
	glBindFramebuffer(GL_FRAMEBUFFER, fboName);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fDepthBuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fDiffuseTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, fPositionTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, fNormalsTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, fBlendTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, fLightsTexture, 0);

	glReadBuffer(GL_NONE);
	GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
		ErrorDialog("RenderControl::Init: FrameBuffer incomplete: %s (0x%x)\n", FrameBufferError(fboStatus), fboStatus);
		exit(1);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	checkError("RenderControl::Init");
}

enum { STENCIL_NOSKY = 1 };

void RenderControl::Draw(Object *selectedObject, bool underWater, bool thirdPersonView, Object *fSelectedObject, bool showMap, int mapWidth) {
	if (!gPlayer.BelowGround())
		this->ComputeShadowMap();

	if (gShowFramework)
		glPolygonMode(GL_FRONT, GL_LINE);
	// Create all bitmaps setup in the frame buffer. This is all stage one shaders.
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboName);
	drawClearFBO();
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, STENCIL_NOSKY, STENCIL_NOSKY); // Set bit for no sky
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE); // Replace bits when something is drawn
	gDrawObjectList.clear();
	drawNonTransparentLandscape();
	if (thirdPersonView)
		drawPlayer(); // Draw self, but not if camera is too close
	drawOtherPlayers();
	drawMonsters();
	drawTransparentLandscape();
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); // Make stencil read-only
	drawSkyBox();

	// Apply deferred shader filters.
	glStencilFunc(GL_EQUAL, STENCIL_NOSKY, STENCIL_NOSKY); // Only execute when no sky and no UI
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	if ((Options::fgOptions.fDynamicShadows || Options::fgOptions.fStaticShadows) && !gPlayer.BelowGround())
		drawDynamicShadows();
	drawPointLights();
	// drawSSAO(); // TODO: Not good enough yet.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Restore default
	glDisable(GL_BLEND);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Frame buffer has done its job now, results are in the diffuse image.
	glDisable(GL_STENCIL_TEST);

	// At this point, there is no depth buffer representing the geometry and no stencil. They are only valid inside the FBO.
	// However, there is the global stencil buffer. Use it to set the bit for the UI, but it is not possible to reconstruct the
	// bit for the sky.
	drawClear();

	// Draw the main result to the screen. TODO: It would be possible to have the deferred rendering update the depth buffer!
	if (gShowFramework)
		glPolygonMode(GL_FRONT, GL_FILL);
	drawDeferredLighting(underWater, Options::fgOptions.fWhitePoint);

	// Do some post processing
	if (!underWater)
		drawPointShadows();
	if (fSelectedObject)
		drawSelection(fSelectedObject->GetPosition());
	if (!underWater)
		drawLocalFog();
	drawUI(); // This time, it will stay on the screen.
	if (showMap)
		drawMap(mapWidth);
}

void RenderControl::drawClear() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
}

void RenderControl::drawClearFBO(void) {
	GLenum windowBuffClear[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
	glDrawBuffers(1, windowBuffClear); // diffuse buffer
	glClearColor(0.584f, 0.620f, 0.698f, 1.0f); // Gray background, will only be used where there is no sky box.
	glClear(GL_COLOR_BUFFER_BIT);

	glDrawBuffers(4, windowBuffClear+1); // Select all buffers but the diffuse buffer
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Set everything to zero.
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
}

void RenderControl::drawNonTransparentLandscape(void) {
	static TimeMeasure tm("Landsc");
	tm.Start();
	GLenum windowBuffOpaque[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_NONE };
	glDrawBuffers(4, windowBuffOpaque); // Nothing is transparent here, do not produce any blending data on the 4:th render target.
	fShader->EnableProgram(); // Get program back again after the skybox.
	DrawLandscape(fShader, DL_NoTransparent);
	tm.Stop();
}

void RenderControl::drawTransparentLandscape(void) {
	static TimeMeasure tm("Transp");
	tm.Start();
	GLenum windowBuffOpaque[] = { GL_COLOR_ATTACHMENT3 }; // Only draw to the transparent buffer
	glDrawBuffers(1, windowBuffOpaque);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Use alpha 1 for source channel, as the colors are premultiplied by the alpha.
	glDepthMask(GL_FALSE);                // The depth buffer shall not be updated, or some transparent blocks behind each other will not be shown.
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, fPositionTexture); // Position data is used by the transparent shader for distance computation.
	glActiveTexture(GL_TEXTURE0);
	gTranspShader.EnableProgram();
	gTranspShader.View(float(gCurrentFrameTime));
	DrawLandscape(&gTranspShader, DL_OnlyTransparent);
	gTranspShader.DisableProgram();
	glDepthMask(GL_TRUE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Restore to default
	glDisable(GL_BLEND);
	tm.Stop();
}

void RenderControl::drawMonsters(void) {
	static TimeMeasure tm("Monst");
	tm.Start();
	GLenum windowBuffOpaque[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, windowBuffOpaque); // Nothing is transparent here, do not produce any blending data on the 4:th render target.
	fAnimation->EnableProgram();
	gMonsters.RenderMonsters(fAnimation, false, false);
	fAnimation->DisableProgram();
	tm.Stop();
}

void RenderControl::drawPlayer(void) {
	GLenum windowBuffOpaque[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, windowBuffOpaque); // Nothing is transparent here, do not produce any blending data on the 4:th render target.
	fAnimation->EnableProgram();
	gPlayer.Draw(fAnimation, fShader, false);
	fAnimation->DisableProgram();
}

void RenderControl::drawDynamicShadows() {
	static TimeMeasure tm("Dynshdws");
	tm.Start();
	GLenum windowBuffers[] = { GL_COLOR_ATTACHMENT4 };
	glDrawBuffers(1, windowBuffers); // Nothing is transparent here, do not produce any blending data on the 4:th render target.
	if ((Options::fgOptions.fDynamicShadows || Options::fgOptions.fStaticShadows) && fShadowRender) {
		glActiveTexture(GL_TEXTURE4);
		fShadowRender->BindTexture();
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, fNormalsTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, fPositionTexture);
		glActiveTexture(GL_TEXTURE0); // Need to restore it or everything will break.
		glBindTexture(GL_TEXTURE_1D, GameTexture::PoissonDisk);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		fAddDynamicShadow->Draw(fShadowRender->GetProViewMatrix());
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}
	tm.Stop();
}

void RenderControl::drawDeferredLighting(bool underWater, float whitepoint) {
	static TimeMeasure tm("Deferred");
	tm.Start();
	// Prepare the input images needed
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_1D, GameTexture::PoissonDisk);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, fLightsTexture);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, fBlendTexture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, fNormalsTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, fPositionTexture);
	glActiveTexture(GL_TEXTURE0); // Need to restore it or everything will break.
	glBindTexture(GL_TEXTURE_2D, fDiffuseTexture);

	fDeferredLighting->EnableProgram();
	fDeferredLighting->InsideTeleport(gMode.Get() == GameMode::TELEPORT);
	fDeferredLighting->PlayerDead(gPlayer.IsDead());
	fDeferredLighting->InWater(underWater);
	fDeferredLighting->SetWhitePoint(whitepoint);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	fDeferredLighting->Draw();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	fDeferredLighting->DisableProgram();
	tm.Stop();
}

void RenderControl::drawPointLights(void) {
	static TimeMeasure tm("Pntlght");
	tm.Start();
	GLenum windowBuffers[] = { GL_COLOR_ATTACHMENT4 };
	glDrawBuffers(1, windowBuffers); // Draw only to the light buffer
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, fNormalsTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, fPositionTexture);
	glActiveTexture(GL_TEXTURE0); // Need to restore it or everything will break.
	glBindTexture(GL_TEXTURE_2D, fDiffuseTexture);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	// Iterate through all the objects, and add lights for some of them
	for (auto it = gDrawObjectList.begin(); it != gDrawObjectList.end(); it++) {
		switch(it->type) {
		case BT_Lamp1:
			// The coordinate is the base of the lamp. Move light souce a little upwards, to middle of lamp
			fAddPointLight->Draw(it->pos+glm::vec3(0, 0.2f, 0), LAMP1_DIST);
			break;
		case BT_Lamp2:
			// The coordinate is the base of the lamp. Move light souce a little upwards, to middle of lamp
			fAddPointLight->Draw(it->pos+glm::vec3(0, 0.3f, 0), LAMP2_DIST);
			break;
		case BT_Treasure:
			if (Options::fgOptions.fPerformance > 1) // Inhibit this for the low performance
				fAddPointLight->Draw(it->pos, 1.5f);
			break;
		case BT_Quest:
			if (Options::fgOptions.fPerformance > 1) // Inhibit this for the low performance
				fAddPointLight->Draw(it->pos, 1.5f);
			break;
		case BT_Teleport:
			if (Options::fgOptions.fPerformance > 1) // Inhibit this for the low performance
				fAddPointLight->Draw(it->pos, 3.0f);
			break;
		}
	}
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	tm.Stop();
}

void RenderControl::drawSSAO(void) {
	static TimeMeasure tm("SSAO ");
	tm.Start();
	GLenum windowBuffers[] = { GL_COLOR_ATTACHMENT4 };
	glDrawBuffers(1, windowBuffers); // Nothing is transparent here, do not produce any blending data on the 4:th render target.
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, fNormalsTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, fPositionTexture);
	glActiveTexture(GL_TEXTURE0); // Need to restore it or everything will break.
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	fAddSSAO->Draw();
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	tm.Stop();
}

void RenderControl::drawPointShadows(void) {
	static TimeMeasure tm("Pntshdw");
	tm.Start();
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, fNormalsTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, fPositionTexture);
	glActiveTexture(GL_TEXTURE0); // Need to restore it or everything will break.
	glm::vec4 *list = gShadows.GetList();
	int count = gShadows.GetCount();
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	for (int i=0; i < count; i++) {
		fAddPointShadow->Draw(list[i]);
	}
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Restore to default
	glDisable(GL_BLEND);
	tm.Stop();
}

void RenderControl::drawLocalFog(void) {
	static TimeMeasure tm("LclFog");
	tm.Start();
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, fPositionTexture);
	glActiveTexture(GL_TEXTURE0); // Need to restore it or everything will break.
	glm::vec4 *list = gFogs.GetList();
	int count = gFogs.GetCount();
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	for (int i=0; i < count; i++) {
		// Extract the integral part (fog strength), and the fractional part (ambient).
		float ambient = modff(list[i].w, &list[i].w);   // TODO: Ugly, will modify original list
		fAddLocalFog->Draw(list[i], ambient);
	}
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	tm.Stop();
}

void RenderControl::ComputeShadowMap() {
	if (Options::fgOptions.fDynamicShadows && fShadowRender) {
		static TimeMeasure tm("ShdwDyn");
		tm.Start();
		fShadowRender->Render(352,160);
		tm.Stop();
	}
	if (Options::fgOptions.fStaticShadows && fShadowRender) {
		static TimeMeasure tm("ShdwStat");
		tm.Start();
		static ChunkCoord prev = {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF};
		static double prevTime;
		ChunkCoord curr;
		gPlayer.GetChunkCoord(&curr);
		if (curr.x != prev.x || curr.y != prev.y || curr.z != prev.z || gCurrentFrameTime-1.0 > prevTime) {
			// Only update the shadowmap when the player move to another chunk, or after a time out
			fShadowRender->Render(224,160);
			prev = curr;
			prevTime = gCurrentFrameTime;
		}
		tm.Stop();
	}
}

void RenderControl::drawOtherPlayers(void) {
	gOtherPlayers.RenderPlayers(false);
}

void RenderControl::drawSelection(const glm::vec3 &coord) {
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, fNormalsTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, fPositionTexture);
	glActiveTexture(GL_TEXTURE0); // Need to restore it or everything will break.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glm::vec4 point = glm::vec4(coord, 2.0f); // 2 blocks wide selection circle
	fAddPointShadow->Draw(point, true);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

void RenderControl::drawSkyBox(void) {
	if (!gPlayer.IsDead()) {
		// If the player is dead, he will get a gray sky.
		// glDisable(GL_DEPTH_TEST);
		static TimeMeasure tm("SkyBox");
		tm.Start();
		GLenum windowBuffOpaque[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_NONE };
		glDrawBuffers(4, windowBuffOpaque); // Nothing is transparent here, do not produce any blending data on the 4:th render target.
		glDepthRange(1, 1); // This will move the sky box to the far plane, exactly
		glDepthFunc(GL_LEQUAL); // Seems to be needed, or depth value 1.0 will not be shown.
		glDisable(GL_CULL_FACE); // Skybox is drawn with the wrong culling order.
		fSkyBox->Draw();
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LESS);
		glDepthRange(0, 1);
		glEnable(GL_DEPTH_TEST);
		tm.Stop();
	}
}

void RenderControl::drawUI(void) {
	static WorstTime tm("MainUI");
	tm.Start();
	fMainUserInterface.Draw();
	tm.Stop();
}

void RenderControl::drawMap(int mapWidth) {
	// Very inefficient algorithm, computing the map every frame.
	// The FBO is overwritten for this.
	std::unique_ptr<Map> map(new Map);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboName);
	GLenum windows[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, windows); // Only diffuse color needed
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	fShader->EnableProgram();
	map->Create(fAnimation, fShader, _angleHor, mapWidth);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, fDiffuseTexture); // Override
	map->Draw(0.6f);
}

float RenderControl::ComputeAverageLuminance(void) {
	const int NUM = NELEM(gPoissonDisk);
	float lightSamples[NUM];
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fboName);
	glReadBuffer(GL_COLOR_ATTACHMENT4); // The light map
	for (unsigned i=0; i<NELEM(gPoissonDisk); i++) {
		GLint x = fWidth * (1.0f + gPoissonDisk[i].x) / 2.0f;
		GLint y = fHeight * (1.0f + gPoissonDisk[i].y) / 2.0f;
		glReadPixels(x, y, 1, 1, GL_RED, GL_FLOAT, &lightSamples[i]);
	}

	glReadBuffer(GL_COLOR_ATTACHMENT0); // The diffuse map
	float sum = 0.0f;
	float weight = 0.0f;
	for (unsigned i=0; i<NELEM(gPoissonDisk); i++) {
		GLint x = fWidth * (1.0f + gPoissonDisk[i].x) / 2.0f;
		GLint y = fHeight * (1.0f + gPoissonDisk[i].y) / 2.0f;
		glm::vec3 diffuse;
		glReadPixels(x, y, 1, 1, GL_RGB, GL_FLOAT, &diffuse);
		float luminance = 0.2126f*diffuse.r + 0.7152f*diffuse.g + 0.0722*diffuse.b;
		sum += luminance * lightSamples[i];
		weight += 1.0f;
	}
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	return sum / weight;
}

Rocket::Core::Context *RenderControl::GetRocketContext(void) {
	return fMainUserInterface.GetRocketContext();
}
