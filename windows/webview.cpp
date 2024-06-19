/*
mkdir deps\webview2
curl -sSL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2" | tar -xf - -C deps\webview2
*/

#include "uipriv_windows.hpp"
#include "deps/webview2/build/native/include/WebView2.h"
#include <wrl.h>
#include <locale>
#include <codecvt>
#include <iostream>
#include <future>

#pragma comment(lib, "..\\deps\\webview2\\build\\native\\x64\\WebView2Loader.dll.lib")

using namespace Microsoft::WRL;

struct uriSchemeHandler {
	void (*handler)(void *request, void *data);
	void *userData;
};

struct uiWebView {
	uiWindowsControl c;
	HWND hwnd;
	ICoreWebView2Environment *env;
	ICoreWebView2Controller *controller;
	ICoreWebView2 *webView;
	ICoreWebView2Settings *settings;
	std::map<std::string, uriSchemeHandler> uriSchemeHandlers;
	void (*onMessage)(uiWebView *w, const char *msg, void *data);
	void *onMessageData;
};

uiWindowsControlAllDefaults(uiWebView)

static void uiWebViewMinimumSize(uiWindowsControl *c, int *width, int *height)
{
}

void uiWebViewEnableDevTools(uiWebView *w, int enable)
{
	w->settings->put_AreDevToolsEnabled(enable);
}

void uiWebViewSetInitScript(uiWebView *w, const char *script)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wideScript = converter.from_bytes(script);
	w->webView->AddScriptToExecuteOnDocumentCreated(wideScript.c_str(), nullptr);
}

void uiWebViewOnMessage(uiWebView *w, void (*f)(uiWebView *w, const char *msg, void *data), void *data)
{
	w->onMessage = f;
	w->onMessageData = data;
}

void uiWebViewRegisterUriScheme(uiWebView *w, const char *scheme, void (*f)(void *request, void *data), void *userData)
{
	/*
	printf("uiWebViewRegisterUriScheme: %s\n", scheme);
	printf("# of schemes: %d\n", w->uriSchemeHandlers.size());
	Sleep(15000);
	w->uriSchemeHandlers[scheme] = {f, userData};
	printf("uiWebViewRegisterUriScheme: %s done!\n", scheme);
	*/
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

static LRESULT CALLBACK webViewWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	uiWebView *w = (uiWebView *) GetWindowLongPtrW(hwnd, GWLP_USERDATA);

	if (w == NULL) {
		if (uMsg == WM_CREATE) {
			CREATESTRUCTW *cs = (CREATESTRUCTW *) lParam;
			w = (uiWebView *) (cs->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) w);
		}
	} else {
		if (uMsg == WM_SIZE) {
			if (w->controller) {
				RECT bounds;
				GetClientRect(hwnd, &bounds);
				w->controller->put_Bounds(bounds);
			}
		}
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

ATOM registerWebViewClass(HICON hDefaultIcon, HCURSOR hDefaultCursor)
{
	WNDCLASSW wc;

	ZeroMemory(&wc, sizeof (WNDCLASSW));
	wc.lpszClassName = webViewClass;
	wc.lpfnWndProc = webViewWndProc;
	wc.hInstance = hInstance;
	wc.hIcon = hDefaultIcon;
	wc.hCursor = hDefaultCursor;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.style = CS_HREDRAW | CS_VREDRAW;

	return RegisterClassW(&wc);
}

void unregisterWebView(void)
{
	UnregisterClassW(webViewClass, hInstance);
}

uiWebView *uiNewWebViewSync(void)
{
	std::promise<void> p;
	std::future<void> f = p.get_future();
	uiWebView *w;
	HRESULT hr;

	uiWindowsNewControl(uiWebView, w);

	w->hwnd = uiWindowsEnsureCreateControlHWND(0,
		webViewClass, L"",
		0,
		hInstance, w,
		FALSE);

	hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[w, &p](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
				if (result != S_OK) {
					p.set_exception(std::make_exception_ptr(std::runtime_error("error creating WebView2 environment")));
					return result;
				}

				w->env = env;

				HRESULT hr = w->env->CreateCoreWebView2Controller(w->hwnd,
					Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
						[w, &p](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
							if (result != S_OK || controller == nullptr) {
								p.set_exception(std::make_exception_ptr(std::runtime_error("error creating WebView2 controller")));
								return result;
							}

							w->controller = controller;
							w->controller->AddRef();

							HRESULT hr = w->controller->get_CoreWebView2(&w->webView);
							if (hr != S_OK) {
								p.set_exception(std::make_exception_ptr(std::runtime_error("error getting WebView2 interface")));
								return hr;
							}

							RECT bounds;
							GetClientRect(w->hwnd, &bounds);
							w->controller->put_Bounds(bounds);
							w->controller->put_IsVisible(TRUE);
							w->webView->get_Settings(&w->settings);

							w->webView->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
								[w](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
									WCHAR *msg;
									args->TryGetWebMessageAsString(&msg);

									if (msg && w->onMessage) {
										std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
										std::string narrowMsg = converter.to_bytes(msg);
										w->onMessage(w, narrowMsg.c_str(), w->onMessageData);
									}

									return S_OK;
								}).Get(), nullptr);

							w->webView->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
							w->webView->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
								[w](ICoreWebView2 *sender, ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT {
									ICoreWebView2WebResourceRequest *request;
									args->get_Request(&request);

									LPWSTR uri;
									request->get_Uri(&uri);

									printf("uri: %ls\n", uri);

									std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
									std::string uriStr = converter.to_bytes(uri);
									std::string scheme = uriStr.substr(0, uriStr.find(":"));
									CoTaskMemFree(uri);
/*
									auto it = w->uriSchemeHandlers.find(scheme);
									if (it != w->uriSchemeHandlers.end()) {
										it->second.handler(request, it->second.userData);
										return S_OK;
									}
*/
									return S_OK;
								}).Get(), nullptr);

							p.set_value();

							return S_OK;
						}).Get());

				if (hr != S_OK) {
					p.set_exception(std::make_exception_ptr(std::runtime_error("error creating WebView2 controller")));
					return hr;
				}

				return S_OK;
			}).Get());

	try {
		while (f.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
			MSG msg;

			while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
	} catch (std::exception &e) {
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::wstring wideError = converter.from_bytes(e.what());
		return NULL;
	}

	if (hr != S_OK) {
		return NULL;
	}

	w->settings->put_AreDevToolsEnabled(TRUE);
	w->webView->OpenDevToolsWindow();

	return w;
}

uiWebView *uiNewWebView(void)
{
	return uiNewWebViewSync();
}
