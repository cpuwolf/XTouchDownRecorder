/*
XTouchDownRecorder

BSD 2-Clause License

Copyright (c) 2018, Wei Shuai <cpuwolf@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <math.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#pragma comment(lib, "wldap32.lib" )
#pragma comment(lib, "crypt32.lib" )
#pragma comment(lib, "Ws2_32.lib")
//#define CURL_STATICLIB
#include <curl/curl.h>

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

#define _PRONAMEVER_ "XTouchDownRecorder V6a (" __DATE__ ")"

#define MAX_TABLE_ELEMENTS 500
#define CURVE_LEN 2

static XPLMCommandRef ToggleCommand = NULL;

static XPLMDataRef gearFRef,gForceRef,vertSpeedRef,pitchRef,elevatorRef,engRef,aglRef,tmRef,gndSpeedRef,latRef,longRef,tailRef,icaoRef;

static float * g_pbuffer = NULL;
static unsigned int g_start = 0;
static unsigned int g_end = 0;
static unsigned int g_size = 0;
static char * g_xppath = NULL;

#define BUFFER_DELETE() g_size=0;g_start = 0;g_end = 0;

#define BUFFER_EMPTY() (g_size==0)

#define BUFFER_FULL() (g_size==MAX_TABLE_ELEMENTS)

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
typedef struct
{
	int winposx;
	int winposy;
	int linkposx;
	int linkposy;
	int linkwidth;
	int linkheight;
}XTDWin;
static XPLMMenuID tdr_menu = NULL;

static BOOL collect_touchdown_data = TRUE;
static unsigned int show_touchdown_counter = 0;

static time_t touchTime;
static char landingString[128];
static BOOL IsLogWritten = TRUE;
static BOOL IsTouchDown = FALSE;
static unsigned int ground_counter = 10;
static unsigned int taxi_counter = 0;

static char g_NewsString[128];

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
	XTDWin * ref = (XTDWin *)inRefcon;
	switch (inMouse) {
	case xplm_MouseDown:
		if ((x >= ref->winposx + _TD_CHART_WIDTH - 10) && (x <= ref->winposx + _TD_CHART_WIDTH) &&
					(y <= ref->winposy) && (y >= ref->winposy - 10)) {
			show_touchdown_counter = 0;
		}

		lastMouseX = x;
		lastMouseY = y;

		break;

	case xplm_MouseDrag:
		ref->winposx += x - lastMouseX;
		ref->winposy += y - lastMouseY;
		XPLMSetWindowGeometry(inWindowID, ref->winposx, ref->winposy,
				ref->winposx + _TD_CHART_WIDTH, ref->winposy - _TD_CHART_HEIGHT);
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
	int width_text_to_print = (int)(floor(XPLMMeasureString(xplmFont_Basic, text_to_print, (int)strlen(text_to_print))));
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
	float max_axis_plus=(float)fabs(max_axis);
	while(!BUFFER_GO_IS_END(k,tmpc)) {
		p = mytable[k];
		if(max_axis_plus<0.000001f) {
			/*avoid division 0 exception*/
			y_height = 0;
		} else {
			y_height = (int)round(p / max_axis_plus * _TD_CHART_HEIGHT);
		}

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

static int gettouchdownanddraw(int idx, float * pfpm, float pg[],int x, int y)
{
	int k,tmpc;
	int x_tmp = x;
	int iter_times=0;
	BOOL iter_start=FALSE;
	BUFFER_GO_START(k,tmpc);
	/*get interval seconds*/
	int last_k = k;
	float zero_tm = touchdown_tm_table[idx];
	float zero_agl = touchdown_agl_table[idx];
	float interval = (float)floor(zero_tm - touchdown_tm_table[k]);
	float s = interval;
	float last_fpm=99999.f,fpm;
	float max_g=0.f,min_g=9999.f;
	float delta_tm_expect;

	while(!BUFFER_GO_IS_END(k,tmpc)) {
		float delta_tm = zero_tm - touchdown_tm_table[k];
		if((delta_tm - s) <= 0.0f) {
			/*align with second*/
			if(s - 1.0f < 0.0001f) {
				/*1sec before touch*/
				iter_start = TRUE;
				delta_tm_expect = delta_tm;
				last_k = k;
			}
			/*draw second axis*/
			draw_line(0,0,0,1,3,x_tmp, y, x_tmp, y + _TD_CHART_HEIGHT);
			/*goto next second*/
			s-=1.0f;
		}
		/*caculate descent rate*/
		if((iter_start) && (iter_times < 4) && (delta_tm-delta_tm_expect<=0.0f)) {
			fpm=(touchdown_agl_table[k]-zero_agl)*196.850394f/delta_tm;
			if(fpm < last_fpm) {
				last_fpm = fpm;
				delta_tm_expect= delta_tm/2.0f;
				iter_times++;
			}
		}
		/*caculate G force*/
		if((delta_tm >= 0.05f)&&(delta_tm <= 3.0f)) {
			if(touchdown_g_table[k] > max_g) {
				max_g = touchdown_g_table[k];
			}
			if(touchdown_g_table[k] < min_g) {
				min_g = touchdown_g_table[k];
			}
		}
		x_tmp = x_tmp + 2;
		BUFFER_GO_NEXT(k,tmpc);
	}

	*pfpm =  last_fpm;
	pg[0] = max_g;
	pg[1] = min_g;

	return last_k;
}
static int drawtouchdownpoints(int x, int y)
{
	int k,tmpc;
	int touchtimes = 0;
	/*-- draw touch point vertical lines*/
	int x_tmp = x;
	
	BUFFER_GO_START(k,tmpc);
	BOOL last_air_recorded = touchdown_air_table[k];
	float last_air_tm = touchdown_tm_table[k];
	BOOL b;
	while(!BUFFER_GO_IS_END(k,tmpc)) {
		b = touchdown_air_table[k];
		if(b != last_air_recorded) {
			if(b) {
				/*-- draw vertical line, skip small debounce*/
				if (touchdown_tm_table[k] - last_air_tm > 0.5f) {
					draw_line(1, 1, 1, 1, 3, x_tmp, y + (_TD_CHART_HEIGHT / 4), x_tmp, y + (_TD_CHART_HEIGHT * 3 / 4));
					touchtimes++;
				}
			} else {
				last_air_tm = touchdown_tm_table[k];
			}
		}
		x_tmp = x_tmp + 2;
		last_air_recorded = b;
		BUFFER_GO_NEXT(k,tmpc);
	}
	return touchtimes;
}

static int getfirsttouchdownpointidx()
{
	int k,tmpc;
	BUFFER_GO_START(k,tmpc);
	BOOL last_air_recorded = touchdown_air_table[k];
	float last_agl_recorded = touchdown_agl_table[k];
	BOOL b;
	float max_agl_recorded = get_max_val(touchdown_agl_table);
	while(!BUFFER_GO_IS_END(k,tmpc)) {
		b = touchdown_air_table[k];
		if(b != last_air_recorded) {
			if(b) {
				if(max_agl_recorded > 0.5f) {
					/* touchdown at least from AGL 0.5 meter to Ground: ignore annoying plane load touch down */
					IsTouchDown = TRUE;
					return k;
				}
			}
		}
		last_air_recorded = b;
		last_agl_recorded = touchdown_agl_table[k];
		BUFFER_GO_NEXT(k,tmpc);
	}
	return -1;
}

static void drawcb(XPLMWindowID inWindowID, void *inRefcon)
{
	/*-- draw background first*/
	int x, y;
	int left, top, right, bottom;
	float color[] = { 1.0, 1.0, 1.0 };
	char text_buf[256];

	XPLMGetWindowGeometry(inWindowID, &left, &top, &right, &bottom);
	XPLMDrawTranslucentDarkBox(left, top, right, bottom);

	x = left;
	y = bottom;

	/*-- draw center line*/
	draw_line(0, 0, 0, 1, 3,x, y + (_TD_CHART_HEIGHT / 2), x + (MAX_TABLE_ELEMENTS * 2), y + (_TD_CHART_HEIGHT / 2));

	int touch_idx = getfirsttouchdownpointidx();

	int x_text = x + 5;
	int y_text = y + 4;

	/* print landing load data */
	float landingVS, landingG[2];
	memset(landingString, 0, sizeof(landingString));
	if(touch_idx >= 0) {
		float landingPitch = touchdown_pch_table[touch_idx];
		float landingGs = touchdown_gs_table[touch_idx];
		gettouchdownanddraw(touch_idx, &landingVS, landingG, x, y);
		/*-- draw touch point vertical lines*/
		int bouncedtimes = drawtouchdownpoints(x, y);
		char *text_to_print = text_buf;
		sprintf(text_to_print,"%.01fFpm Max%.02fG Min%.02fG %.02fDegree %.01fKnots %s", landingVS, landingG[0], landingG[1],
			landingPitch, landingGs*1.943844f, (bouncedtimes>1?"Bounced":""));
		int width_text_to_print = (int)floor(XPLMMeasureString(xplmFont_Basic, text_to_print, (int)strlen(text_to_print)));
		XPLMDrawString(color, x_text, y_text, text_to_print, NULL, xplmFont_Basic);
		x_text = x_text + width_text_to_print;
		/*update content for file output*/
		strcat(landingString,text_to_print);


	}

	/*start a new line*/
	x_text = x + 5;
	y_text = y + 16;
	/*-- now draw the chart line green
	float max_vs_recorded = get_max_val(touchdown_vs_table);
	sprintf(text_buf, "Max %.02ffpm ", max_vs_recorded);
	x_text = draw_curve(touchdown_vs_table, 0.0f,1.0f,0.0f, text_buf, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_vs_recorded*2.0f, max_vs_recorded);
	*/

	/*-- now draw the chart line red
	float max_g_axis = 1.8f;
	float max_g_recorded = get_max_val(touchdown_g_table);
	sprintf(text_buf, "Max %.02fG ", max_g_recorded);
	x_text = draw_curve(touchdown_g_table, 1,0.68f,0.78f, text_buf, x_text, y_text, x, y, x, y, max_g_axis, max_g_recorded);
	*/

	/*-- now draw the chart line light blue*/
	float max_pch_recorded = get_max_val(touchdown_pch_table);
	sprintf(text_buf, "Max pitch %.02fDegree ", max_pch_recorded);
	x_text = draw_curve(touchdown_pch_table, 0.6f,0.85f,0.87f, text_buf, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_pch_recorded*2.0f, max_pch_recorded);

	/*-- now draw the chart line orange
	float max_elev_axis = 2.0;
	float max_elev_recorded = get_max_val(touchdown_elev_table);
	sprintf(text_buf, "Max elevator %.02f%% ", max_elev_recorded*100.0f);
	x_text = draw_curve(touchdown_elev_table, 1.0f,0.49f,0.15f, text_buf, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_elev_recorded, max_elev_recorded);
	*/
	/*-- now draw the chart line yellow*/
	float max_eng_recorded = get_max_val(touchdown_eng_table);
	sprintf(text_buf, "Max eng %.02f%% ", max_eng_recorded*100.0f);
	x_text = draw_curve(touchdown_eng_table, 1.0f,1.0f,0.0f, text_buf, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_eng_recorded*2.0f, max_eng_recorded);

	/*-- now draw the chart line red*/
	float max_agl_recorded = get_max_val(touchdown_agl_table);
	sprintf(text_buf, "Max AGL %.02fM ", max_agl_recorded);
	x_text = draw_curve(touchdown_agl_table, 1.0f,0.1f,0.1f, text_buf, x_text, y_text, x, y, x, y + (_TD_CHART_HEIGHT / 2), max_agl_recorded*2.0f, max_agl_recorded);

	/*-- now draw the chart line blue*/
	float max_gs_recorded = get_max_val(touchdown_gs_table);
	sprintf(text_buf, "Max %.02fknots ", max_gs_recorded*1.943844f);
	x_text = draw_curve(touchdown_gs_table, 0.24f,0.35f,0.8f, text_buf, x_text, y_text, x, y, x, y, max_gs_recorded, max_gs_recorded);

	/*-- title*/
	color[0] = 1.0;
	color[1] = 1.0;
	color[2] = 1.0;
	sprintf(text_buf, _PRONAMEVER_" by cpuwolf %s", g_NewsString);
	XPLMDrawString(color, x + 5, y + _TD_CHART_HEIGHT - 15, text_buf, NULL, xplmFont_Basic);

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
	XTDWin * ref = (XTDWin *)inRefcon;
	if (!g_win) {
		ref->winposx = 10;
		ref->winposy = 900;
		g_win = XPLMCreateWindow(ref->winposx, ref->winposy,
			ref->winposx + _TD_CHART_WIDTH, ref->winposy - _TD_CHART_HEIGHT,
			0, drawcb, keycb,
			mousecb, inRefcon);
	}

	if(collect_touchdown_data) {
		collect_flight_data();
	} else ret = 0.3f;
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
	size_t len = strlen(str);
	for (i=0; i < len; i++) {
		if (('\r' == str[i]) || ('\n' == str[i])) {
			str[i] = 0;
		}
	}
}

static int write_csv_file_bool(FILE *ofile,BOOL mytable[], const char * title)
{
	int ret;
	int k,tmpc;
	fprintf(ofile, "%s,", title);
	BUFFER_GO_START(k,tmpc);
	while(!BUFFER_GO_IS_END(k,tmpc)) {
		ret=fprintf(ofile, "%d,", mytable[k]);
		if(ret <= 0) return ret;
		BUFFER_GO_NEXT(k,tmpc);
	}
	ret=fprintf(ofile, "\n");
	return ret;
}
static int write_csv_file(FILE *ofile,float mytable[], const char * title)
{
	int ret;
	int k,tmpc;
	fprintf(ofile, "%s,", title);
	BUFFER_GO_START(k,tmpc);
	while(!BUFFER_GO_IS_END(k,tmpc)) {
		ret=fprintf(ofile, "%.02f,", mytable[k]);
		if(ret <= 0) return ret;
		BUFFER_GO_NEXT(k,tmpc);
	}
	ret=fprintf(ofile, "\n");
	return ret;
}
static int movefile(char * srcfile, char * dstfile)
{
	size_t len = 0;
	char buffer[512];
	FILE* in = fopen(srcfile, "rb");
	if (in == NULL)
	{
		XPLMDebugString("XTouchDownRecorder: movefile skip\n");
		in = NULL;
	} else {
		FILE* out = fopen(dstfile, "a+b");
		if (out) {
			while ((len = fread(buffer, 512, 1, in)) > 0)
			{
				fwrite(buffer, 512, 1, out);
			}
			fclose(out);
		}
		fclose(in);


		if (remove(srcfile))
		{
			XPLMDebugString("XTouchDownRecorder: movefile done\n");
		} else {
			XPLMDebugString("XTouchDownRecorder: movefile:delete error\n");
		}
	}
	return 0;
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
	static char tmbuf[500], path[512];

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

	sprintf(path, "%sXTouchDownRecorderLog.txt", g_xppath);

	/*back compatible */
	movefile("XTouchDownRecorderLog.txt", path);
	ofile = fopen(path, "a");
	if (ofile) {
		fprintf(ofile, "%s [%s][%s] %s %s %s\n", tmbuf, logAircraftIcao, logAircraftTail, logAirportId, logAirportName, landingString);
		fclose(ofile);
	} else {
		XPLMDebugString("XTouchDownRecorder: XTouchDownRecorderLog.txt open error\n");
	}
	/*reuse tmbuf, generating file name*/
	strftime(tmbuf, sizeof(tmbuf), "XTD-%F%H%M%S.csv", tblock);
	sprintf(path, "%s%s", g_xppath, tmbuf);
	ofile = fopen(path, "a");
	if (ofile) {
		write_csv_file(ofile, touchdown_tm_table,"\"time(s)\"");
		write_csv_file_bool(ofile, touchdown_air_table,"\"is air\"");
		write_csv_file(ofile, touchdown_vs_table,"\"(fpm)\"");
		write_csv_file(ofile, touchdown_g_table,"\"G force(G)\"");
		write_csv_file(ofile, touchdown_pch_table,"\"pitch(degree)\"");
		write_csv_file(ofile, touchdown_elev_table,"\"elevator(%)\"");
		write_csv_file(ofile, touchdown_eng_table,"\"engine(%)\"");
		write_csv_file(ofile, touchdown_agl_table,"\"AGL(meter)\"");
		write_csv_file(ofile, touchdown_gs_table,"\"ground speed(meter/s)\"");
		fclose(ofile);
	} else {
		XPLMDebugString("XTouchDownRecorder: data exporting error\n");
	}
	IsLogWritten = TRUE;
}

static BOOL getnetinfodone()
{
	return (strlen(g_NewsString) > 1)?TRUE:FALSE;
}
static size_t httpcb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t len = size*nmemb;
    if(len < sizeof g_NewsString) {
    	memcpy(g_NewsString, ptr, len);
    } else {
    	memcpy(g_NewsString, ptr, sizeof g_NewsString - 2);
    }
    return size*nmemb;
}
static void getnetinfo()
{
	CURL *curl;
 	CURLcode res;
 
 	curl_global_init(CURL_GLOBAL_DEFAULT);
 
 	curl = curl_easy_init();
 	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, "https://raw.githubusercontent.com/cpuwolf/XTouchDownRecorder/net/news.txt");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, httpcb);
    	res = curl_easy_perform(curl);
    	if(res != CURLE_OK)
    		XPLMDebugString("XTouchDownRecorder: getnetinfo error\n");
 
		curl_easy_cleanup(curl);
	}
 
	curl_global_cleanup();
}

static float secondcb(float inElapsedSinceLastCall,
	float inElapsedTimeSinceLastFlightLoop, int inCounter,
	void *inRefcon)
{
	char tmpbuf[100];

	if (is_on_ground()) {
		if(is_taxing()) {
			taxi_counter++;
			if (taxi_counter == 6) {
				if(BUFFER_FULL()) {
					show_touchdown_counter = 10;
				}
			} else if (taxi_counter == 8) {
				if (IsTouchDown) {
					IsLogWritten = FALSE;
				}
			} else if (taxi_counter == 9) {
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
	if(!getnetinfodone()) {
		getnetinfo();
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
	char path[600], *prefpath;
	const char *csep;

	/* Plugin details */
	sprintf(outName, _PRONAMEVER_" %s %s", __DATE__ , __TIME__);
	strcpy(outSig, "cpuwolf.xtouchdownrecorder");
	strcpy(outDesc, "More information https://github.com/cpuwolf/");

	memset(g_NewsString, 0, sizeof(g_NewsString));

	/* get path*/
	XPLMGetPrefsPath(path);
	csep=XPLMGetDirectorySeparator();
	prefpath = XPLMExtractFileAndPath(path);
	size_t len = strlen(path);
	sprintf(path+len,"%c..%c",*csep,*csep);
	g_xppath = malloc(512);
	if (!g_xppath) {
		XPLMDebugString("XTouchDownRecorder:malloc g_xppath error!\n");
		return 0;
	}
	strcpy(g_xppath, path);
	sprintf(path, "XTouchDownRecorder: xp path %s\n", g_xppath);
	XPLMDebugString(path);

	g_pbuffer = malloc(MAX_TABLE_ELEMENTS * sizeof(float) * MAX_TOUCHDOWN_IDX);
	if(!g_pbuffer) {
		XPLMDebugString("XTouchDownRecorder:malloc g_pbuffer error!\n");
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

	XTDWin * ref = malloc(sizeof(XTDWin));
	if (!ref) {
		XPLMDebugString("XTouchDownRecorder: win malloc error\n");
		return 0;
	}
	/* register loopback starting at 10s */
	XPLMRegisterFlightLoopCallback(flightcb, -1, ref);

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
		XTDWin *ref=XPLMGetWindowRefCon(g_win);
		if (ref) {
			free(ref);
		}
		XPLMDestroyWindow(g_win);
		//g_win = NULL;
	}
	if(g_pbuffer) {
		free(g_pbuffer);
		//g_pbuffer = NULL;
	}
	if (!g_xppath) {
		free(g_xppath);
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
			if (inParam == NULL) {
				BUFFER_DELETE();
			}
			break;
		}
	}
}
