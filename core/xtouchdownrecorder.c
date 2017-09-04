/*
    XTouchDownRecorder
    Copyright (C) 2017  Wei Shuai <cpuwolf@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <math.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include <XPLMPlugin.h>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMDataAccess.h>
#include <XPLMUtilities.h>
#include <XPLMProcessing.h>
#include <XPLMMenus.h>
#include <XPLMNavigation.h>

static XPLMWindowID g_win = NULL;

static float flightcb(float inElapsedSinceLastCall,
	float inElapsedTimeSinceLastFlightLoop, int inCounter,
	void *inRefcon)
{
	/*if (!g_win)
		g_win = XPLMCreateWindow(winPosX, winPosY,
			winPosX + WINDOW_WIDTH, winPosY - WINDOW_HEIGHT,
			1, drawWindowCallback, keyboardCallback,
			mouseCallback, NULL);*/
	return 0.01f;
}

PLUGIN_API int XPluginStart(char * outName, char * outSig, char * outDesc) {
    /* Plugin details */
	strcpy(outName, "XTouchDownRecorder V4");
	strcpy(outSig, "cpuwolf.xtouchdownrecorder");
	strcpy(outDesc, "More information https://github.com/cpuwolf");

    // You probably want this on
	XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

	/* register loopback in 0.01s */
	XPLMRegisterFlightLoopCallback(flightcb, 0.01f, NULL);

	return 1;
}

PLUGIN_API void	XPluginStop(void) {
}

PLUGIN_API void XPluginDisable(void) {
}

PLUGIN_API int XPluginEnable(void) {
	return 1;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inForm, int inMessage, void * inParam) {
}
