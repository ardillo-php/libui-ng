#import "uipriv_darwin.h"
#import <WebKit/WebKit.h>

@interface uiWebViewURLSchemeHandler : NSObject <WKURLSchemeHandler>
@property void (*f)(void *request, void *data);
@property void *userData;
@end

@interface uiWebViewScriptMessageHandler : NSObject <WKScriptMessageHandler>
@property uiWebView *w;
@end

struct uiWebView {
	uiDarwinControl c;
	NSView *view;
	WKWebView *webview;
	WKWebViewConfiguration *config;
	uiWebViewScriptMessageHandler *msgHandler;
	void (*onMessage)(uiWebView *w, const char *msg, void *data);
	void *onMessageData;
	void (*onRequest)(uiWebView *w, void *request, void *data);
	void *onRequestData;
};

uiDarwinControlAllDefaults(uiWebView, view)

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
	id<WKURLSchemeTask> task = (id<WKURLSchemeTask>)request;
	return [[[task request].URL absoluteString] UTF8String];
}

void uiWebViewRequestRespond(uiWebView *w, void *request, const char *body, size_t length, const char *contentType)
{
	id<WKURLSchemeTask> task = (id<WKURLSchemeTask>)request;
	NSURLRequest *req = [task request];
	NSURLResponse *response = [[NSURLResponse alloc] initWithURL:[req URL] MIMEType:[NSString stringWithUTF8String:contentType] expectedContentLength:length textEncodingName:nil];

	[task didReceiveResponse:response];
	[task didReceiveData:[NSData dataWithBytes:body length:length]];
	[task didFinish];
}

void uiWebViewSetHtml(uiWebView *w, const char *html)
{
	[w->webview loadHTMLString:[NSString stringWithUTF8String:html] baseURL:nil];
}

void uiWebViewSetUri(uiWebView *w, const char *uri)
{
	[w->webview loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithUTF8String:uri]]]];
}

void uiWebViewEval(uiWebView *w, const char *js)
{
	[w->webview evaluateJavaScript:[NSString stringWithUTF8String:js] completionHandler:nil];
}

static void customRequestReceived(id<WKURLSchemeTask> request, void *data) API_AVAILABLE(macos(10.13))
{
	uiWebView *w = (uiWebView *)data;

	if (w->onRequest != NULL) {
		w->onRequest(w, request, w->onRequestData);
	}
}

uiWebView *uiNewWebView(uiWebViewParams *p)
{
	uiWebView *w;

	uiDarwinNewControl(uiWebView, w);

	w->view = [[NSView alloc] initWithFrame:NSZeroRect];
	w->config = [[WKWebViewConfiguration alloc] init];
	w->msgHandler = [[uiWebViewScriptMessageHandler alloc] init];
	w->msgHandler.w = w;
	[[w->config userContentController] addScriptMessageHandler:w->msgHandler name:@"webview"];

	if (p->EnableDevTools) {
		[[w->config preferences] setValue:@YES forKey:@"developerExtrasEnabled"];
	} else {
		[[w->config preferences] setValue:@NO forKey:@"developerExtrasEnabled"];
	}

	if (p->InitScript) {
		NSString *scriptString = [NSString stringWithUTF8String:p->InitScript];
		WKUserScript *userScript = [[WKUserScript alloc] initWithSource:scriptString injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES];
		[[w->config userContentController] addUserScript:userScript];
	}

	if (p->CustomUriSchemes) {
		uiWebViewURLSchemeHandler *handler = [[uiWebViewURLSchemeHandler alloc] init];
		char *schemes = strdup(p->CustomUriSchemes);
		char *scheme = strtok(schemes, ",");

		if (@available(macOS 10.13, *)) {
			handler.f = (void (*)(void *, void *))customRequestReceived;
			handler.userData = w;
		}

		while (scheme) {
			if (@available(macOS 10.13, *)) {
				[w->config setURLSchemeHandler:handler forURLScheme:[NSString stringWithUTF8String:scheme]];
			}

			scheme = strtok(NULL, ",");
		}

		free(schemes);
	}

	if (p->EnableFullScreen) {
		[[w->config preferences] setValue:@YES forKey:@"fullScreenEnabled"];
	} else {
		[[w->config preferences] setValue:@NO forKey:@"fullScreenEnabled"];
	}

	w->webview = [[WKWebView alloc] initWithFrame:w->view.bounds configuration:w->config];
	[w->view addSubview:w->webview];
	[w->webview setTranslatesAutoresizingMaskIntoConstraints:NO];

	NSLayoutConstraint *leadingConstraint = [NSLayoutConstraint constraintWithItem:w->webview
											attribute:NSLayoutAttributeLeading
											relatedBy:NSLayoutRelationEqual
											toItem:w->view
											attribute:NSLayoutAttributeLeading
											multiplier:1.0
											constant:0.0];

	NSLayoutConstraint *trailingConstraint = [NSLayoutConstraint constraintWithItem:w->webview
											attribute:NSLayoutAttributeTrailing
											relatedBy:NSLayoutRelationEqual
											toItem:w->view
											attribute:NSLayoutAttributeTrailing
											multiplier:1.0
											constant:0.0];

	NSLayoutConstraint *topConstraint = [NSLayoutConstraint constraintWithItem:w->webview
											attribute:NSLayoutAttributeTop
											relatedBy:NSLayoutRelationEqual
											toItem:w->view
											attribute:NSLayoutAttributeTop
											multiplier:1.0
											constant:0.0];

	NSLayoutConstraint *bottomConstraint = [NSLayoutConstraint constraintWithItem:w->webview
											attribute:NSLayoutAttributeBottom
											relatedBy:NSLayoutRelationEqual
											toItem:w->view
											attribute:NSLayoutAttributeBottom
											multiplier:1.0
											constant:0.0];

	[w->view addConstraints:@[leadingConstraint, trailingConstraint, topConstraint, bottomConstraint]];

	return w;
}

@implementation uiWebViewURLSchemeHandler

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask API_AVAILABLE(macos(10.13))
{
	if (self.f != NULL) {
		self.f(urlSchemeTask, self.userData);
	}
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask API_AVAILABLE(macos(10.13))
{
}

@end

@implementation uiWebViewScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
	if (self.w->onMessage != NULL) {
		self.w->onMessage(self.w, [message.body UTF8String], self.w->onMessageData);
	}
}

@end
