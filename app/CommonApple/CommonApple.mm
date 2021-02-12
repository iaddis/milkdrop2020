
#import <Foundation/Foundation.h>

#include "CommonApple.h"
#include "platform.h"

bool GetResourceDir(std::string &outdir)
{
    outdir = [[[NSBundle mainBundle] resourcePath] UTF8String];
    return true;
}

bool GetApplicationSupportDir(std::string &outdir)
{
    NSArray* paths = NSSearchPathForDirectoriesInDomains(
                                                            NSApplicationSupportDirectory,
                                                            NSUserDomainMask,
                                                            YES);
    if ([paths count] == 0)
    {
        return false;
    }

    
    NSString *bundleId = [NSBundle mainBundle].bundleIdentifier;
    NSString *resolvedPath = [paths objectAtIndex:0];
    NSString *dir = [resolvedPath stringByAppendingPathComponent:bundleId];
         
    NSError *error;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:dir
                               withIntermediateDirectories:YES
                                                attributes:nil
                                                     error:&error])
    {
        return false;
    }
    
    outdir = dir.UTF8String;
    return true;
}

