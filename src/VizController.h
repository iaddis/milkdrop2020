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

class PresetGroup;
using PresetGroupPtr = std::shared_ptr<PresetGroup>;


class PresetInfo
{
public:
    PresetInfo(std::string path, std::string name)
    : Path(path), Name(name), ShortName(PathGetFileName(name))
    {
        
        SortKey = Name;
        for (int i=0; i < SortKey.size(); i++)
        {
            SortKey[i] = tolower(SortKey[i]);
        }
    }
    
    const std::string     ShortName;
    const std::string     Name;
    const std::string     Path;
    std::string           SortKey;
   // std::string             NameWithIndex;
    
    std::string           PresetText;       // text content of preset (if preloaded as text)

    PresetTestResultPtr  TestResult;        // test result if testing has been completed
    
    bool            Blacklist = false; 
    int             Index = 0;
    bool            Loaded = false;
    int             DisplayCount = 0;
    std::string     LoadErrors;
};

using PresetInfoPtr = std::shared_ptr<PresetInfo>;

class PresetGroup;
using PresetGroupPtr = std::shared_ptr<PresetGroup>;

class PresetGroup
{
public:
    std::string Name;

    std::weak_ptr<PresetGroup> Parent;
    
    std::vector< PresetGroupPtr > ChildGroups;

    std::vector< PresetInfoPtr > Presets;
};





class VizController
{
public:
    
	VizController(render::ContextPtr context, std::string assetDir, std::string userDir);
	virtual ~VizController();

    virtual bool ShouldQuit() { return m_shouldQuit; }

	virtual void OpenInputAudioFile(const char *path);
    virtual void SetMicrophoneAudioSource(IAudioSourcePtr source)
    {
        m_audio_microphone = source;
    }
    
	virtual void Render(int screenId, int screenCount, float dt = (1.0f / 60.0f) );
    virtual void RenderFrame(float dt);


	virtual void AddPresetsFromDir(const char *dir, bool recurse);
    virtual void AddPresetsFromArchive(std::string archivePath);
    virtual void AddPreset(std::string path, std::string name, std::string presetText);

    virtual PresetGroupPtr AddPresetGroup(std::string name);

    virtual void SelectEmptyPreset();
    virtual bool SelectPreset(const std::string &name);
    
    virtual void SetPreset(PresetInfoPtr presetInfo, PresetPtr preset, PresetLoadArgs args);
    
    virtual bool LoadPreset(PresetInfoPtr presetInfo, PresetLoadArgs args);
    virtual bool LoadPreset(PresetInfoPtr presetInfo, bool blend, bool addToHistory, bool async);
    virtual void LoadPresetAsync(PresetInfoPtr presetInfo, PresetLoadArgs args);

    void AddToPlaylist();
    
    virtual void RenderToScreenshot(PresetInfoPtr preset,  Size2D size, int frameCount);
    virtual render::ImageDataPtr RenderToImageBuffer(PresetInfoPtr preset, Size2D size, int frameCount);

    virtual void CheckDeterminism(PresetInfoPtr preset);


    virtual void NavigatePrevious();
    virtual void NavigateNext();
    virtual void NavigateRandom();
    
    virtual void SingleStep();
    virtual void TogglePause();
    
    virtual void ToggleSettingsMenu();
    virtual void ToggleControlsMenu();

    virtual bool OnKeyDown(KeyEvent key);
    virtual bool OnKeyUp(KeyEvent key);
    virtual void OnDragDrop(std::string path);

    void LoadTexturesFromDir(const char *texturedir, bool recurse);
    void TestAllPresets(std::function<void (std::string name, std::string error)> callback);
    
    
    virtual void LoadSettings();
    virtual void SaveSettings();
    
    void SetSelectionLock(bool lock) { m_selectionLocked = lock;}
    
    
    virtual void DrawDebugUI();

    void OnTestingComplete();
    
    void DrawTitleWindow();
    void DrawControlsWindow();
    void DrawSettingsWindow();
    void DrawStatusTable();
    void DrawSettingsTabs();
    void DrawPresetSelector();
    void DrawPresetPlaylist();

    
    void DrawPanel_About();
    void DrawPanel_Video();
    void DrawPanel_History();
    void DrawPanel_Presets();
    void DrawPanel_Audio();
    void DrawPanel_Log();

    void BlacklistCurrentPreset();


    void RenderNextScreenshot();
    void CaptureAllScreenshots(bool overwriteExisting);
    
    virtual void CaptureTestResult(PresetTestResult &result);
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
    std::string                 m_settingsPath;
    std::string                 m_testRunDir;
	PresetInfoPtr               m_currentPreset;
	std::vector<PresetInfoPtr>	m_presetList;
    std::vector<PresetInfoPtr>  m_playlist;
    std::set<PresetInfoPtr>     m_presetBlacklist;
//    int                         m_playlistPos = 0;

    
    
    struct HistoryEntry
    {
        int                 index;
        double              time;
        PresetInfoPtr       preset;
        PresetLoadArgs      args;
    };
    
    std::vector<HistoryEntry>  m_presetHistory;
    int                        m_presetHistoryPos = 0;

    std::map<std::string, PresetInfoPtr>    m_presetLookup;
    
    
    std::vector<PresetGroupPtr> m_presetGroups;
    std::map<std::string, PresetGroupPtr> m_presetGroupMap;
    
    ImGuiTextFilter     m_presetFilter;
    std::vector<PresetInfoPtr> m_presetListFiltered;

    bool                        m_shouldQuit = false;
    
    bool                        m_selectionLocked = false;
    ContentMode                 m_contentMode = ContentMode::ScaleAspectFill;
    IAudioAnalyzerPtr           m_audio;
    ITextureSetPtr              m_texture_map;
	IVisualizerPtr				m_vizualizer;

    IAudioSourcePtr             m_audio_null;
    IAudioSourcePtr             m_audio_wavfile;
    IAudioSourcePtr             m_audio_microphone;
    IAudioSourcePtr             m_audioSource;
    float                       m_audioGain = 1.0f;
    
	int                         m_frame = 0;
	float                       m_deltaTime = 0.0f;
    float                       m_renderTime = 0.0f;
    float                       m_frameTime = 0.0f;
	float                       m_time = 0.0f;
    float                       m_lastProgress = 0.0f;
	bool						m_singleStep = false;
	bool						m_paused = false;
    bool                        m_showUI  = false;
    bool                        m_showSettings  = false;
    bool                        m_showAudioWindow = false;
    bool                        m_showProfiler = false;
    bool                        m_showDemoWindow = false;

    StopWatch                   m_frameTimer;
    
    bool    m_testing = false;
    bool    m_captureScreenshots = false;
    bool    m_captureProfiles = false;
    bool    m_captureScreenshotsFast = false;

    std::future<bool>   m_screenshotFuture;
    
    std::future<void> m_LoadPresetFuture;
    
    float        m_fBlendTimeAuto= 2.7f;        // blend time when preset auto-switches
    float        m_fTimeBetweenPresets = 15.0f;        // <- this is in addition to m_fBlendTimeAuto

    std::mt19937     m_random_generator;
    
    render::ShaderPtr m_shader_hdr;
    render::TexturePtr m_texture_halfwhite;
    
    std::vector<float> m_frameTimes;
    std::vector<float> m_frameTimeHistory;
};

using VizControllerPtr = std::shared_ptr<VizController>;


VizControllerPtr CreateVizController(render::ContextPtr context, std::string assetDir, std::string userDir);
