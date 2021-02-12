#pragma once

#include "IVisualizer.h"
#include "vis_milk2/IAudioAnalyzer.h"
#include "script/script.h"
#include "KeyEvent.h"

#include <map>
#include <set>
#include <string>
#include <queue>
#include <functional>
#include <thread>
#include <future>         // std::async, std::future


struct PresetTestResult
{
    std::vector<float>  FrameTimes;
    float MinFrameTime;
    float MaxFrameTime;
    float AveFrameTime;
    float SlowPercentage;
    int     SlowFrameCount;
    int     FastFrameCount;
};

using PresetTestResultPtr = std::shared_ptr<PresetTestResult>;

class PresetInfo
{
public:
    PresetInfo(std::string path, std::string name)
    : Path(path), Name(name)
    {
        SortKey = Name;
        for (int i=0; i < SortKey.size(); i++)
        {
            SortKey[i] = tolower(SortKey[i]);
        }
    }
    
    const std::string     Name;
    const std::string     Path;
    std::string           SortKey;
   // std::string             NameWithIndex;
    

    PresetTestResultPtr  TestResult;        // test result if testing has been completed
    
    bool            Blacklist = false; 
    int             Index = 0;
    bool            Loaded = false;
    int             DisplayCount = 0;
    std::string     LoadErrors;
};

using PresetInfoPtr = std::shared_ptr<PresetInfo>;

class VizController
{
public:
    
	VizController(render::ContextPtr context, std::string assetDir, std::string userDir);
	virtual ~VizController();

    virtual bool ShouldQuit() { return m_shouldQuit; }

	virtual void OpenInputAudioFile(const char *path);
    virtual void AddAudioSource(IAudioSourcePtr source);
    virtual IAudioSourcePtr GetAudioSource(int index)
    { return m_audioSourceList[index];}
    virtual void SelectAudioSource(IAudioSourcePtr source) { m_audioSource = source; }
	virtual void Render(int screenId, int screenCount, float dt = (1.0f / 60.0f) );
    virtual void RenderFrame(float dt);


	virtual void AddPresetsFromDir(const char *dir, bool recurse);
//    virtual int GetPresetIndex(PresetInfoPtr preset);
    virtual void SelectEmptyPreset();
//	virtual void SelectPreset(int index, bool blend);
    virtual void SelectPreset(PresetInfoPtr preset, bool blend);
    virtual bool SelectPreset(const std::string &name);
    virtual void LoadPreset(PresetInfoPtr preset, bool blend, bool addToHistory);
	virtual void SelectNextPreset(bool blend);
	virtual void SelectPrevPreset(bool blend);
    virtual bool SelectPlaylistPreset(int index, bool blend);
    virtual void SelectRandomPreset();
    
    void AddToPlaylist();
    
    virtual void RenderToScreenshot(PresetInfoPtr preset,  Size2D size, int frameCount);
    virtual render::ImageDataPtr RenderToImageBuffer(std::string presetPath, Size2D size, int frameCount);

    virtual void CheckDeterminism(PresetInfoPtr preset);


    virtual void NavigateHistoryPrevious();
    virtual void NavigateHistoryNext();
    virtual void NavigateForward();
    
    virtual void SingleStep();
    virtual void TogglePause();
    
    virtual void ToggleDebugMenu();
    virtual void ToggleControlsMenu();

    virtual bool OnKeyDown(KeyEvent key);
    virtual bool OnKeyUp(KeyEvent key);
    virtual void OnDragDrop(std::string path);

    void LoadTexturesFromDir(const char *texturedir, bool recurse);
    void TestAllPresets(std::function<void (std::string name, std::string error)> callback);
    
    
    void SetSelectionLock(bool lock) { m_selectionLocked = lock;}
    
    
    virtual void DrawDebugUI();
    virtual void AddCustomDebugMenu( std::function< void ()> func ) { m_customDebugMenus.push_back(func);}

    void OnTestingComplete();
    
    void GeneratePresetCPPCode();
    void DrawTitleWindow();
    void DrawControlsWindow();
    void DrawStatusWindow();
    void DrawPresetSelector();
    void DrawPresetPlaylist();
    
    void BlacklistCurrentPreset();


    void RenderNextScreenshot();
    void CaptureAllScreenshots(bool overwriteExisting);
    
    PresetTestResultPtr CaptureTestResult();
    virtual void CaptureScreenshot();
    
    virtual void CaptureScreenshot(std::string path);
    
    virtual std::string GetScreenshotPath(PresetInfoPtr preset);
    
    bool IsTesting() {return m_testing;}
    void TestingStart();
    void TestingAbort();
    void TestingDump();
    std::string TestingResultsToString();
    
    void DrawQuad(render::TexturePtr texture, float x, float y, float w, float h, Color4F c0, Color4F c1);
    void DrawHDRTest();
    

    void TestServer();
public:
    DispatchQueue               m_dispatcher;
    
	render::ContextPtr					m_context;

    std::vector<render::DisplayInfo> m_screens;

    render::TexturePtr          m_screenshotTexture;
	std::string                 m_assetDir;
    std::string                 m_userDir;
    std::string                 m_testRunDir;
	PresetInfoPtr               m_currentPreset;
	std::vector<PresetInfoPtr>	m_presetList;
    std::vector<PresetInfoPtr>  m_playlist;
    std::set<PresetInfoPtr>     m_presetBlacklist;
    int                         m_playlistPos = 0;
    

    std::map<std::string, PresetInfoPtr>    m_presetLookup;
    
    
    bool                        m_shouldQuit = false;
    int                         m_presetLoadCount = 0;
    
    bool                        m_selectionLocked = false;
    ContentMode                 m_contentMode = ContentMode::ScaleAspectFill;
    IAudioAnalyzerPtr           m_audio;
	IVisualizerPtr				m_vizualizer;

    std::vector<IAudioSourcePtr> m_audioSourceList;
    IAudioSourcePtr              m_audioSource;
    
	int                         m_frame = 0;
	float                       m_deltaTime = 0.0f;
    float                       m_renderTime = 0.0f;
    float                       m_frameTime = 0.0f;
	float                       m_time = 0.0f;
	bool						m_singleStep = false;
	bool						m_paused = false;
    bool                        m_showControls  = false;
    bool                        m_showDebug  = false;
    bool                        m_showPresetSelector = false;
    bool                        m_showPlayList = false;
    bool                        m_showLogWindow = false;
    bool                        m_showAudioWindow = false;
    bool                        m_showProfiler = false;

    std::vector< std::function< void ()> > m_customDebugMenus;

    StopWatch                   m_frameTimer;
    
    bool    m_testing = false;
    bool    m_captureScreenshots = false;
    bool    m_captureProfiles = false;
    bool    m_captureScreenshotsFast = false;

    std::future<bool>   m_screenshotFuture;
    
    float        m_fBlendTimeAuto= 2.7f;        // blend time when preset auto-switches
    float        m_fTimeBetweenPresets = 15.0f;        // <- this is in addition to m_fBlendTimeAuto


    std::mt19937     m_random_generator;
    
    render::ShaderPtr m_shader_hdr;
    render::TexturePtr m_texture_halfwhite;
    
    std::vector<float> m_frameTimes;
};

using VizControllerPtr = std::shared_ptr<VizController>;


VizControllerPtr CreateVizController(render::ContextPtr context, std::string assetDir, std::string userDir);
