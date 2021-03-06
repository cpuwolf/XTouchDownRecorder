
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

#include <XPLMUtilities.h>

RenderHandler::RenderHandler()
	: width_(2), height_(2), tex_(0)
{
}

void RenderHandler::init(GLuint ** ceftxt)
{
	XPLMDebugString("\nXTouchDownRecorder:render start:\n");
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

	*ceftxt = &tex_;
	XPLMDebugString("\nXTouchDownRecorder:render End:\n");
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
	glBindTexture(GL_TEXTURE_2D, tex_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (unsigned char*)buffer);
	glBindTexture(GL_TEXTURE_2D, 0);

}

BrowserClient::BrowserClient(RenderHandler *renderHandler)
	: m_renderHandler(renderHandler)
{
}

static void openbrowser(char * purl)
{
#if defined(_WIN32)
	ShellExecute(NULL, "open", purl, NULL, NULL, SW_SHOWNORMAL);
#elif defined(__linux__)
	char tmp[512];
	sprintf(tmp, "sensible-browser %s&", purl);
	system(tmp);
#elif defined(__APPLE__)
	char tmp[512];
	sprintf(tmp, "open %s&", purl);
	system(tmp);
#endif
}

bool BrowserClient::OnBeforePopup(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		const CefString& target_url,
		const CefString& target_frame_name,
		WindowOpenDisposition target_disposition,
		bool user_gesture,
		const CefPopupFeatures& popup_features,
		CefWindowInfo& window_info,
		CefRefPtr<CefClient>& client,
		CefBrowserSettings& settings,
		bool* no_javascript_access) 
{
	const wchar_t * purl;
	char m_char[512];
	std::wstring str=target_url.ToWString();
	purl=str.c_str();
	size_t wlen = wcslen(purl);
 	size_t len = WideCharToMultiByte(CP_ACP, 0, purl, wlen, NULL, 0, NULL, NULL);
	XPLMDebugString("XTouchDownRecorder:onPopup: Start\n");
	if ((len > 1) && (len+1<sizeof(m_char))) {
		memset(m_char, 0, sizeof(m_char));
		WideCharToMultiByte(CP_ACP, 0, purl, wlen, m_char, len, NULL, NULL);
		XPLMDebugString(m_char);
		openbrowser(m_char);
	}
	XPLMDebugString("\nXTouchDownRecorder:onPopup End:\n");
	return true; //prevent popup
}


struct cefui * CEF_init(int w, int h, const char * exepath, const char * dbgpath, const char * cachepath)
{
	RenderHandler* render_handler_;

	struct cefui * pcef = (struct cefui *)malloc(sizeof(struct cefui));
	if (!pcef) {return NULL;}
	memset(pcef, 0, sizeof(struct cefui));

	CefMainArgs args;

	CefSettings settings;
	
	CefString(&settings.browser_subprocess_path).FromASCII(exepath);
	CefString(&settings.log_file).FromASCII(dbgpath);
	CefString(&settings.cache_path ).FromASCII(cachepath);
	settings.log_severity= LOGSEVERITY_WARNING;
	

	bool result = CefInitialize(args, settings, nullptr, nullptr);
	if (!result) {
		pcef->errorcode = -1;
		return pcef;
	}
	

	render_handler_ = new RenderHandler();
	render_handler_->init(&(pcef->ceftxt));

	render_handler_->resize(w, h);

	CefBrowserSettings browserSettings;
	CefWindowInfo window_info;
	window_info.SetAsWindowless(NULL); 

	// browserSettings.windowless_frame_rate = 60; // 30 is default
	pcef->client_ = new BrowserClient(render_handler_);

	pcef->browser_ = CefBrowserHost::CreateBrowserSync(window_info, pcef->client_.get(), "https://x-plane.vip/xtdr/static/tiny.html", browserSettings, nullptr);

	pcef->isinit=true;
	return pcef;
}
void CEF_update()
{
	CefDoMessageLoopWork();
}

void CEF_url(struct cefui * pcef,char * url)
{
	CefString curl;
	curl.FromASCII(url);
	pcef->browser_->GetMainFrame()->LoadURL(curl);
	//pcef->browser_->GetMainFrame()->LoadURL("https://x-plane.vip/xtdr/static/bad.html");
}

void CEF_mouseclick(struct cefui * pcef, int x, int y,bool up)
{
	CefMouseEvent evt;
	CefRefPtr<CefBrowserHost> host;
	host = pcef->browser_->GetHost();
	evt.x = x;
	evt.y = y;
	if(host) {
		host->SendMouseClickEvent(evt, MBT_LEFT, up, 1);
	}
	CEF_update();
}

void CEF_mousemove(struct cefui * pcef, int x, int y)
{
	CefMouseEvent evt;
	CefRefPtr<CefBrowserHost> host;
	host = pcef->browser_->GetHost();
	evt.x = x;
	evt.y = y;
	if(host) {
		pcef->browser_->GetHost()->SendMouseMoveEvent(evt, false);
	}
	CEF_update();
}

void CEF_deinit(struct cefui * pcef)
{
	pcef->browser_->GetHost()->CloseBrowser(true);
	CefDoMessageLoopWork();
	CefShutdown();
	free(pcef);
}