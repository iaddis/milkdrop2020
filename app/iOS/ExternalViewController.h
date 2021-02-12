

#pragma once

#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>

#import "RenderViewControllerIOS.h"
#include "render/context_metal.h"

@interface ExternalViewController : UIViewController <MTKViewDelegate> {
    
}


@property (strong, nonatomic) IBOutlet Visualizer * _Nullable visualizer;


@property (nonatomic) render::metal::IMetalContextPtr context;




-(void)draw;




@end

