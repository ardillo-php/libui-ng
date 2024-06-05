#include "uipriv_unix.h"
#include <webkit2/webkit2.h>

struct uiWebView {
	uiUnixControl c;
	GtkWidget *widget;
	WebKitWebView *webview;
	WebKitUserContentManager *manager;
	void (*onMessage)(uiWebView *w, const char *msg, void *data);
	void *onMessageData;
};

uiUnixControlAllDefaults(uiWebView)

void uiWebViewEnableDevTools(uiWebView *w, int enable)
{
	WebKitSettings *settings = webkit_web_view_get_settings(w->webview);
	webkit_settings_set_enable_developer_extras(settings, enable);
}

void uiWebViewSetInitScript(uiWebView *w, const char *script)
{
	WebKitUserScript *userScript = webkit_user_script_new(script, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL);
	webkit_user_content_manager_add_script(w->manager, userScript);
	webkit_user_script_unref(userScript);
}

void uiWebViewOnMessage(uiWebView *w, void (*f)(uiWebView *w, const char *msg, void *data), void *data)
{
	w->onMessage = f;
	w->onMessageData = data;
}

void uiWebViewRegisterUriScheme(uiWebView *w, const char *scheme, void (*f)(void *request, void *data), void *userData)
{
    WebKitWebContext *context = webkit_web_view_get_context(w->webview);
    WebKitSecurityManager *securityManager = webkit_web_context_get_security_manager(context);

    webkit_security_manager_register_uri_scheme_as_secure(securityManager, scheme);
    webkit_security_manager_register_uri_scheme_as_cors_enabled(securityManager, scheme);

    void (*gf)(WebKitURISchemeRequest *, void *) = (void (*)(WebKitURISchemeRequest *, void *))f;

    webkit_web_context_register_uri_scheme(context, scheme, gf, userData, NULL);
}

const char *uiWebViewRequestGetScheme(void *request)
{
    return webkit_uri_scheme_request_get_scheme((WebKitURISchemeRequest *)request);
}

const char *uiWebViewRequestGetUri(void *request)
{
    return webkit_uri_scheme_request_get_uri((WebKitURISchemeRequest *)request);
}

const char *uiWebViewRequestGetPath(void *request)
{
    return webkit_uri_scheme_request_get_path((WebKitURISchemeRequest *)request);
}

void uiWebViewRequestRespond(void *request, const char *body, size_t length, const char *contentType)
{
    GInputStream *stream = g_memory_input_stream_new_from_data(g_memdup2(body, length), length, g_free);
    webkit_uri_scheme_request_finish((WebKitURISchemeRequest *)request, stream, length, contentType);
    g_object_unref(stream);
}

void uiWebViewSetHtml(uiWebView *w, const char *html)
{
	webkit_web_view_load_html(w->webview, html, NULL);
}

void uiWebViewSetUri(uiWebView *w, const char *uri)
{
	webkit_web_view_load_uri(w->webview, uri);
}

void uiWebViewEval(uiWebView *w, const char *js)
{
	webkit_web_view_evaluate_javascript(w->webview, js, -1, NULL, NULL, NULL, NULL, NULL);
}

static void scriptMessageReceived(WebKitUserContentManager *manager, WebKitJavascriptResult *result, uiWebView *w)
{
	JSCValue *arg = webkit_javascript_result_get_js_value(result);
	const char *message = jsc_value_to_string(arg);

	if (!message) {
		return;
	}

	if (w->onMessage) {
		(*(w->onMessage))(w, message, w->onMessageData);
	}
}

uiWebView *uiNewWebView()
{
	uiWebView *w;

	uiUnixNewControl(uiWebView, w);

	w->webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
	w->widget = GTK_WIDGET(w->webview);
	w->manager = webkit_web_view_get_user_content_manager(w->webview);

	webkit_user_content_manager_register_script_message_handler(w->manager, "webview");
	g_signal_connect(w->manager, "script-message-received::webview", G_CALLBACK(scriptMessageReceived), w);

	return w;
}
