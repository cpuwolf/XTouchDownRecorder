
#ifndef XTDCEF3_H
#define XTDCEF3_H

#include <include/cef_app.h>
#include <include/cef_client.h>
#include <include/cef_render_handler.h>
#include <include/cef_life_span_handler.h>
#include <include/cef_load_handler.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <Carbon/Carbon.h>
#else
#include <stdlib.h>
#include <GL/gl.h>
//#include <GL/glew.h>
#endif

class RenderHandler : public CefRenderHandler
{
public:
	RenderHandler();

	void init(GLuint ** ceftxt);
	void resize(int w, int h);

	// CefRenderHandler interface
	void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect);
	void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirtyRects, const void *buffer, int width, int height);

	// CefBase interface

	IMPLEMENT_REFCOUNTING(RenderHandler);


	GLuint tex() const { return tex_; }

private:
	int width_;
	int height_;

	GLuint tex_;
};


class BrowserClient : public CefClient,
	public CefLifeSpanHandler,
	public CefLoadHandler
{
public:
	BrowserClient(RenderHandler *renderHandler);

	CefRefPtr<CefRenderHandler> GetRenderHandler() override {
		return m_renderHandler;
	}
	CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override
    {
        return this;
    }

    CefRefPtr<CefLoadHandler> GetLoadHandler() override
    {
        return this;
    }
    bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		const CefString& target_url,
		const CefString& target_frame_name,
		WindowOpenDisposition target_disposition,
		bool user_gesture,
		const CefPopupFeatures& popup_features,
		CefWindowInfo& window_info,
		CefRefPtr<CefClient>& client,
		CefBrowserSettings& settings,
		bool* no_javascript_access) override;

	CefRefPtr<CefRenderHandler> m_renderHandler;

	IMPLEMENT_REFCOUNTING(BrowserClient);
};

struct cefui
{
	bool isinit;
	CefRefPtr<CefBrowser> browser_;
	CefRefPtr<BrowserClient> client_;
	GLuint * ceftxt;
};


#ifdef __cplusplus
extern "C" {
#endif
struct cefui * CEF_init(int w, int h);
void CEF_update();
void CEF_url(struct cefui * pcef,char * url);
void CEF_mouseclick(struct cefui * pcef, int x, int y,bool up);
void CEF_mousemove(struct cefui * pcef, int x, int y);
void CEF_deinit(struct cefui *);

#ifdef __cplusplus
}
#endif


#endif //XTDCEF3_H