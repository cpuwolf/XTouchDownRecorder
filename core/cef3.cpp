
/*
LightWorker

BSD 2-Clause License

Copyright (c) 2019, Wei Shuai <cpuwolf@gmail.com>
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

#include "cef3.h"

#include <XPLMPlugin.h>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>

RenderHandler::RenderHandler()
	: width_(2), height_(2), tex_(0)
{
}

void RenderHandler::init()
{
	glGenTextures(1, &tex_);
	glBindTexture(GL_TEXTURE_2D, tex_);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// dummy texture data - for debugging
	const unsigned char data[] = {
		255, 0, 0, 255,
		0, 255, 0, 255,
		0, 0, 255, 255,
		255, 255, 255, 255,
	};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glBindTexture(GL_TEXTURE_2D, 0);
}
void RenderHandler::resize(int w, int h)
{
	width_ = w;
	height_ = h;
}

void RenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect)
{
	rect = CefRect(0, 0, width_, height_);
}

void RenderHandler::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirtyRects, const void *buffer, int width, int height)
{
#if 0
	int screen_x, screen_y;
	#define MARGIN_SIZE 0
	XPLMGetScreenSize(&screen_x, &screen_y);
	XPLMSetGraphicsState(1, 1, 0, 1, 1, 1, 1);

	glBindTexture(GL_TEXTURE_2D, tex_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (unsigned char*)buffer);

	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 1.0);
	glVertex2f((screen_x - width_) / 2 - MARGIN_SIZE, 0);
	glTexCoord2f(0.0, 0.0);
	glVertex2f((screen_x - width_) / 2 - MARGIN_SIZE,
	    height_ + 2 * MARGIN_SIZE);
	glTexCoord2f(1.0, 0.0);
	glVertex2f((screen_x + width_) / 2 + MARGIN_SIZE,
	    height_ + 2 * MARGIN_SIZE);
	glTexCoord2f(1.0, 1.0);
	glVertex2f((screen_x + width_) / 2 + MARGIN_SIZE, 0);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
#endif
}

BrowserClient::BrowserClient(RenderHandler *renderHandler)
	: m_renderHandler(renderHandler)
{
}

bool CEF_init(int w, int h)
{
	int exit_code;
	CefRefPtr<CefBrowser> browser_;
	CefRefPtr<BrowserClient> client_;
	RenderHandler* render_handler_;

	CefMainArgs args;
	exit_code = CefExecuteProcess(args, nullptr, nullptr);;
	if (exit_code >= 0) {
		return false;
	}
	

	CefSettings settings;
	bool result = CefInitialize(args, settings, nullptr, nullptr);
	if (!result) {
		exit_code = -1;
		return false;
	}
	return true;

	render_handler_ = new RenderHandler();
	render_handler_->init();

	render_handler_->resize(w, h);

	CefBrowserSettings browserSettings;
	CefWindowInfo window_info;
	window_info.SetAsWindowless(NULL); 

	// browserSettings.windowless_frame_rate = 60; // 30 is default
	client_ = new BrowserClient(render_handler_);

	browser_ = CefBrowserHost::CreateBrowserSync(window_info, client_.get(), "https://x-plane.vip/xtdr/static/", browserSettings, nullptr);
	return true;
}
void CEF_update()
{
	CefDoMessageLoopWork();
}

void CEF_deinit()
{
	//CefRunMessageLoop();
	CefShutdown();
}