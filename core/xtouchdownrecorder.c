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

#if defined(__APPLE__) || defined(__unix__)
#ifndef BOOL
#define BOOL unsigned char
#define TRUE 1
#define FALSE 0
#endif
#endif

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

static XPLMCommandRef ToggleCommand = NULL;

static XPLMDataRef gearFRef,gForceRef,vertSpeedRef,pitchRef,elevatorRef,engRef,aglRef,tmRef,gndSpeedRef,latRef,longRef,tailRef,icaoRef;

static float * g_pbuffer = NULL;
static unsigned int g_start = 0;
static unsigned int g_end = 0;
static unsigned int g_size = 0;

#define BUFFER_DELETE() g_size=0;g_start = 0;g_end = 0;

#define BUFFER_EMPTY() (g_size==0)

/* set value firstly, then increase index */
#define BUFFER_INSERT_BACK() if(g_end < MAX_TABLE_ELEMENTS) {\
								g_end++;} if(g_end==MAX_TABLE_ELEMENTS) {\
									g_end = 0;\
								} \
								if(g_size < MAX_TABLE_ELEMENTS) {\
									g_size++;\
								} else {\
									if(g_start < MAX_TABLE_ELEMENTS) {\
										g_start++;} if(g_start==MAX_TABLE_ELEMENTS) {\
											g_start = 0;}\
								}

#define BUFFER_GO_START(idx,tmp_count)  tmp_count=g_size; idx=g_start;

#define BUFFER_GO_IS_END(idx,tmp_count)  (tmp_count<=0)

#define BUFFER_GO_NEXT(idx,tmp_count) if(idx < MAX_TABLE_ELEMENTS) {\
								idx++;} if(idx==MAX_TABLE_ELEMENTS) {\
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
	TOUCHDOWN_GS_IDX,
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
static float *touchdown_gs_table;

static float lastVS = 1.0;
static float lastG = 1.0;
static float lastPitch = 1.0;
static BOOL lastAir = FALSE;
static float lastElev = 0.0;
static float lastEng = 0.0;
static float lastAgl = 0.0;
static float lastTm = 0.0;
static float lastGs = 0.0;

#define _TD_CHART_HEIGHT 200
#define _TD_CHART_WIDTH (MAX_TABLE_ELEMENTS*CURVE_LEN)
static XPLMWindowID g_win = NULL;
static int g_winposx = 10;
static int g_winposy = 900;

static XPLMMenuID tdr_menu = NULL;

static BOOL collect_touchdown_data = TRUE;
static unsigned int show_touchdown_counter = 3;

static time_t touchTime;
static char landingString[128];
static BOOL IsLogWritten = TRUE;
static BOOL IsTouchDown = FALSE;
static unsigned int ground_counter = 10;
static unsigned int taxi_counter = 0;

static BOOL check_ground(float n)
{
	if ( n != 0.0f ) {
		return TRUE;
	} else return FALSE;
}

static BOOL is_on_ground()
{
	return check_ground(XPLMGetDataf(gearFRef));
}

static BOOL is_taxing()
{
	/*push back ground speed is < 1.5ms*/
	float speed = XPLMGetDataf(gndSpeedRef);
	if((speed > 2.0f) && (speed < 10.0f)) {
		return TRUE;
	}
	return FALSE;
}

static float get_max_val(float mytable[])
{
	int k,tmpc;
	/*-- calculate max data*/
	float max_data = 0.0f;

	BUFFER_GO_START(k,tmpc);
	while(!BUFFER_GO_IS_END(k,tmpc)) {
		float el = mytable[k];
		if (fabs(el) > fabs(max_data)) {
			max_data = el;
		}
		BUFFER_GO_NEXT(k,tmpc);
	}
	return max_data;
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
	XPLMGetDatavf(engRef,engtb,0,3);
	lastEng = engtb[0];
	lastGs = XPLMGetDataf(gndSpeedRef);

	/*-- fill the table */
	touchdown_vs_table[iw] = lastVS;
	touchdown_g_table[iw] = lastG;
	touchdown_pch_table[iw] = lastPitch;
	touchdown_air_table[iw] = lastAir;
	touchdown_elev_table[iw] = lastElev;
	touchdown_eng_table[iw] = lastEng;
	touchdown_agl_table[iw] = lastAgl;
	touchdown_tm_table[iw] = lastTm;
	touchdown_gs_table[iw] = lastGs;
	BUFFER_INSERT_BACK();

}

static void keycb(XPLMWindowID inWindowID, char inKey, XPLMKeyFlags inFlags,
				   char inVirtualKey, void *inRefcon, int losingFocus)
{

}
static int mousecb(XPLMWindowID inWindowID, int x, int y,
				   XPLMMouseStatus inMouse, void *inRefcon)
{
	static int lastMouseX, lastMouseY;
	switch (inMouse) {
	case xplm_MouseDown:
		if ((x >= g_winposx + _TD_CHART_WIDTH - 10) && (x <= g_winposx + _TD_CHART_WIDTH) &&
					(y <= g_winposy) && (y >= g_winposy - 10)) {
			show_touchdown_counter = 0;
		}

		lastMouseX = x;
		lastMouseY = y;

		break;

	case xplm_MouseDrag:
		g_winposx += x - lastMouseX;
		g_winposy += y - lastMouseY;
		XPLMSetWindowGeometry(inWindowID, g_winposx, g_winposy,
				g_winposx + _TD_CHART_WIDTH, g_winposy - _TD_CHART_HEIGHT);
		lastMouseX = x;
		lastMouseY = y;
		break;

	}
	return 1;
}

static void draw_line(float r,float g, float b, float alpha, float width, int x1, int y1, int x2, int y2)
{
	glDisable(GL_TEXTURE_2D);
	glColor3f(r, g, b);
	glLineWidth(width);
	glBegin(GL_LINES);
	glVertex2i(x1, y1);
	glVertex2i(x2, y2);
	glEnd();
	glEnable(GL_TEXTURE_2D);
}

static int draw_curve(float mytable[], float cr, float cg, float cb,
	char * text_to_print,
	int x_text_start, int y_text_start, int x_orig, int y_orig,
	int x_start,int y_start,
	float max_axis, float max_data)
{
	int k,tmpc;
	float color[] = { cr, cg, cb };

	/*-- print text*/
	int x_text = x_text_start;
	int y_text = y_text_start;
	int width_text_to_print = (int)(floor(XPLMMeasureString(xplmFont_Basic, text_to_print, strlen(text_to_print))));
	XPLMDrawString(color, x_text, y_text, text_to_print, NULL, xplmFont_Basic);
	x_text = x_text + width_text_to_print;
	/*-- draw line*/
	int x_tmp = x_start;
	int y_tmp = y_start;
	int x_max_tmp = x_tmp;
	glDisable(GL_TEXTURE_2D);
	glColor3f(cr, cg, cb);
	glLineWidth(1);
	glBegin(GL_LINE_STRIP);
	BUFFER_GO_START(k,tmpc);
	float p;
	int y_height,y1,ymax=y_orig + _TD_CHART_HEIGHT;
	int draw_max_counter = 0;
	while(!BUFFER_GO_IS_END(k,tmpc)) {
		p = mytable[k];
		y_height = (int)round(p / max_axis * _TD_CHART_HEIGHT);

		y1 = y_tmp + y_height;
		if(y1 < y_orig) {
			glVertex2i(x_tmp, y_orig);
		} else if (y1 > ymax) {
			glVertex2i(x_tmp, ymax);
		} else {
			glVertex2i(x_tmp, y_tmp + y_height);
		}
		if (p == max_data) {
			if (draw_max_counter == 0) {
				x_max_tmp = x_tmp;
			}
			draw_max_counter = draw_max_counter + 1;
		}
		x_tmp = x_tmp + 2;
		BUFFER_GO_NEXT(k,tmpc);
	}
	glEnd();
	glEnable(GL_TEXTURE_2D);
	draw_line(cr,cg,cb,1,1,x_max_tmp, y_orig, x_max_tmp, y_orig + _TD_CHART_HEIGHT);

	return x_text;
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
	XPLMDrawString(color, x + 5, y + _TD_CHART_HEIGHT - 15, "XTouchDownRecorder V4 by cpuwolf", NULL, xplmFont_Basic);

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
				float landingGs = touchdown_gs_table[k];
				char *text_to_print = text_buf;
				sprintf(text_to_print,"%.02ffpm %.02fG %.02fDegree %.02fknots| ", landingVS, landingG, landingPitch, landingGs*1.943844f);
				strcat(landingString,text_to_print);
				int width_text_to_print = (int)floor(XPLMMeasureString(xplmFont_Basic, text_to_print, strlen(text_to_print)));
				XPLMDrawString(color, x_text, y_text, text_to_print, NULL, xplmFont_Basic);
				x_text = x_text + width_text_to_print;
			}
		}
		x_tmp = x_tmp + 2;
		last_air_recorded = b;
		BUFFER_GO_NEXT(k,tmpc);
	}

	/*-- now draw the chart line green*/
	float max_vs_axis = 1000.0f;
	float max_vs_recorded = get_max_val(touchdown_vs_table);
	sprintf(text_buf, "Max %.02ffpm ", max_vs_recorded);
	x_text = draw_curve(touchdown_vs_table, 0.0f,1.0f,0.0f, text_buf, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_vs_axis, max_vs_recorded);

	/*-- now draw the chart line red*/
	float max_g_axis = 1.8f;
	float max_g_recorded = get_max_val(touchdown_g_table);
	sprintf(text_buf, "Max %.02fG ", max_g_recorded);
	x_text = draw_curve(touchdown_g_table, 1,0.68f,0.78f, text_buf, x_text, y_text, x, y, x, y, max_g_axis, max_g_recorded);

	/*-- now draw the chart line light blue*/
	float max_pch_axis = 14.0;
	float max_pch_recorded = get_max_val(touchdown_pch_table);
	sprintf(text_buf, "Max pitch %.02fDegree ", max_pch_recorded);
	x_text = draw_curve(touchdown_pch_table, 0.6f,0.85f,0.87f, text_buf, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_pch_axis, max_pch_recorded);

	/*-- now draw the chart line orange*/
	float max_elev_axis = 2.0;
	float max_elev_recorded = get_max_val(touchdown_elev_table);
	sprintf(text_buf, "Max elevator %.02f%% ", max_elev_recorded*100.0f);
	x_text = draw_curve(touchdown_elev_table, 1.0f,0.49f,0.15f, text_buf, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_elev_axis, max_elev_recorded);

	/*-- now draw the chart line yellow*/
	float max_eng_axis = 1.0;
	float max_eng_recorded = get_max_val(touchdown_eng_table);
	sprintf(text_buf, "Max eng %.02f%% ", max_eng_recorded*100.0f);
	x_text = draw_curve(touchdown_eng_table, 1.0f,1.0f,0.0f, text_buf, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_eng_axis, max_eng_recorded);

	/*-- now draw the chart line red*/
	float max_agl_axis = 6.0;
	float max_agl_recorded = get_max_val(touchdown_agl_table);
	sprintf(text_buf, "Max AGL %.02fM ", max_agl_recorded);
	x_text = draw_curve(touchdown_agl_table, 1.0f,0.1f,0.1f, text_buf, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_agl_axis, max_agl_recorded);

	/*-- now draw the chart line blue*/
	float max_gs_axis = 180.0f;
	float max_gs_recorded = get_max_val(touchdown_gs_table);
	sprintf(text_buf, "Max %.02fknots ", max_gs_recorded*1.943844f);
	x_text = draw_curve(touchdown_gs_table, 0.24f,0.35f,0.8f, text_buf, x_text, y_text, x, y, x, y, max_gs_axis, max_gs_recorded);

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
	float ret = -1.0f;
	if (!g_win)
		g_win = XPLMCreateWindow(g_winposx, g_winposy,
			g_winposx + _TD_CHART_WIDTH, g_winposy - _TD_CHART_HEIGHT,
			1, drawcb, keycb,
			mousecb, NULL);

	if(collect_touchdown_data) {
		collect_flight_data();
	} else ret = 1.0f;
	/*-- dont draw when the function isn't wanted*/
	if(show_touchdown_counter <= 0) {
		XPLMSetWindowIsVisible(g_win, 0);
	} else {
		XPLMSetWindowIsVisible(g_win, 1);
	}

	return ret;
}

static void formattm(char *str)
{
	unsigned int i;
	unsigned int len = strlen(str);
	for (i=0; i < len; i++) {
		if (('\r' == str[i]) || ('\n' == str[i])) {
			str[i] = 0;
		}
	}
}


static void write_log_file()
{
	FILE *ofile;
	struct tm *tblock;
	int num;
	static char logAirportId[50];
	static char logAirportName[256];
	static char logAircraftTail[50];
	static char logAircraftIcao[40];
	static char tmbuf[500];

	tblock=localtime(&touchTime);
	memset(tmbuf,0,sizeof(tmbuf));
	strcpy(tmbuf, asctime(tblock));
	formattm(tmbuf);

	float lat = XPLMGetDataf(latRef);
	float lon = XPLMGetDataf(longRef);
	XPLMNavRef navref = XPLMFindNavAid(NULL, NULL, &lat, &lon, NULL, xplm_Nav_Airport);

	if (XPLM_NAV_NOT_FOUND != navref)
		XPLMGetNavAidInfo(navref, NULL, &lat, &lon, NULL, NULL, NULL, logAirportId,
				logAirportName, NULL);
	else {
		logAirportId[0] = 0;
		logAirportName[0] = 0;
	}

	num = XPLMGetDatab(tailRef, logAircraftTail, 0, 49);
	logAircraftTail[num] = 0;

	num = XPLMGetDatab(icaoRef, logAircraftIcao, 0, 39);
	logAircraftIcao[num] = 0;

	ofile = fopen("XTouchDownRecorderLog.txt", "a");
	if (ofile) {
		fprintf(ofile, "%s [%s][%s] %s %s %s\n", tmbuf, logAircraftIcao, logAircraftTail, logAirportId, logAirportName, landingString);
		fclose(ofile);
	} else {
		XPLMDebugString("XTouchDownRecorder: XTouchDownRecorderLog.txt open error");
	}
	IsLogWritten = TRUE;
}
static float secondcb(float inElapsedSinceLastCall,
	float inElapsedTimeSinceLastFlightLoop, int inCounter,
	void *inRefcon)
{
	char tmpbuf[100];

	if (is_on_ground()) {
		if(is_taxing()) {
			taxi_counter++;
			/*-- ignore debounce takeoff*/
			if (taxi_counter == 6) {
				show_touchdown_counter = 10;
			} else if (taxi_counter == 7) {
				if (IsTouchDown) {
					IsLogWritten = FALSE;
				}
			} else if (taxi_counter == 8) {
				sprintf(tmpbuf, "XTouchDownRecorder: on ground %ds\n", ground_counter);
				XPLMDebugString(tmpbuf);
				if (!IsLogWritten) {
					write_log_file();
				}
			}
		}
		ground_counter = ground_counter + 1;
		if (ground_counter == 3) {
			time(&touchTime);
			/*-- stop data collection*/
			collect_touchdown_data = FALSE;
		} 
	} else {
		/*-- in the air*/
		ground_counter = 0;
		collect_touchdown_data = TRUE;
		IsTouchDown = FALSE;
		taxi_counter = 0;
	}
	/*-- count down*/
	if (show_touchdown_counter > 0) {
		show_touchdown_counter = show_touchdown_counter - 1;
	}
	return 1.0f;
}

static void toggle_touchdown()
{
	if (show_touchdown_counter > 0) {
		show_touchdown_counter = 0;
	} else {
		show_touchdown_counter = 60;
	}
}
static int ToggleCommandHandler(XPLMCommandRef       inCommand,
								XPLMCommandPhase     inPhase,
								void *               inRefcon)
{
	if (inPhase == xplm_CommandBegin) {
		toggle_touchdown();
	}
	return 0;
}

static void menucb(void *menuRef, void *param)
{
	toggle_touchdown();
}

PLUGIN_API int XPluginStart(char * outName, char * outSig, char * outDesc)
{
	XPLMMenuID plugins_menu;
	int menuidx;

	/* Plugin details */
	sprintf(outName, "XTouchDownRecorder V4 %s %s", __DATE__ , __TIME__);
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
	touchdown_gs_table = g_pbuffer + (TOUCHDOWN_GS_IDX * MAX_TABLE_ELEMENTS);

	gearFRef = XPLMFindDataRef("sim/flightmodel/forces/fnrml_gear");
	gForceRef = XPLMFindDataRef("sim/flightmodel2/misc/gforce_normal");
	vertSpeedRef = XPLMFindDataRef("sim/flightmodel/position/vh_ind_fpm2");
	pitchRef = XPLMFindDataRef("sim/flightmodel/position/theta");
	elevatorRef = XPLMFindDataRef("sim/flightmodel2/controls/pitch_ratio");
	engRef = XPLMFindDataRef("sim/flightmodel2/engines/throttle_used_ratio");
	aglRef = XPLMFindDataRef("sim/flightmodel/position/y_agl");
	tmRef = XPLMFindDataRef("sim/time/total_flight_time_sec");

	gndSpeedRef = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
	latRef = XPLMFindDataRef("sim/flightmodel/position/latitude");
	longRef = XPLMFindDataRef("sim/flightmodel/position/longitude");
	tailRef = XPLMFindDataRef("sim/aircraft/view/acf_tailnum");
	icaoRef = XPLMFindDataRef("sim/aircraft/view/acf_ICAO");

	/* MAC OS */
	XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

	/* register loopback starting at 10s */
	XPLMRegisterFlightLoopCallback(flightcb, -1, NULL);

	/* register loopback in 1s */
	XPLMRegisterFlightLoopCallback(secondcb, 1.0f, NULL);

	ToggleCommand = XPLMCreateCommand("cpuwolf/XTouchDownRecorder/Toggle", "Toggle TouchDownRecorder Chart");
	XPLMRegisterCommandHandler(ToggleCommand, ToggleCommandHandler, 0, NULL);

	plugins_menu = XPLMFindPluginsMenu();
	menuidx = XPLMAppendMenuItem(plugins_menu, "XTouchDownRecorder", NULL, 1);
	tdr_menu = XPLMCreateMenu("XTouchDownRecorder", plugins_menu, menuidx,
				menucb, NULL);
	XPLMAppendMenuItem(tdr_menu, "Show/Hide", NULL, 1);

	return 1;
}

PLUGIN_API void	XPluginStop(void) {
	XPLMUnregisterCommandHandler(ToggleCommand, ToggleCommandHandler, 0, 0);
	XPLMUnregisterFlightLoopCallback(secondcb, NULL);
	XPLMUnregisterFlightLoopCallback(flightcb, NULL);
	if (g_win) {
		XPLMDestroyWindow(g_win);
		//g_win = NULL;
	}
	if(g_pbuffer) {
		free(g_pbuffer);
		//g_pbuffer = NULL;
	}
}

PLUGIN_API void XPluginDisable(void) {
}

PLUGIN_API int XPluginEnable(void) {
	return 1;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inForm, int inMessage, void * inParam) 
{
	if (inForm == XPLM_PLUGIN_XPLANE)
	{
		switch(inMessage) {
		case XPLM_MSG_PLANE_LOADED:
			if ((int)inParam == 0) {
				BUFFER_DELETE();
			}
			break;
		}
	}
}
