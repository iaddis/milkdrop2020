

#pragma once

#import <Foundation/Foundation.h>
#include "KeyEvent.h"
#include "render/context.h"

@protocol RenderViewDelegate <NSObject>
@required

-(render::ContextPtr)context;

-(void)draw:(int)screenId screenCount:(int)screenCount;

-(void)onDragDrop:(NSArray * _Nonnull)files;

-(bool)shouldQuit;

@end


@interface Visualizer : NSObject<RenderViewDelegate>


- (instancetype _Nonnull)initWithContext:(render::ContextPtr)context;

//-(render::ContextPtr)context;
//
//-(void)drawInSingleScreen;
//-(void)drawInDebugScreen;
//-(void)drawInExternalScreen;
//
//
//-(void)onDragDrop:(NSArray * _Nonnull)files;

@end


