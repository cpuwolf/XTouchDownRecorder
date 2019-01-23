
#ifndef XTDCEF3_H
#define XTDCEF3_H

#include <include/cef_app.h>
#include <include/cef_client.h>
#include <include/cef_render_handler.h>

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

public:
	void init(GLuint ** ceftxt);
	void resize(int w, int h);

	// CefRenderHandler interface
public:
	void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect);
	void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirtyRects, const void *buffer, int width, int height);

	// CefBase interface
public:
	IMPLEMENT_REFCOUNTING(RenderHandler);

public:
	GLuint tex() const { return tex_; }

private:
	int width_;
	int height_;

	GLuint tex_;
};


class BrowserClient : public CefClient
{
public:
	BrowserClient(RenderHandler *renderHandler);

	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() {
		return m_renderHandler;
	}

	CefRefPtr<CefRenderHandler> m_renderHandler;

	IMPLEMENT_REFCOUNTING(BrowserClient);
};


#ifdef __cplusplus
extern "C" {
#endif
bool CEF_init(int w, int h, GLuint **);
void CEF_update();
void CEF_deinit();

#ifdef __cplusplus
}
#endif


#endif //XTDCEF3_H