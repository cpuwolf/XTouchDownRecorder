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

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <Carbon/Carbon.h>
#else
#include <stdlib.h>
#include <GL/gl.h>
#endif

#define MAX_TABLE_ELEMENTS 500
#define CURVE_LEN 2

static XPLMDataRef gearFRef,gForceRef,vertSpeedRef,pitchRef,elevatorRef,engRef,aglRef,tmRef;

static float * g_pbuffer = NULL;
static unsigned int g_start = 0;
static unsigned int g_end = 0;
static unsigned int g_size = 0;

#define BUFFER_EMPTY() (g_size==0)

/* set value firstly, then increase index */
#define BUFFER_INSERT_BACK() if(g_end < MAX_TABLE_ELEMENTS) {\
								g_end++;} else {\
									g_end = 0;\
								} \
								if(g_size < MAX_TABLE_ELEMENTS) {\
									g_size++;\
								} else {\
									if(g_start < MAX_TABLE_ELEMENTS) {g_start++;} else {g_start = 0;}\
								}

#define BUFFER_GO_START(idx,tmp_count)  tmp_count=g_size; idx=g_start;

#define BUFFER_GO_IS_END(idx,tmp_count)  (tmp_count<=0)

#define BUFFER_GO_NEXT(idx,tmp_count) if(idx < MAX_TABLE_ELEMENTS) {\
                                idx++;} else {\
                                    idx = 0;\
                                } tmp_count--;\

enum
{
	TOUCHDOWN_VS_IDX = 0,
	TOUCHDOWN_G_IDX,
	TOUCHDOWN_PCH_IDX,
	TOUCHDOWN_AIR_IDX,
	TOUCHDOWN_ELEV_IDX,
	TOUCHDOWN_ENG_IDX,
	TOUCHDOWN_AGL_IDX,
	TOUCHDOWN_TM_IDX,
	MAX_TOUCHDOWN_IDX
};

static float *touchdown_vs_table;
static float *touchdown_g_table;
static float *touchdown_pch_table;
static BOOL *touchdown_air_table;
static float *touchdown_elev_table;
static float *touchdown_eng_table;
static float *touchdown_agl_table;
static float *touchdown_tm_table;

static float lastVS = 1.0;
static float lastG = 1.0;
static float lastPitch = 1.0;
static BOOL lastAir = FALSE;
static float lastElev = 0.0;
static float lastEng = 0.0;
static float lastAgl = 0.0;
static float lastTm = 0.0;

#define _TD_CHART_HEIGHT 200
#define _TD_CHART_WIDTH (MAX_TABLE_ELEMENTS*CURVE_LEN)
static XPLMWindowID g_win = NULL;
static int g_winposx = 0;
static int g_winposy = 500;

static BOOL collect_touchdown_data = TRUE;
static unsigned int show_touchdown_counter = 3;


static char *landingString[128];
static BOOL IsLogWritten = TRUE;
static BOOL IsTouchDown = FALSE;

static BOOL check_ground(float n)
{
    if ( 0.0 != n ) {
        return TRUE;
        /*-- LAND */
    }

    return FALSE;
    /*-- AIR*/
}

static int is_on_ground()
{
    return check_ground(XPLMGetDataf(gearFRef));
}


void collect_flight_data()
{
	float engtb[4];
	int iw = g_end;

    lastVS = XPLMGetDataf(vertSpeedRef);
    lastG = XPLMGetDataf(gForceRef);
    lastPitch = XPLMGetDataf(pitchRef);
    lastAir = check_ground(XPLMGetDataf(gearFRef));
    lastElev = XPLMGetDataf(elevatorRef);
    lastAgl = XPLMGetDataf(aglRef);
    lastTm = XPLMGetDataf(tmRef);
    XPLMGetDatavf(engRef,engtb,0,4);
    lastEng = engtb[0];

    /*-- fill the table */
    touchdown_vs_table[iw] = lastVS;
    touchdown_g_table[iw] = lastG;
    touchdown_pch_table[iw] = lastPitch;
    touchdown_air_table[iw] = lastAir;
    touchdown_elev_table[iw] = lastElev;
    touchdown_eng_table[iw] = lastEng;
    touchdown_agl_table[iw] = lastAgl;
    touchdown_tm_table[iw] = lastTm;
    BUFFER_INSERT_BACK();

}

static void keycb(XPLMWindowID inWindowID, char inKey, XPLMKeyFlags inFlags,
                   char inVirtualKey, void *inRefcon, int losingFocus)
{

}
static int mousecb(XPLMWindowID inWindowID, int x, int y,
                   XPLMMouseStatus inMouse, void *inRefcon)
{
	return 1;
}
static draw_line(float r,float g, float b, float alpha, float width, int x1, int y1, int x2, int y2)
{
	glDisable(GL_TEXTURE_2D);
	glColor3f(r, g, b);
	glBegin(GL_LINES);
	glVertex2i(x1, y1);
	glVertex2i(x2, y2);
	glEnd();
	glEnable(GL_TEXTURE_2D);
}
static void drawcb(XPLMWindowID inWindowID, void *inRefcon)
{
    /*-- draw background first*/
    int x, y, x_tmp;
    int left, top, right, bottom;
    float color[] = { 1.0, 1.0, 1.0 };
    char text_buf[100];
            
    XPLMGetWindowGeometry(inWindowID, &left, &top, &right, &bottom);
    XPLMDrawTranslucentDarkBox(left, top, right, bottom);

    x = left;
    y = bottom;

    /*-- draw center line*/
    draw_line(0, 0, 0, 1, 3,x, y + (_TD_CHART_HEIGHT / 2), x + (MAX_TABLE_ELEMENTS * 2), y + (_TD_CHART_HEIGHT / 2));

    /*-- draw horizontal axis*/
    int k,tmpc;
    x_tmp = x;
    BUFFER_GO_START(k,tmpc);
    float last_tm_recorded = touchdown_tm_table[k];
    float a;
    while(!BUFFER_GO_IS_END(k,tmpc)) {
        /*-- second axis*/
        a = touchdown_tm_table[k];
        if (a - last_tm_recorded >= 1.0) {
            /*-- 1 second*/
            draw_line(0,0,0,1,3,x_tmp, y, x_tmp, y + _TD_CHART_HEIGHT);
            last_tm_recorded = touchdown_tm_table[k];
        }
        x_tmp = x_tmp + 2;
        BUFFER_GO_NEXT(k,tmpc);
    }
    
    /*-- title*/
    color[0] = 1.0;
    color[1] = 1.0;
    color[2] = 1.0;
    XPLMDrawString(color, x + 5, y + _TD_CHART_HEIGHT - 15, "TouchDownRecorder V3.0 by cpuwolf", NULL, xplmFont_Basic);

    int x_text = x + 5;
    int y_text = y + 8;
    /*-- draw touch point vertical lines*/
    x_tmp = x;
    memset(landingString,0,sizeof(landingString));
    BUFFER_GO_START(k,tmpc);
    BOOL last_air_recorded = touchdown_air_table[k];
    BOOL b;
    while(!BUFFER_GO_IS_END(k,tmpc)) {
        b = touchdown_air_table[k];
        if(b != last_air_recorded) {
            if(b) {
                IsTouchDown = TRUE;
                /*-- draw vertical line*/
                draw_line(1,1,1,1,3,x_tmp, y + (_TD_CHART_HEIGHT/4), x_tmp, y + (_TD_CHART_HEIGHT*3/4));
                /*-- print text*/
                float landingVS = touchdown_vs_table[k];
                float landingG = touchdown_g_table[k];
                float landingPitch = touchdown_pch_table[k];
                char *text_to_print = text_buf;
                sprintf(text_to_print,"%.02f fpm %.02f G %.02f Degree", landingVS, landingG, landingPitch);
                strcat(landingString,text_to_print);
                int width_text_to_print = abs(XPLMMeasureString(xplmFont_Basic, text_to_print, strlen(text_to_print)));
                XPLMDrawString(color, x_text, y_text, text_to_print, NULL, xplmFont_Basic);
                x_text = x_text + width_text_to_print;
            }
        }
        x_tmp = x_tmp + 2;
        last_air_recorded = b;
        BUFFER_GO_NEXT(k,tmpc);
    }
/*
    -- now draw the chart line green
    max_vs_axis = 1000.0
    max_vs_recorded = get_max_val(touchdown_vs_table)
    text_to_p = "Max "..string.format("%.02f", max_vs_recorded).."fpm "
    x_text = draw_curve(touchdown_vs_table, 0,1,0, text_to_p, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_vs_axis, max_vs_recorded)

    -- now draw the chart line red
    max_g_axis = 2.0
    max_g_recorded = get_max_val(touchdown_g_table)
    text_to_p = "Max "..string.format("%.02f", max_g_recorded).."G "
    x_text = draw_curve(touchdown_g_table, 1,0.68,0.78, text_to_p, x_text, y_text, x, y, x, y, max_g_axis, max_g_recorded)

    -- now draw the chart line light blue
    max_pch_axis = 14.0
    max_pch_recorded = get_max_val(touchdown_pch_table)
    text_to_p = "Max pitch "..string.format("%.02f", max_pch_recorded).."Degree "
    x_text = draw_curve(touchdown_pch_table, 0.6,0.85,0.87, text_to_p, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_pch_axis, max_pch_recorded)

    -- now draw the chart line orange
    max_elev_axis = 2.0
    max_elev_recorded = get_max_val(touchdown_elev_table)
    text_to_p = "Max elevator "..string.format("%.02f", max_elev_recorded*100.0).."% "
    x_text = draw_curve(touchdown_elev_table, 1.0,0.49,0.15, text_to_p, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_elev_axis, max_elev_recorded)

    -- now draw the chart line yellow
    max_eng_axis = 2.0
    max_eng_recorded = get_max_val(touchdown_eng_table)
    text_to_p = "Max eng "..string.format("%.02f", max_eng_recorded*100.0).."% "
    x_text = draw_curve(touchdown_eng_table, 1.0,1.0,0.0, text_to_p, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_eng_axis, max_eng_recorded)

    -- now draw the chart line red
    max_agl_axis = 6.0
    max_agl_recorded = get_max_val(touchdown_agl_table)
    text_to_p = "Max AGL "..string.format("%.02f", max_agl_recorded).."M "
    x_text = draw_curve(touchdown_agl_table, 1.0,0.1,0.1, text_to_p, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_agl_axis, max_agl_recorded)
*/
    /*-- draw close button on top-right*/
	glDisable(GL_TEXTURE_2D);
	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_LINES);
	glVertex2i(right - 1, top - 1);
	glVertex2i(right - 10, top - 10);
	glVertex2i(right - 10, top - 1);
	glVertex2i(right - 1, top - 10);
	glEnd();
	glEnable(GL_TEXTURE_2D);
}

static float flightcb(float inElapsedSinceLastCall,
	float inElapsedTimeSinceLastFlightLoop, int inCounter,
	void *inRefcon)
{
	if (!g_win)
		g_win = XPLMCreateWindow(g_winposx, g_winposy,
			g_winposx + _TD_CHART_WIDTH, g_winposy - _TD_CHART_HEIGHT,
			1, drawcb, keycb,
			mousecb, NULL);

	if(collect_touchdown_data) {
        collect_flight_data();
    }
    /*-- dont draw when the function isn't wanted*/
    if(show_touchdown_counter <= 0) {
		XPLMSetWindowIsVisible(g_win, 0);
		goto flightclean;
	} else {
		XPLMSetWindowIsVisible(g_win, 1);
    }

flightclean:
	return -1;
}

PLUGIN_API int XPluginStart(char * outName, char * outSig, char * outDesc) {
    /* Plugin details */
	strcpy(outName, "XTouchDownRecorder V4");
	strcpy(outSig, "cpuwolf.xtouchdownrecorder");
	strcpy(outDesc, "More information https://github.com/cpuwolf");

	g_pbuffer = malloc(MAX_TABLE_ELEMENTS * sizeof(float) * MAX_TOUCHDOWN_IDX);
	if(!g_pbuffer) {
		XPLMDebugString("malloc error!");
		return 0;
	}
	touchdown_vs_table = g_pbuffer + (TOUCHDOWN_VS_IDX * MAX_TABLE_ELEMENTS);
	touchdown_g_table  = g_pbuffer + (TOUCHDOWN_G_IDX * MAX_TABLE_ELEMENTS);
	touchdown_pch_table = g_pbuffer + (TOUCHDOWN_PCH_IDX * MAX_TABLE_ELEMENTS);
	touchdown_air_table = (BOOL *)(g_pbuffer + (TOUCHDOWN_AIR_IDX * MAX_TABLE_ELEMENTS));
	touchdown_elev_table = g_pbuffer + (TOUCHDOWN_ELEV_IDX * MAX_TABLE_ELEMENTS);
	touchdown_eng_table = g_pbuffer + (TOUCHDOWN_ENG_IDX * MAX_TABLE_ELEMENTS);
	touchdown_agl_table = g_pbuffer + (TOUCHDOWN_AGL_IDX * MAX_TABLE_ELEMENTS);
	touchdown_tm_table = g_pbuffer + (TOUCHDOWN_TM_IDX * MAX_TABLE_ELEMENTS);

	gearFRef = XPLMFindDataRef("sim/flightmodel/forces/fnrml_gear");
	gForceRef = XPLMFindDataRef("sim/flightmodel2/misc/gforce_normal");
	vertSpeedRef = XPLMFindDataRef("sim/flightmodel/position/vh_ind_fpm2");
	pitchRef = XPLMFindDataRef("sim/flightmodel/position/theta");
	elevatorRef = XPLMFindDataRef("sim/flightmodel2/controls/pitch_ratio");
	engRef = XPLMFindDataRef("sim/flightmodel2/engines/throttle_used_ratio");
	aglRef = XPLMFindDataRef("sim/flightmodel/position/y_agl");
	tmRef = XPLMFindDataRef("sim/time/total_flight_time_sec");

    // You probably want this on
	XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

	/* register loopback in 0.01s */
	XPLMRegisterFlightLoopCallback(flightcb, -1, NULL);

	return 1;
}

PLUGIN_API void	XPluginStop(void) {
	XPLMUnregisterFlightLoopCallback(flightcb, NULL);
	if (!g_win) {
		XPLMDestroyWindow(g_win);
		g_win = NULL;
	}
	if(g_pbuffer) {
		free(g_pbuffer);
		g_pbuffer = NULL;
	}
}

PLUGIN_API void XPluginDisable(void) {
}

PLUGIN_API int XPluginEnable(void) {
	return 1;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inForm, int inMessage, void * inParam) {
}
