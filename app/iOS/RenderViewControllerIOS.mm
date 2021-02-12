

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <GLKit/GLKit.h>

#import "RenderViewControllerIOS.h"
#include "VizController.h"
#include "audio/IAudioSource.h"

#include "context.h"
#include "context_metal.h"
#include "context_gles.h"
#include "keycode_osx.h"
#include "../external/imgui/imgui.h"
#include "../external/imgui/imgui_impl_osx.h"
#include "../CommonApple/CommonApple.h"
#import "../CommonApple/Visualizer.h"

#import "ExternalViewController.h"

bool UseGL = false;

// load texture with GLKit
static bool LoadTextureFromFile(const char *path, render::gles::GLTextureInfo &ti)
{
    NSError *error = nil;
    GLKTextureInfo *info =[GLKTextureLoader
                           textureWithContentsOfFile:[NSString stringWithUTF8String:path]
                           options:nil
                           error:&error];
    if (!info)
        return false;
    ti.name = info.name;
    ti.width = info.width;
    ti.height = info.height;
    return true;
}


@implementation RenderViewControllerIOS
{
    MTKView *_metal_view;
    GLKView *_gl_view;
    UIWindow * _externalWindow;
    ExternalViewController * _externalWindowController;
    render::ContextPtr _context;
    render::ContextPtr _gl_context;
    render::metal::IMetalContextPtr _metal_context;
    int _screenCount;
}

#if !TARGET_OS_TV

-(BOOL)prefersStatusBarHidden
{
    return YES;
}
#endif

-(BOOL)prefersHomeIndicatorAutoHidden
{
    return YES;
}


- (void)viewDidLoad
{
    PROFILE_FUNCTION()

    [super viewDidLoad];
    
    _screenCount = 1;

    
   if (!UseGL)
   {
       [self initMetal];
   }
   else
   {
       [self initGL];
   }
   
   self.visualizer = [[Visualizer alloc] initWithContext:_context];
    

    [self setupScreens];
    [self setupGestures];
}

-(void)initGL
{
    PROFILE_FUNCTION()

    EAGLContext *eagl_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    
    GLKView * view = [[GLKView alloc] initWithFrame:self.view.frame context:eagl_context];
    
    view.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
    view.drawableDepthFormat = GLKViewDrawableDepthFormatNone;
    view.drawableStencilFormat = GLKViewDrawableStencilFormatNone;
    view.drawableMultisample   = GLKViewDrawableMultisampleNone;
    
    
    view.delegate = self;
    self.view = view;
    _gl_view = view;
    
    
    [view bindDrawable];
    
    _gl_context = render::gles::GLCreateContext(LoadTextureFromFile);
    _gl_context->SetRenderTarget(nullptr);
    
    _context = _gl_context;

}

-(void)initMetal
{
    PROFILE_FUNCTION()

    id <MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device)
    {
        NSLog(@"Metal is not supported on this device");
        return;
    }
    MTKView *view = [[MTKView alloc] initWithFrame:self.view.frame device:device];;
    
    // configure view
//    view.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

#if 0
    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
#else
    
#if TARGET_OS_SIMULATOR
    view.colorPixelFormat = MTLPixelFormatRGBA16Float;
//    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
//    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
//    view.colorPixelFormat = MTLPixelFormatBGR10_XR;

#else
    view.colorPixelFormat = MTLPixelFormatBGR10_XR;
//    view.colorPixelFormat = MTLPixelFormatRGB9E5Float;
    
//        view.colorPixelFormat = MTLPixelFormatBGR10_XR;
//    view.colorPixelFormat = MTLPixelFormatRGBA16Float;

#endif
    

#endif
    
    view.depthStencilPixelFormat = MTLPixelFormatInvalid;
    view.sampleCount = 1;
    view.preferredFramesPerSecond = 60;
    view.delegate = self;
//    view.eventDelegate = self;
//    [view registerDragDrop];
    

    self.view = view;
    
    
    
    

        if (@available(tvOS 13.0, iOS 13.0, *))
        {
        
            CAMetalLayer *layer = (CAMetalLayer *)view.layer;
    //        layer.wantsExtendedDynamicRangeContent= YES;
    //        layer.pixelFormat = view.colorPixelFormat;
            const CFStringRef name = kCGColorSpaceExtendedSRGB;
    //        const CFStringRef name = kCGColorSpaceExtendedLinearSRGB;
        //    const CFStringRef name = kCGColorSpaceSRGB;
        //    const CFStringRef name = kCGColorSpaceSRGB;

            CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(name);
            layer.colorspace = colorspace;
            CGColorSpaceRelease(colorspace);
        }

    
    
    
    
    _metal_view = view;
    

    _metal_context = render::metal::MetalCreateContext(device);
    _metal_context->SetView( view );
    _metal_context->SetRenderTarget(nullptr);
    
    _context = _metal_context;

}



-(void)setupScreens
{
    auto screens = [UIScreen screens];
    if (screens.count > 1) {
        [self setupExternalScreen:screens[1]];
    }
    
    
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(handleScreenConnectNotification:) name:UIScreenDidConnectNotification object:nil];
    [center addObserver:self selector:@selector(handleScreenDisconnectNotification:) name:UIScreenDidDisconnectNotification object:nil];
    

}



- (void)handleScreenConnectNotification:(NSNotification*)notification {
    NSLog(@"Screen connected!");    
    UIScreen *screen = (UIScreen *)notification.object;
    [self setupExternalScreen:screen];
}

- (void)handleScreenDisconnectNotification:(NSNotification*)notification {
    NSLog(@"Screen disconnected");
    UIScreen *screen = (UIScreen *)notification.object;
    [self shutdownExternalScreen:screen];
}


-(void)setupExternalScreen:(UIScreen *)screen
{
    if (!_metal_context)
        return;
    
    if (_externalWindow)
        return;
    
    UIStoryboard *sb = self.storyboard;
    _externalWindowController = (ExternalViewController *)[sb  instantiateViewControllerWithIdentifier:@"ExternalWindowScene"];
    _externalWindowController.context = _metal_context;
    _externalWindowController.visualizer = self.visualizer;

    
    _externalWindow = [[UIWindow alloc] initWithFrame:screen.bounds];
    _externalWindow.screen = screen;
    _externalWindow.rootViewController = _externalWindowController;
    _externalWindow.hidden = NO;


    _screenCount = 2;
    NSLog(@"UIWindow: %@ screen:%fx%f window:%fx%f\n", _externalWindow,
          screen.bounds.size.width, screen.bounds.size.height,
             _externalWindow.bounds.size.width,
             _externalWindow.bounds.size.height
             );


}

-(void)shutdownExternalScreen:(UIScreen *)screen
{
    if (!_externalWindow)
        return;
    
    if (_externalWindow.screen != screen)
        return;

    _externalWindowController = nil;
    _externalWindow.hidden = YES;
    _externalWindow = nil;
    _screenCount = 1;
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

-(void)respondToTapGesture:(UIGestureRecognizer *)gestureRecognizer
{
//    NSLog(@"tap!\n");
    
//    vizController->ToggleDebugMenu();
    
//    vizController->NavigateForward();
}

-(void)respondToDoubleTapGesture:(UIGestureRecognizer *)gestureRecognizer
{
//    _vizController->NavigateForward();
}


-(void)respondToSwipeLeftGesture:(UIGestureRecognizer *)gestureRecognizer
{
//    _vizController->NavigateHistoryPrevious();
}



-(void)respondToSwipeRightGesture:(UIGestureRecognizer *)gestureRecognizer
{
//    _vizController->NavigateHistoryNext();
}


-(void)setupGestures
{
#if 0
    {
        // Create and initialize a tap gesture
        UITapGestureRecognizer *tapRecognizer = [[UITapGestureRecognizer alloc]
                                                 initWithTarget:self
                                                 action:@selector(respondToTapGesture:)];
        
    #if TARGET_OS_TV
        tapRecognizer.allowedPressTypes = @[[NSNumber numberWithInteger:UIPressTypeSelect], [NSNumber numberWithInteger:UIPressTypePlayPause]];
    #endif
        
        // Specify that the gesture must be a single tap
        tapRecognizer.numberOfTapsRequired = 1;
        
        // Add the tap gesture recognizer to the view
        [self.view addGestureRecognizer:tapRecognizer];
    }
#endif

#if 0
    {
           // Create and initialize a tap gesture
           UITapGestureRecognizer *r = [[UITapGestureRecognizer alloc]
                                                    initWithTarget:self
                                                    action:@selector(respondToDoubleTapGesture:)];
           
      
           
           // Specify that the gesture must be a single tap
           r.numberOfTapsRequired = 2;
           
           // Add the tap gesture recognizer to the view
           [self.view addGestureRecognizer:r];
       }
#endif
    
    
#if TARGET_OS_IOS && 0

    {
        UISwipeGestureRecognizer *r = [[UISwipeGestureRecognizer alloc]
                                                       initWithTarget:self
                                                       action:@selector(respondToSwipeLeftGesture:)];
        r.numberOfTouchesRequired = 2;
        r.direction = UISwipeGestureRecognizerDirectionLeft;
        [self.view addGestureRecognizer:r];
    }
    
    {
        UISwipeGestureRecognizer *r = [[UISwipeGestureRecognizer alloc]
                                                       initWithTarget:self
                                                       action:@selector(respondToSwipeRightGesture:)];
        r.numberOfTouchesRequired = 2;
        r.direction = UISwipeGestureRecognizerDirectionRight;
        [self.view addGestureRecognizer:r];
    }
#endif
    
    
}
- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    @autoreleasepool {
          // setup viewport
//          _gl_context->SetView(view);
         PROFILE_FRAME()

        [view bindDrawable];

        // setup viewport
        GLsizei width  = (int)view.drawableWidth;
        GLsizei height = (int)view.drawableHeight;
        CGFloat framebufferScale = view.window.screen.scale ?: UIScreen.mainScreen.scale;
        _context->SetDisplayInfo(
            {
            .size = Size2D(width, height),
            .format = render::PixelFormat::RGBA8Unorm,
            .refreshRate = 60.0f,
            .scale = (float)framebufferScale,
            .samples = 1,
            .maxEDR = 1.0f
            }
            );


        
        [self.visualizer draw:0 screenCount:_screenCount];

    }
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool {
        PROFILE_FRAME()

        // setup viewport
        _metal_context->SetView(view);
        
        [self.visualizer draw:0 screenCount:_screenCount];

    }
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
//    NSLog(@"drawableSizeWillChange: %@ %fx%f\n", view, size.width, size.height);

}


#if TARGET_OS_IOS

// This touch mapping is super cheesy/hacky. We treat any touch on the screen
// as if it were a depressed left mouse button, and we don't bother handling
// multitouch correctly at all. This causes the "cursor" to behave very erratically
// when there are multiple active touches. But for demo purposes, single-touch
// interaction actually works surprisingly well.
- (void)updateIOWithTouchEvent:(UIEvent *)event {
    UITouch *anyTouch = event.allTouches.anyObject;
    CGPoint touchLocation = [anyTouch locationInView:self.view];
    ImGuiIO &io = ImGui::GetIO();
    io.MousePos = ImVec2(touchLocation.x, touchLocation.y);
    
    BOOL hasActiveTouch = NO;
    for (UITouch *touch in event.allTouches) {
        if (touch.phase != UITouchPhaseEnded && touch.phase != UITouchPhaseCancelled) {
            hasActiveTouch = YES;
            break;
        }
    }
    io.MouseDown[0] = hasActiveTouch;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self updateIOWithTouchEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self updateIOWithTouchEvent:event];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self updateIOWithTouchEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self updateIOWithTouchEvent:event];
}

#endif





@end

