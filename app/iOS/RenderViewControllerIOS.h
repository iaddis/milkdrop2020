
#pragma once

#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>
#import <GLKit/GLKit.h>
#import "../CommonApple/Visualizer.h"


@interface RenderViewControllerIOS : UIViewController <MTKViewDelegate, GLKViewDelegate>


@property (strong, nonatomic) IBOutlet Visualizer * _Nullable visualizer;


@end

