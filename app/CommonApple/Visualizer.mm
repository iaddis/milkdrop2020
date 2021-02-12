
#import "Visualizer.h"
#import "CommonApple.h"
#include "VizController.h"


@implementation Visualizer 
{
    render::ContextPtr _context;
    VizControllerPtr _vizController;

}

- (instancetype _Nonnull)initWithContext:(render::ContextPtr)context
{
    self = [super init];
    if (self) {
        _context = context;
        
        std::string resourceDir;
        GetResourceDir(resourceDir);
        std::string pluginDir =  resourceDir + "/assets";
        std::string userDir;
        GetApplicationSupportDir(userDir);


        _vizController = CreateVizController(context, pluginDir, userDir);
          
    }
    return self;
}

-(render::ContextPtr)context
{
    return _context;
}

-(void)draw:(int)screenId screenCount:(int)screenCount
{
    // begin scene
    _context->BeginScene();
    _vizController->Render(screenId, screenCount);
    _context->EndScene();
    _context->Present();
}

-(bool)shouldQuit
{
    return _vizController->ShouldQuit();
}


-(void)onKeyDown:(KeyEvent)e
{
    _vizController->OnKeyDown(e);
}

-(void)onKeyUp:(KeyEvent)e
{
    _vizController->OnKeyUp(e);

}


-(void)onDragDrop:(NSArray * _Nonnull)files
{
    for (int i=0; i < files.count; i++)
    {
        NSString *file = files[i];
        _vizController->OnDragDrop( file.UTF8String );
    }

}

@end


