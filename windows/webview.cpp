/*
mkdir deps\webview2
curl -sSL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2" | tar -xf - -C deps\webview2
*/

#include "uipriv_windows.hpp"
#include "deps/webview2/build/native/include/WebView2.h"
#include <wrl.h>
#include <locale>
#include <codecvt>

#pragma comment(lib, "..\\deps\\webview2\\build\\native\\x64\\WebView2Loader.dll.lib")

using namespace Microsoft::WRL;

struct uiWebView {
	uiWindowsControl c;
	HWND hwnd;
	ICoreWebView2Environment *env;
	ICoreWebView2EnvironmentOptions *options;
	ICoreWebView2Controller *controller;
	ICoreWebView2 *webView;
};

uiWindowsControlAllDefaults(uiWebView)

static void uiWebViewMinimumSize(uiWindowsControl *c, int *width, int *height)
{
}

void uiWebViewEnableDevTools(uiWebView *w, int enable)
{
	// TODO
}

void uiWebViewSetInitScript(uiWebView *w, const char *script)
{
	// TODO
}

void uiWebViewOnMessage(uiWebView *w, void (*f)(uiWebView *w, const char *msg, void *data), void *data)
{
	// TODO
}

void uiWebViewRegisterUriScheme(uiWebView *w, const char *scheme, void (*f)(void *request, void *data), void *userData)
{
	// TODO
}

const char *uiWebViewRequestGetScheme(void *request)
{
	// TODO
	return NULL;
}

const char *uiWebViewRequestGetUri(void *request)
{
	// TODO
	return NULL;
}

const char *uiWebViewRequestGetPath(void *request)
{
	// TODO
	return NULL;
}

void uiWebViewRequestRespond(void *request, const char *body, size_t length, const char *contentType)
{
	// TODO
}

void uiWebViewSetHtml(uiWebView *w, const char *html)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wideHtml = converter.from_bytes(html);
	w->webView->NavigateToString(wideHtml.c_str());
}

void uiWebViewSetUri(uiWebView *w, const char *uri)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wideUri = converter.from_bytes(uri);
	w->webView->Navigate(wideUri.c_str());
}

void uiWebViewEval(uiWebView *w, const char *js)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wideJs = converter.from_bytes(js);
	w->webView->ExecuteScript(wideJs.c_str(), nullptr);
}

uiWebView *uiNewWebView(void)
{
	uiWebView *w;
	HRESULT hr;

	printf("uiNewWebView\n");

	uiWindowsNewControl(uiWebView, w);

	hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
		[w](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
			w->env = env;
			printf("CreateCoreWebView2EnvironmentWithOptions\n");
			return env->CreateCoreWebView2Controller(w->hwnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				[w](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
					printf("CreateCoreWebView2Controller\n");
					w->controller = controller;
					controller->get_CoreWebView2(&w->webView);
					return S_OK;
				}).Get());
		}).Get());

	if (hr != S_OK) {
		logHRESULT(L"error creating webview", hr);
		return NULL;
	}

	printf("uiNewWebView done\n");

	return w;
}
