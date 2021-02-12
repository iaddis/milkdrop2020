
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import "../CommonApple/Visualizer.h"
#include "render/context_metal.h"
#import "MetalView.h"
#import "GLView.h"

// Our macOS view controller.
@interface RenderViewControllerOSX : NSViewController <MTKViewDelegate, GLViewDelegate, EventDelegate>

@property (strong, nonatomic) IBOutlet id<RenderViewDelegate>  _Nullable visualizer;


@end
