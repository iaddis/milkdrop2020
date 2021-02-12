
// dear imgui: standalone example application for Emscripten, using SDL2 + OpenGL3
// This is mostly the same code as the SDL2 + OpenGL3 example, simply with the modifications needed to run on Emscripten.
// It is possible to combine both code into a single source file that will compile properly on Desktop and using Emscripten.
// See https://github.com/ocornut/imgui/pull/2492 as an example on how to do just that.
//
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (Emscripten is a C++-to-javascript compiler, used to publish executables for the web. See https://emscripten.org/)

#include "../external/imgui/imgui.h"
#include "../external/imgui/imgui_impl_sdl.h"
#include "imgui_support.h"
#include <stdio.h>
#include <assert.h>
#include <emscripten.h>
#include <SDL.h>
#include <SDL_opengles2.h>
#include <stdlib.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <emscripten/html5.h>
#include <emscripten/trace.h>

#include "../external/imgui/imgui.h"

#include "render/context.h"
#include "render/context_gles.h"
#include "VizController.h"
#include "audio/IAudioSource.h"

using namespace render;





// Emscripten requires to have full control over the main loop. We're going to store our SDL book-keeping variables globally.
// Having a single function that acts as a loop prevents us to store state in the stack of said function. So we need some location for this.
static SDL_Window*     g_Window = NULL;
static SDL_GLContext   g_GLContext = NULL;
static ContextPtr _context;
static VizControllerPtr vizController;
static IAudioSourcePtr m_audioSource;


static bool LoadTextureFromFile(const char *path, render::gles::GLTextureInfo &ti)
{
    int width, height;
    void *data = emscripten_get_preloaded_image_data(path, &width, &height);
    if (!data)
        return false;
    
    GLuint name;
    glGenTextures(1, &name);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, name);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    
    free(data);
    
    ti.name = name;
    ti.width = width;
    ti.height = height;
    return true;
}



void main_loop(void* arg)
{
   SDL_GL_SetSwapInterval(1); // Enable vsync
    
    ImGuiIO& io = ImGui::GetIO();
    IM_UNUSED(arg); // We can pass this argument as the second parameter of emscripten_set_main_loop_arg(), but we don't use that.
    
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_MOUSEWHEEL:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_TEXTINPUT:
                ImGui_ImplSDL2_ProcessEvent(&event);
                break;
                
            case SDL_KEYUP:
            case SDL_KEYDOWN:
            {
//                printf("key: %c %c\n", event.key.keysym.scancode, event.key.keysym.sym);
                
                KeyEvent ke;
                ke.c = 0; //(char) event.key.keysym.mod;
//                ke.code = (KeyCode)event.key.keysym.sym;
                ke.code = (KeyCode)event.key.keysym.scancode;

                ke.KeyShift = ((event.key.keysym.mod & KMOD_SHIFT) != 0);
                ke.KeyCtrl = ((event.key.keysym.mod & KMOD_CTRL) != 0);
                ke.KeyAlt = ((event.key.keysym.mod & KMOD_ALT) != 0);
                ke.KeyCommand = ((event.key.keysym.mod & KMOD_GUI) != 0);
                if (event.type == SDL_KEYDOWN)
                    vizController->OnKeyDown(ke);
                else
                    vizController->OnKeyUp(ke);
                break;
            }
        }

        // Capture events here, based on io.WantCaptureMouse and io.WantCaptureKeyboard
    }
    
    // Rendering
    SDL_GL_MakeCurrent(g_Window, g_GLContext);

    float dpr = (float)emscripten_get_device_pixel_ratio();

    int width, height;
    SDL_GetWindowSize(g_Window, &width, &height);
    emscripten_set_canvas_element_size("canvas", width, height);

    int buffer_width, buffer_height;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_context = emscripten_webgl_get_current_context();
    emscripten_webgl_get_drawing_buffer_size(webgl_context, &buffer_width, &buffer_height);
    Size2D displaySize(buffer_width, buffer_height);


    float scale = (int)displaySize.width / (int)width;
    
    render::DisplayInfo info;
    info.size= displaySize;
    info.format= render::PixelFormat::RGBA8Unorm;
    info.refreshRate = 60.0f;
    info.scale = scale;
    info.samples = 1;
    info.maxEDR = 1;
    

    _context->SetDisplayInfo(info);


    ImGui_ImplSDL2_NewFrame(g_Window);


    _context->BeginScene();
    vizController->Render(0, 1);
    _context->EndScene();
    _context->Present();


    SDL_GL_SwapWindow(g_Window);
}

void HttpPut(std::string url, const std::string &json, std::function<void (const std::string *)> complete)
{

}


extern "C" int main(int argc, const char *argv[])
{
    for (int i=1; i < argc; i++)
        printf("arg[%d] = %s", i, argv[i]);
    
    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    printf("Compiled SDL version %d.%d.%d\n",
         compiled.major, compiled.minor, compiled.patch);
    printf("Linked SDL version %d.%d.%d\n",
         linked.major, linked.minor, linked.patch);

    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }
    
    // For the browser using Emscripten, we are going to use WebGL1 with GL ES2. See the Makefile. for requirement details.
    // It is very likely the generated file won't work in many browsers. Firefox is the only sure bet, but I have successfully
    // run this code on Chrome for Android for example.
//    const char* glsl_version = "#version 100";
    //const char* glsl_version = "#version 300 es";
//    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
//    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    
    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
//    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    g_Window = SDL_CreateWindow("MilkDrop2-Portable-Emscripten", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 1024, window_flags);
    g_GLContext = SDL_GL_CreateContext(g_Window);
    if (!g_GLContext)
    {
        fprintf(stderr, "Failed to initialize WebGL context!\n");
        return 1;
    }
//   SDL_GL_SetSwapInterval(1); // Enable vsync
    
    
    
    _context = GLCreateContext(LoadTextureFromFile);
    
    std::string resourceDir = "";
    std::string pluginDir =  resourceDir + "/assets";
    std::string userDir= "/user";
    
    vizController = CreateVizController(_context, pluginDir, userDir);
    
    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(g_Window, g_GLContext);

    
    // This function call won't return, and will engage in an infinite loop, processing events from the browser, and dispatching them.
    emscripten_set_main_loop_arg(main_loop, NULL, 0, true);
}
