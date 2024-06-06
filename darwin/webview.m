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
	NSMutableArray *urlSchemeHandlers;
	void (*onMessage)(uiWebView *w, const char *msg, void *data);
	void *onMessageData;
};

static void uiWebViewReset(uiWebView *w)
{
	if (w->webview != nil) {
		[w->webview removeFromSuperview];
		[w->webview release];
		w->webview = nil;
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
}

uiDarwinControlAllDefaults(uiWebView, view)

void uiWebViewEnableDevTools(uiWebView *w, int enable)
{
	if (enable) {
		[[w->config preferences] setValue:@YES forKey:@"developerExtrasEnabled"];
	} else {
		[[w->config preferences] setValue:@NO forKey:@"developerExtrasEnabled"];
	}
}

void uiWebViewSetInitScript(uiWebView *w, const char *script)
{
	NSString *scriptString = [NSString stringWithUTF8String:script];
	WKUserScript *userScript = [[WKUserScript alloc] initWithSource:scriptString injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES];
	[[w->config userContentController] addUserScript:userScript];
}

void uiWebViewOnMessage(uiWebView *w, void (*f)(uiWebView *w, const char *msg, void *data), void *data)
{
	w->onMessage = f;
	w->onMessageData = data;
}

void uiWebViewRegisterUriScheme(uiWebView *w, const char *scheme, void (*f)(void *request, void *data), void *userData)
{
	uiWebViewURLSchemeHandler *handler = [[uiWebViewURLSchemeHandler alloc] init];
	handler.f = f;
	handler.userData = userData;

	if (@available(macOS 10.13, *)) {
		[w->config setURLSchemeHandler:handler forURLScheme:[NSString stringWithUTF8String:scheme]];
	}

	if (w->urlSchemeHandlers == nil) {
		w->urlSchemeHandlers = [[NSMutableArray alloc] init];
	}

	[w->urlSchemeHandlers addObject:handler];

	uiWebViewReset(w);
}

const char *uiWebViewRequestGetScheme(void *request)
{
	NSURLRequest *req = (NSURLRequest *)request;
	return [[[req URL] scheme] UTF8String];
}

const char *uiWebViewRequestGetUri(void *request)
{
	NSURLRequest *req = (NSURLRequest *)request;
	return [[[req URL] absoluteString] UTF8String];
}

const char *uiWebViewRequestGetPath(void *request)
{
	NSURLRequest *req = (NSURLRequest *)request;
	return [[[req URL] path] UTF8String];
}

void uiWebViewRequestRespond(void *request, const char *body, const char *contentType)
{
	NSURLRequest *req = (NSURLRequest *)request;
	NSURLResponse *response = [[NSURLResponse alloc] initWithURL:[req URL] MIMEType:[NSString stringWithUTF8String:contentType] expectedContentLength:strlen(body) textEncodingName:nil];
	[[NSURLCache sharedURLCache] storeCachedResponse:[[NSCachedURLResponse alloc] initWithResponse:response data:[NSData dataWithBytes:body length:strlen(body)]] forRequest:req];
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

uiWebView *uiNewWebView()
{
	uiWebView *w;

	uiDarwinNewControl(uiWebView, w);

	w->view = [[NSView alloc] initWithFrame:NSZeroRect];
	w->config = [[WKWebViewConfiguration alloc] init];
	w->msgHandler = [[uiWebViewScriptMessageHandler alloc] init];
	w->msgHandler.w = w;
	[[w->config userContentController] addScriptMessageHandler:w->msgHandler name:@"webview"];

	uiWebViewReset(w);

	return w;
}

@implementation uiWebViewURLSchemeHandler

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask API_AVAILABLE(macos(10.13))
{
    // Handle custom scheme request
    NSURL *url = urlSchemeTask.request.URL;
    NSLog(@"Handling custom URL: %@", url);

    // Provide a simple response
    NSString *responseString = [NSString stringWithFormat:@"Handled by custom scheme handler: %@", url.absoluteString];
    NSData *responseData = [responseString dataUsingEncoding:NSUTF8StringEncoding];

    NSURLResponse *response = [[NSURLResponse alloc] initWithURL:url
                                                        MIMEType:@"text/plain"
                                           expectedContentLength:responseData.length
                                                textEncodingName:@"utf-8"];

    [urlSchemeTask didReceiveResponse:response];
    [urlSchemeTask didReceiveData:responseData];
    [urlSchemeTask didFinish];
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
