/*
mkdir deps\webview2
curl -sSL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2" | tar -xf - -C deps\webview2
*/

#include "uipriv_windows.hpp"
#include "deps/webview2/build/native/include/WebView2.h"
#include <wrl.h>

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

// uiWebViewNew creates a new WebView control
uiWebView *uiWebViewNew(void)
{
	uiWebView *w;
	HRESULT hr;

	uiWindowsNewControl(uiWebView, w);

	hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
		[w](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
			w->env = env;
			return env->CreateCoreWebView2Controller(w->hwnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				[w](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
					w->controller = controller;
					controller->get_CoreWebView2(&w->webView);
					return S_OK;
				}).Get());
		}).Get());

	if (hr != S_OK) {
		// TODO
	}

	return w;
}
