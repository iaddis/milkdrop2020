
#pragma once

#include "render/context.h"
#include "audio/IAudioSource.h"
#include "vis_milk2/IAudioAnalyzer.h"
#include "script/script.h"
#include "script/mdp-eel2/script-codegenerator.h"
#include <future>



enum class ContentMode
{
    ScaleToFill,    // aspect ratio may change
    ScaleAspectFit, // aspect ratio will not change, no content is cropped, may have black areas around contetn
    ScaleAspectFill // aspect ratio will not change, content may be cropped
};


class IVisualizer
{
public:
    virtual ~IVisualizer() {}
    virtual void Draw(ContentMode mode, float alpha = 1.0f) =0;
    virtual void SetOutputSize(Size2D size) = 0;
    virtual render::TexturePtr GetOutputTexture() =0;
    virtual render::TexturePtr GetScreenshotTexture() =0;


    virtual void Render(float dt) =0;
    virtual void CheckResize(Size2D size) =0;
    virtual void DrawDebugUI() = 0;
    virtual void DumpState() = 0;
	virtual int HandleRegularKey(char key) = 0;
    virtual void SetRandomSeed(uint32_t seed) = 0;
    
    
    virtual bool        IsLoadingPreset() = 0;
    virtual bool        LoadPreset(std::string path, float fBlendTime, float duration)  = 0;
    virtual void        LoadPresetAsync(std::string path, float fBlendTime, float duration)  = 0;
    
    virtual void        LoadEmptyPreset() = 0;
    virtual void        LoadPresetKernelsFromFile(std::string path, Script::mdpx::KernelCodeGenerator &cg) = 0;


    virtual const std::string &GetPresetName() = 0;
    virtual float GetPresetProgress() = 0;
    virtual bool TestPreset(std::string path, std::string &error) = 0;
    
    virtual void ShowPresetEditor() = 0;
    virtual void ShowPresetDebugger() = 0;

};

using IVisualizerPtr = std::shared_ptr<IVisualizer>;


IVisualizerPtr CreateVisualizer(render::ContextPtr context, IAudioAnalyzerPtr audio, std::string pluginDir);
