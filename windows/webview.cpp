#include "uipriv_windows.hpp"
#include "WebView2.h"
#include "WebView2EnvironmentOptions.h"
#include <wrl.h>
#include <locale>
#include <codecvt>
#include <iostream>
#include <future>
#include <Shlwapi.h>

using namespace Microsoft::WRL;

struct uiWebView {
	uiWindowsControl c;
	HWND hwnd;
	ICoreWebView2Environment *env;
	ICoreWebView2Controller *controller;
	ICoreWebView2 *webView;
	ICoreWebView2Settings *settings;
	void (*onMessage)(uiWebView *w, const char *msg, void *data);
	void *onMessageData;
	void (*onRequest)(uiWebView *w, void *request, void *data);
	void *onRequestData;
};

uiWindowsControlAllDefaults(uiWebView)

static void uiWebViewMinimumSize(uiWindowsControl *c, int *width, int *height)
{
}

void uiWebViewOnMessage(uiWebView *w, void (*f)(uiWebView *w, const char *msg, void *data), void *data)
{
	w->onMessage = f;
	w->onMessageData = data;
}

void uiWebViewOnRequest(uiWebView *w, void (*f)(uiWebView *w, void *request, void *data), void *data)
{
	w->onRequest = f;
	w->onRequestData = data;
}

const char *uiWebViewRequestGetUri(void *request)
{
	ICoreWebView2WebResourceRequestedEventArgs *args = (ICoreWebView2WebResourceRequestedEventArgs *)request;
	ICoreWebView2WebResourceRequest *req;
	WCHAR *uri;
	char *ret;

	args->get_Request(&req);
	req->get_Uri(&uri);

	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::string narrowUri = converter.to_bytes(uri);

	ret = _strdup(narrowUri.c_str());

	return ret;
}

void uiWebViewRequestRespond(uiWebView *w, void *request, const char *body, size_t length, const char *contentType)
{
	ICoreWebView2WebResourceResponse *response;
	ICoreWebView2WebResourceRequestedEventArgs *args = (ICoreWebView2WebResourceRequestedEventArgs *)request;
	ICoreWebView2WebResourceRequest *req;

	args->get_Request(&req);

	WCHAR *uri;
	req->get_Uri(&uri);

	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wideContentType = converter.from_bytes(contentType);

	IStream *stream = SHCreateMemStream((BYTE *)body, length);
	w->env->CreateWebResourceResponse(stream, 200, L"OK", (L"Content-Type: " + wideContentType).c_str(), &response);
	args->put_Response(response);

	response->Release();
	stream->Release();
	args->Release();
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

uiWebView *uiNewWebView(uiWebViewParams *p)
{
	std::promise<void> promise;
	std::future<void> f = promise.get_future();
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	uiWebView *w;
	HRESULT hr;
	auto woptions = Make<CoreWebView2EnvironmentOptions>();
	ComPtr<ICoreWebView2EnvironmentOptions4> woptions4;

	uiWindowsNewControl(uiWebView, w);

	w->hwnd = uiWindowsEnsureCreateControlHWND(0,
		webViewClass, L"",
		0,
		hInstance, w,
		FALSE);

	woptions.As(&woptions4);

	if (p->CustomUriSchemes) {
		char *schemes = _strdup(p->CustomUriSchemes);
		char *scheme = strtok(schemes, ",");

		std::vector<ICoreWebView2CustomSchemeRegistration*> registrations;

		while (scheme) {
			auto reg = Make<CoreWebView2CustomSchemeRegistration>(converter.from_bytes(scheme).c_str());
			reg->AddRef();
			reg->put_HasAuthorityComponent(TRUE);
			reg->put_TreatAsSecure(TRUE);

			registrations.push_back(reg.Get());

			scheme = strtok(NULL, ",");
		}

		woptions4->SetCustomSchemeRegistrations(registrations.size(), registrations.data());
	}

	hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, woptions.Get(),
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[w, &promise](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
				if (result != S_OK) {
					promise.set_exception(std::make_exception_ptr(std::runtime_error("error creating WebView2 environment")));
					return result;
				}

				w->env = env;

				HRESULT hr = w->env->CreateCoreWebView2Controller(w->hwnd,
					Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
						[w, &promise](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
							if (result != S_OK || controller == nullptr) {
								promise.set_exception(std::make_exception_ptr(std::runtime_error("error creating WebView2 controller")));
								return result;
							}

							w->controller = controller;
							w->controller->AddRef();

							HRESULT hr = w->controller->get_CoreWebView2(&w->webView);
							if (hr != S_OK) {
								promise.set_exception(std::make_exception_ptr(std::runtime_error("error getting WebView2 interface")));
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

							w->webView->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
								[w](ICoreWebView2 *sender, ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT {
									if (args && w->onRequest) {
										args->AddRef();
										w->onRequest(w, args, w->onRequestData);
									}

									return S_OK;
								}).Get(), nullptr);

							promise.set_value();

							return S_OK;
						}).Get());

				if (hr != S_OK) {
					promise.set_exception(std::make_exception_ptr(std::runtime_error("error creating WebView2 controller")));
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
		std::wstring wideError = converter.from_bytes(e.what());
		return NULL;
	}

	if (hr != S_OK) {
		return NULL;
	}

	w->settings->put_AreDevToolsEnabled(p->EnableDevTools);

	if (p->InitScript) {
		std::wstring wideScript = converter.from_bytes(p->InitScript);
		w->webView->AddScriptToExecuteOnDocumentCreated(wideScript.c_str(), nullptr);
	}

	if (p->CustomUriSchemes) {
		char *schemes = _strdup(p->CustomUriSchemes);
		char *scheme = strtok(schemes, ",");

		while (scheme) {
			char *pattern = (char *)malloc(strlen(scheme) + 2);
			strcpy(pattern, scheme);
			strcat(pattern, ":*");

			w->webView->AddWebResourceRequestedFilter(converter.from_bytes(pattern).c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

			scheme = strtok(NULL, ",");
		}
	}

	w->settings->put_IsFullscreenAllowed(p->EnableFullScreen);

	return w;
}
