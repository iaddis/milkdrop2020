
#include "VizController.h"
#include "StopWatch.h"
#include "vis_milk2/IAudioAnalyzer.h"
#include "vis_milk2/state.h"
#include <algorithm>


#include "imgui_support.h"
#include "script/mdp-eel2/script-codegenerator.h"
#include "ImageWriter.h"
#include "../external/imgui/imgui_internal.h"
#include "json.h"

#include <chrono>
#include <cstdio>

#if !defined(EMSCRIPTEN)
#include "../external/cpp-httplib/httplib.h"
#endif

using namespace render;


#if !defined(EMSCRIPTEN)

std::string dump_headers(const httplib::Headers &headers) {
  std::string s;
  for (auto header : headers) {
      s += header.first;
      s += ": ";
      s += header.second;
      s += '\n';
  }

  return s;
}


void VizController::TestServer()
{
    using namespace httplib;

  Server svr;
  if (!svr.is_valid()) {
    return;
  }
    
    std::string www_root = PathCombine(m_assetDir, "www");
    
    svr.set_base_dir(www_root.c_str());

//    svr.Get("/hi", [](const Request &req, Response &res) {
//      res.set_content("Hello World!\n", "text/plain");
//    });

    svr.Get("/api/headers", [](const Request &req, Response &res) {
       res.set_content(dump_headers(req.headers), "text/plain");
     });

    
    svr.Get("/api/status", [this](const Request &req, Response &res) {
//          res.set_content(dump_headers(req.headers), "text/plain");
        
        m_dispatcher.Invoke([this, &res]() {
            std::string content = m_currentPreset->Path;
            res.set_content(content , "text/plain");
        });
    });
    

    svr.Get("/api/screenshot", [this](const Request &req, Response &res) {
        m_dispatcher.Invoke([this, &res]() {
            std::string path = GetScreenshotPath(m_currentPreset);
            CaptureScreenshot(path);
            std::vector<uint8_t> data;
            if (FileReadAllBytes(path, data)) {
                res.set_content((const char *)data.data(), data.size() , "image/png");
            } else {
                 res.set_content("Error capturing screenshot" , "text/plain");
            }

        });
    });

    svr.Get("/api/tests", [this](const Request &req, Response &res) {
        m_dispatcher.Invoke([this, &res]() {

            std::string str = this->TestingResultsToString();
            
            res.set_content(str, "application/json");

        });
    });

    
    svr.Get("/api/presets", [this](const Request &req, Response &res) {
        m_dispatcher.Invoke([this, &res]() {
            
            JsonStringWriter writer;
            writer.StartObject();
            
            writer.WriteArray("presets");
            for (auto preset : m_presetList)
            {
                writer.Write(preset->Name);
            }
            writer.EndArray();
            
            writer.EndObject();
            
            
            std::string str = writer.ToString();
            
            res.set_content(str, "application/json");

        });
    });

    
    svr.Get("/api/control", [this](const Request &req, Response &res) {
        m_dispatcher.Invoke([this, &req, &res]() {
            
            std::string action = req.get_param_value("action");
            if (action == "next") {
                SelectNextPreset(false);
            } else if (action == "prev") {
                SelectPrevPreset(false);
            } else if (action == "pause") {
                TogglePause();
            }
            res.set_content(m_currentPreset->Name , "text/plain");
        });
        
    });

    
    
        svr.Get("/api/load", [this](const Request &req, Response &res) {
            m_dispatcher.Invoke([this, &req, &res]() {
                
                std::string preset = req.get_param_value("name");
                
                if (SelectPreset(preset)) {
                    res.set_content(m_currentPreset->Name , "text/plain");
                } else {
                    res.status = 404;
                    res.set_content("Could not load preset", "text/plain");
                }
            });
            
        });

    svr.Get("/api/profile", [](const Request &req, Response &res) {
        
        std::string json;
        TProfiler::CaptureToJsonString(json);
        res.set_content(json, "application/json");
     });

    

    svr.listen("localhost", 19999);

}
#endif


VizController::VizController(ContextPtr context, std::string assetDir, std::string userDir)
	: m_context(context), m_assetDir(assetDir), m_userDir(userDir)
{
    
#if !defined(EMSCRIPTEN)
    std::thread thread( [this]() { this->TestServer(); });
    
    thread.detach();
#endif
    
    // seed with random number
    std::random_device device;
    m_random_generator.seed( device() );
    
    {
        std::string subdir = PathCombine(m_userDir, "runs/");
  
        subdir += PlatformGetName();
        subdir += "-";
        subdir += m_context->GetDriver();
        if (PlatformIsDebug()) {
            subdir += "-";
            subdir += "debug";

        }
        
//        subdir += "-";
//        subdir += PlatformGetTimeString();
        
        m_testRunDir  = subdir;
    }


    m_audio = CreateAudioAnalyzer();
    
    {
        auto viz = CreateVisualizer(context, m_audio, assetDir);
        viz->LoadEmptyPreset();
        m_vizualizer = viz;
    }
    
	m_deltaTime = 1.0f / 60.0f;
	m_frame     = 0;
	m_time      = 0.0f;
	m_singleStep = false;

	m_currentPreset = nullptr;
    
    m_selectionLocked = true;
    
    m_showProfiler = false;
    
    
    m_contentMode = ContentMode::ScaleAspectFill;
    
    
//    m_contentMode = ContentMode::ScaleAspectFit;
    
    ImGuiSupport_Init(m_context, m_assetDir);


    LogPrint("Visualizer assetDir: %s\n", assetDir.c_str());
    LogPrint("Visualizer userDir: %s\n", userDir.c_str());
}

VizController::~VizController()
{
#if PLATFORM_SUPPORTS_THREADS
    if (m_screenshotFuture.valid()) {
        m_screenshotFuture.wait();
    }
#endif
    
    ImGuiSupport_Shutdown();

    
}

void VizController::SelectRandomPreset()
{
//    if (m_presetList.empty())
//        return;
//
//    std::uniform_int_distribution<int> dist(0, (int)(m_presetList.size() - 1) ) ;
//    int i = dist(m_random_generator);
//    SelectPreset(i, false);
//
    SelectNextPreset(false);
}



void VizController::OnTestingComplete()
{
    TestingDump();

    m_testing =  false;
    LogPrint("Testing complete\n");
    
    m_fTimeBetweenPresets = 8.0f;        // <- this is in addition to m_fBlendTimeAuto
}

bool VizController::SelectPlaylistPreset(int index, bool blend)
{
    // select next preset from playlist
    if (index >= 0 && index < (int)m_playlist.size() )
    {
        PresetInfoPtr info = m_playlist[index];
        m_playlistPos = index;
        LoadPreset(info, blend, false);
        return true;
    }
    return false;
}


void VizController::AddToPlaylist()
{
    std::uniform_int_distribution<int> dist(0, (int)(m_presetList.size() - 1) ) ;
    for (int i=0; i < 64; i++)
    {
        int index = dist(m_random_generator);
        auto preset = m_presetList[index];
        if (!preset->Blacklist)
        {
            m_playlist.push_back(preset);
        }
    }
}


void VizController::SelectNextPreset(bool blend)
{
    if (!SelectPlaylistPreset( m_playlistPos + 1, blend))
    {
        // ran out of playlist entries
        if (IsTesting())
         {
             m_testing = false;
             OnTestingComplete();
         }
        else
        {
        }

        // add more stuff to playlist!
        if (!m_captureScreenshotsFast)
        AddToPlaylist();

    }
}

void VizController::SelectPrevPreset(bool blend)
{
    SelectPlaylistPreset( m_playlistPos - 1, blend);
}


bool VizController::SelectPreset(const std::string &name)
{
    auto it = m_presetLookup.find(name);
    if (it == m_presetLookup.end())
        return false;
    
    // insert into preset
    SelectPreset(it->second, false);
    return true;
}

void VizController::SelectPreset(PresetInfoPtr preset, bool blend)
{
    // insert into preset
    m_playlist.insert( m_playlist.begin() + m_playlistPos, preset);
    
    SelectPlaylistPreset(m_playlistPos, blend);
}

PresetTestResultPtr VizController::CaptureTestResult()
{
    PresetTestResultPtr profile = std::make_shared<PresetTestResult>();
    profile->FrameTimes = m_frameTimes;
    profile->FastFrameCount = 0;
    profile->SlowFrameCount = 0;
    
    float threshold = m_deltaTime * 1.10f * 1000.0f;
    
    float total = 0;
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::min();
    
    for (auto time : m_frameTimes) {
        if (time > threshold) profile->SlowFrameCount++;
        else profile->FastFrameCount++;
        total += time;
        min = std::min(min, time );
        max = std::max(max, time );
    }
    float average = total / (float)m_frameTimes.size();
    profile->MinFrameTime = min;
    profile->MaxFrameTime = max;
    profile->AveFrameTime = average;
    profile->SlowPercentage = 100.0f * (float)profile->SlowFrameCount / (float)profile->FrameTimes.size();
    return profile;
}


void VizController::LoadPreset(PresetInfoPtr preset, bool blend, bool addToHistory)
{
    if (!preset) {
        return;
    }
    
    if (m_currentPreset && !m_frameTimes.empty())
    {
        if (IsTesting() && m_captureScreenshots)
            CaptureScreenshot();
        
        if (IsTesting() && m_captureProfiles)
        {
            DirectoryCreate(m_testRunDir);
            std::string path = PathCombine(m_testRunDir, preset->Name);
            path += ".trace.json";
            
            TProfiler::CaptureToFile(path);
            TProfiler::Clear();
            
//            std::string serverUrl;
//            if (Config::TryGetString("SERVER_URL", &serverUrl)) {
//              std::string name = PathGetFileName(path);
//              std::string url = PathCombine(PathCombine(serverUrl, "appdata"), name);
//              HttpSendFile("POST", url, path.c_str(), true, "application/json", nullptr);
//            }
        }

        // store information for testing
        m_currentPreset->TestResult = CaptureTestResult();
    }
    m_frameTimes.clear();

    
    float timeDelta = 0.40f;
    float timeMin =     m_fTimeBetweenPresets * (1.0f - timeDelta);
    float timeMax =     m_fTimeBetweenPresets * (1.0f + timeDelta);


    std::uniform_real_distribution<float> dist(timeMin, timeMax);

    float blendTime = blend ? m_fBlendTimeAuto : 0.0f;
    float duration = blendTime + dist(m_random_generator);
    
    if (m_testing)
    {
        blendTime = 0.0f;
        //        duration = 4 / 60.0f;
        duration = 60 / 60.0f;
    }
    
    
    // add to preset history...
    
    m_currentPreset = preset;
    
    
    m_vizualizer->LoadPresetAsync(preset->Path.c_str(), blendTime, duration);


    
    m_presetLoadCount++;
    
    LogPrint("LoadPreset: '%s' (blend:%fs duration:%.1fs)\n", preset->Name.c_str(), blendTime, duration );

}



void VizController::SelectEmptyPreset()
{
    m_vizualizer->LoadEmptyPreset();
    m_vizualizer->LoadEmptyPreset();
}

void VizController::TestAllPresets(std::function<void (std::string name, std::string error)> callback)
{
    int errorCount = 0;
    int totalCount = 0;
    for (auto preset : m_presetList)
    {
        //printf("preset[%03d] %s\n", (int)index, name.c_str());
        std::string error;
        if (!m_vizualizer->TestPreset(preset->Path, error)) {
            if (callback)
            {
                callback(preset->Path, error);
                errorCount++;
            }
        }
        
        totalCount++;
    }
    
    LogPrint("TestAllPresets (%d/%d)\n", errorCount, totalCount);
    
}


void VizController::LoadTexturesFromDir(const char *texturedir, bool recurse)
{
    std::vector<std::string> files;
    std::string dir = PathCombine(m_assetDir, texturedir);
    DirectoryGetFiles(dir, files, true);
    
    for (size_t i = 0; i < files.size(); i++)
    {
        std::string path = files[i];
        
        auto texture = m_context->CreateTextureFromFile(path.c_str(), path.c_str());
        if (texture)
        {
            printf("LoadTexture: %s %dx%d\n", path.c_str(), texture->GetWidth(), texture->GetHeight());
        }
        else
        {
            printf("ERROR: Could not load %s\n", path.c_str());

        }
    }
}


void VizController::AddPresetsFromDir(const char *presetdir, bool recurse)
{
	std::vector<std::string> files;
	std::string dir = PathCombine(m_assetDir, presetdir);
	DirectoryGetFiles(dir, files, true);

	for (size_t i = 0; i < files.size(); i++)
	{
        std::string ext = PathGetExtensionLower(files[i]);
        
		if (ext == ".milk")
		{
            std::string path = files[i];
            std::string name = path.substr(dir.size());
            name = PathRemoveLeadingSlash(name);
            name = PathRemoveExtension(name);

            auto pi = std::make_shared<PresetInfo>(path, name );
			m_presetList.push_back(pi);

		}
	}
    
    // sort them by name
    std::sort(m_presetList.begin(), m_presetList.end(),
              [](const PresetInfoPtr &a, const PresetInfoPtr &b) -> bool{ return a->SortKey < b->SortKey; }
          );
    
    // set indices
    for (size_t i=0; i < m_presetList.size(); i++)
    {
        auto preset =        m_presetList[i];
        preset->Index = (int)i;
       // preset->NameWithIndex = StringFormat("[%d] %s", preset->Index, preset->Name.c_str());
        m_presetLookup[preset->Name] = preset;
    }
    
//    SelectPreset(0);
}

void VizController::TestingStart()
{
    m_testing = true;
    m_captureScreenshots = false;
    m_captureProfiles = true;
//    m_selectionLocked = false;
    
    m_playlist.clear();
    m_playlistPos = 0;

    if (m_playlist.empty())
    {
        for (auto preset : m_presetList)
        {
            if (!preset->TestResult)
            {
                m_playlist.push_back(preset);
            }
        }
    }
    
    m_fTimeBetweenPresets = 1.0f;

    
    LogPrint("Testing started\n");
    SelectNextPreset(false);
}


std::string VizController::TestingResultsToString()
{
    using namespace rapidjson;
    StringBuffer s;
    PrettyWriter<StringBuffer> writer(s);

    writer.StartObject();

    writer.Key("metadata");
    writer.StartObject();
    writer.Key("platform");
    writer.String(PlatformGetName());
    writer.Key("config");
    writer.String(PlatformGetBuildConfig());
    writer.Key("version");
    writer.String(PlatformGetVersion());
    writer.EndObject();

    
    writer.Key("presets");
    writer.StartArray();
    
    for (auto preset : m_presetList)
      {
          auto profile = preset->TestResult;
          if (profile && !profile->FrameTimes.empty() )
          {
//             LogPrint("\"%s\" ave:%3.2f fast:%d slow:%d (%.2f%%)",
//                        preset->Name.c_str(),
//                         profile.AveFrameTime,
//                         profile.FastFrameCount,
//                         profile.SlowFrameCount,
//                         pct
//                         );
              
              
              writer.StartObject();
              writer.Key("name");
              writer.String(preset->Name);
              writer.Key("ave");
              writer.Double(profile->AveFrameTime);
              writer.Key("fast");
              writer.Int(profile->FastFrameCount);
              writer.Key("slow");
              writer.Int(profile->SlowFrameCount);
              writer.Key("pct");
              writer.Double(profile->SlowPercentage);

              writer.EndObject();
              
          }
      }
    
    writer.EndArray();

    writer.EndObject();

    return s.GetString();
}


void VizController::TestingDump()
{
   
    
    std::string json(TestingResultsToString());
//    std::cout << json << '\n';
    
    std::string name;
    name = PlatformGetAppName();
    name += "-";
    name += PlatformGetName();
    if (PlatformIsDebug()) {
        name += "-debug";
    }
    name += ".";
    name += PlatformGetTimeString();
    name += ".test-results.json";
    
    std::string path = PathCombine(m_userDir, name);
    if (FileWriteAllText(path, json)) {
        LogPrint("Wrote test results to: %s\n", path.c_str());
    }
    
    std::string serverUrl;
    if (Config::TryGetString("SERVER_URL", &serverUrl)) {
        std::string url = PathCombine(PathCombine(serverUrl, "appdata"), name);
        HttpSend("PUT", url, json.c_str(), json.size(), true, "application/json", nullptr);
    }

}

void VizController::TestingAbort()
{
    if (!m_testing)
        return;
    
    TestingDump();
  
    
    m_testing =  false;
    m_selectionLocked = true;
    LogPrint("Testing paused\n");
}


bool VizController::OnKeyUp(KeyEvent ke)
{
    ImGuiIO& io = ImGui::GetIO();
    io.KeyCtrl      = ke.KeyCtrl;
    io.KeyShift     = ke.KeyShift;
    io.KeyAlt       = ke.KeyAlt;
    io.KeySuper     = ke.KeyCommand;
    if (ke.code != 0)
    {
       io.KeysDown[ke.code] = false;
    }
    
    return false;
    
}

bool VizController::OnKeyDown(KeyEvent ke)
{
//	 LogPrint("Key: code:%02X\n", ke.code);
    
    ImGuiIO& io = ImGui::GetIO();
    io.KeyCtrl      = ke.KeyCtrl;
    io.KeyShift     = ke.KeyShift;
    io.KeyAlt       = ke.KeyAlt;
    io.KeySuper     = ke.KeyCommand;
    if (ke.code != 0)
    {
       io.KeysDown[ke.code] = true;
    }

    if (ImGui::IsAnyItemActive())
        return false;
    
    bool windowHasFocus =(ImGui::IsAnyWindowFocused());

	switch (ke.code)
	{
    case KEYCODE_BACKSPACE:
        if (!windowHasFocus)
            NavigateHistoryPrevious();
        break;

    case KEYCODE_DELETE:
        BlacklistCurrentPreset();
        break;

//    case KEYCODE_Z:
//        RunUnitTests();
//        break;

    case KEYCODE_L:
        m_selectionLocked = !m_selectionLocked;
        break;
    case KEYCODE_T:
        if (!m_testing)
        {
            TestingStart();
        }
        else
        {
             TestingAbort();
        }
        break;
            
        case KEYCODE_K:
            CaptureAllScreenshots(ke.KeyShift);
            break;
            
//        case KEYCODE_J:
//        {
//            m_screenshotCaptureList.push(m_currentPreset);
//            m_paused = true;
//        }
//            break;
            

                

    case KEYCODE_P:
        TogglePause();
		break;

    case KEYCODE_O:
            
        if (ke.KeyShift)
        {
            TProfiler::CaptureNextFrame();
        }
        else
        {
            m_showProfiler = !m_showProfiler;

        }
        break;

            
    case KEYCODE_LEFT:
//        if (!windowHasFocus)
            NavigateHistoryPrevious();
        break;

    case KEYCODE_RIGHT:
//        if (!windowHasFocus)
            NavigateHistoryNext();
        break;

            
    case KEYCODE_SPACE:
//        if (!windowHasFocus)
            NavigateForward();
        break;

    case KEYCODE_D:
            ToggleControlsMenu();
//        ToggleDebugMenu();
        break;
            
    case KEYCODE_F:
        if (!windowHasFocus)
            SingleStep();
        break;
	default:
        if (!windowHasFocus)
            m_vizualizer->HandleRegularKey(ke.c);
		break;
	}
	return true;
}

void VizController::OnDragDrop(std::string path)
{
    std::string name =  PathRemoveExtension( PathGetFileName(path) );
    SelectPreset(name);
}


void VizController::OpenInputAudioFile(const char *path)
{
    IAudioSourcePtr OpenWavFileAudioSource(const char *path);
    IAudioSourcePtr OpenRawFileAudioSource(const char *path);
    
    std::string fullPath = PathCombine(m_assetDir, path);

    if (!FileExists(fullPath))
    {
        LogError("File does not exist %s\n", fullPath.c_str());
        return;
    }

    std::string ext = PathGetExtensionLower(fullPath);
    if (ext == ".raw")
    {
        IAudioSourcePtr source = OpenRawFileAudioSource(fullPath.c_str());
        AddAudioSource(source);
    }
    else if (ext == ".wav")
    {
        IAudioSourcePtr source = OpenWavFileAudioSource(fullPath.c_str());
        AddAudioSource(source);

    }
}


void VizController::AddAudioSource(IAudioSourcePtr source)
{
    m_audioSourceList.push_back(source);
}


void VizController::SingleStep()
{
    m_singleStep = true;
    m_paused = false;
}

void VizController::TogglePause()
{
    m_paused = !m_paused;
    LogPrint("Playback %s\n", m_paused ? "paused" : "resumed");
}

void VizController::BlacklistCurrentPreset()
{
    if (!m_currentPreset) return;
    
    m_currentPreset->Blacklist = true;
}


void VizController::NavigateHistoryPrevious()
{
    SetSelectionLock(true);
    SelectPrevPreset(false);
}

void VizController::NavigateHistoryNext()
{
    m_paused = false;
    SetSelectionLock(true);
    SelectNextPreset(false);
}


void VizController::NavigateForward()
{
    m_paused = false;

    SetSelectionLock(false);
    SelectNextPreset(false);
}


extern bool UsePrecompiledKernels;
extern bool UseQuadLines;


static bool ToolbarButton(const char *id, const char *tooltip)
{
    ImVec2 bsize(64,64);
    
    ImGui::SameLine();
    
    bool result = ImGui::Button(id, bsize);
    if (ImGui::IsItemHovered())
         ImGui::SetTooltip("%s", tooltip);
    return result;
}

static bool ToolbarToggleButton(const char *id, bool toggle, const char *tooltip)
{
    if (toggle)
          ImGui::PushStyleColor(ImGuiCol_Button,
                                       ImGui::GetColorU32(ImGuiCol_ButtonActive)
                                       );
    
    bool result = ToolbarButton(id, tooltip);
    
    if (toggle)
        ImGui::PopStyleColor(1);
    
    return result;

}

static void ToolbarSeparator()
{
    ImVec2 bsize(64,64);
    ImGui::SameLine();
    ImGui::InvisibleButton("seperator2", ImVec2(bsize.x / 2,  -1.0f));

}

void VizController::DrawTitleWindow()
{
    #if 1
        {
            ImGui::SetNextWindowPos(ImVec2(20, 10), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowContentSize (ImVec2(ImGui::GetIO().DisplaySize.x - 80, 0) );
            if (ImGui::Begin("MilkdropStatus", &m_showControls, 0
                             | ImGuiWindowFlags_NoMove
                             | ImGuiWindowFlags_NoDecoration
                             |  ImGuiWindowFlags_NoTitleBar
                             //                         | ImGuiWindowFlags_NoResize
                             | ImGuiWindowFlags_NoScrollbar
                             | ImGuiWindowFlags_AlwaysAutoResize
                             | ImGuiWindowFlags_NoSavedSettings
                             | ImGuiWindowFlags_NoFocusOnAppearing
                             | ImGuiWindowFlags_NoNav
                             ))
            {
                {
                    auto &presetName = m_vizualizer->GetPresetName();
                    ImGui::TextWrapped("%s\n", presetName.c_str());
                }
                
            }
            ImGui::End();
            
        }
    #endif
    
//    m_audio->DrawStatsUI();

    #if 0
           {
               ImGui::SetNextWindowPos(ImVec2(20, 300), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
               ImGui::SetNextWindowContentWidth(  ImGui::GetIO().DisplaySize.x - 80);
               if (ImGui::Begin("PerfHUD", &m_showControls, 0
                                | ImGuiWindowFlags_NoMove
                                | ImGuiWindowFlags_NoDecoration
                                |  ImGuiWindowFlags_NoTitleBar
                                //                         | ImGuiWindowFlags_NoResize
                                | ImGuiWindowFlags_NoScrollbar
                                | ImGuiWindowFlags_AlwaysAutoResize
                                | ImGuiWindowFlags_NoSavedSettings
                                | ImGuiWindowFlags_NoFocusOnAppearing
                                | ImGuiWindowFlags_NoNav
                                ))
               {
                   {
                       
                       int w = 128;
                       size_t count = m_frameTimes.size();
                       if (count > w) count = w;
                 ImGui::PlotLines("Lines", m_frameTimes.data() + m_frameTimes.size() - count, (int)count,
                                          0, "", 0.0f, 64.0f, ImVec2(0,80));
                   }
                   
               }
               ImGui::End();
               
           }
       #endif
  

}


void VizController::DrawControlsWindow()
{
    {
  
        ImGui::SetNextWindowPos(ImVec2(20, 50), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
//        ImGui::SetNextWindowContentWidth(  ImGui::GetIO().DisplaySize.x - 80);

//        ImGui::SetNextWindowPos(ImVec2(20, ImGui::GetIO().DisplaySize.y - 60), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
//        ImGui::SetNextWindowContentWidth(800);
        if (ImGui::Begin(
                         //m_currentPreset->Name.c_str(),
                         "MilkdropControls",
                         &m_showControls, 0
                         | ImGuiWindowFlags_NoMove
                         | ImGuiWindowFlags_NoDecoration
                         |  ImGuiWindowFlags_NoTitleBar
                         | ImGuiWindowFlags_NoResize
                         | ImGuiWindowFlags_NoScrollbar
//                         | ImGuiWindowFlags_AlwaysAutoResize
                         | ImGuiWindowFlags_NoSavedSettings
  //                       | ImGuiWindowFlags_NoFocusOnAppearing
                         | ImGuiWindowFlags_NoNav
                         ))
        {
            //                    ImGui::PushFont(g_font_awesome);
            
            ImGui::SetWindowFontScale(2.0f);
            

            
            if (ToolbarButton(ICON_FA_BACKWARD,"Go to previous preset"))
            {
                NavigateHistoryPrevious();
                m_paused = false;
                
            }

            
            if (ToolbarButton(ICON_FA_FORWARD,"Go to next preset"))
                
            {
                NavigateHistoryNext();
                m_paused = false;
            }

            
            
            if (ToolbarButton(!m_paused ? ICON_FA_PAUSE : ICON_FA_PLAY,"Pause visualizer"))
            {
                TogglePause();
            }
            
            
            if (ToolbarButton(ICON_FA_STEP_FORWARD,"Step a single preset frame"))
            {
                SingleStep();
            }

            
            //
            //        ImGui::SameLine();
            //
            //        if (ImGui::Button("Select"))
            //        {
            //            m_showPresetSelector = !m_showPresetSelector;
            //        }
            
            
            
            if (ToolbarButton(m_selectionLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK,"Lock or unlock preset switching"))
            {
                m_selectionLocked = !m_selectionLocked;
                m_paused = false;
            }
            
            
            ToolbarSeparator();
            
            
            if (ToolbarToggleButton(ICON_FA_SIGN_IN, IsTesting(), "Toggle testing mode"))
            {
                if (!IsTesting())
                    TestingStart();
                else
                    TestingAbort();
            }
            
            if (ToolbarButton(ICON_FA_CAMERA,"Capture screenshot"))
            {
                CaptureScreenshot();
            }

//            if (ToolbarButton(ICON_FA_CLOCK_O, "Capture performance profile"))
            if (ToolbarButton(ICON_FA_CLOCK_O, "Profiler"))
            {
                m_showProfiler = !m_showProfiler;
//                TProfiler::CaptureNextFrame();
            }


            
            
            
            /*
            if (m_audioSourceList.size() >= 3)
            {
                ImGui::SameLine();
                bool microphone = m_audioSourceIndex == 2;
                if (ImGui::Button(microphone ? ICON_FA_MICROPHONE : ICON_FA_MICROPHONE_SLASH,bsize))
                {
                    if (microphone)
                        SelectAudioSource(1);
                    else
                        SelectAudioSource(2);
                }
            }
             */
            
            
            
            if (ToolbarButton(ICON_FA_COG,"Toggle debug UI"))
            {
                ToggleDebugMenu();
            }
            
            
            ToolbarSeparator();
            
            if (ToolbarButton(ICON_FA_TIMES,"Close controls"))
            {
                ToggleControlsMenu();
            }

            
            ImGui::SetWindowFontScale(1.0f);


        }
        ImGui::End();
    }
    
}



void VizController::DrawStatusWindow()
{
    static bool show_demo_window = false;

    const char *appName = PlatformGetAppName();
    
    ImGui::SetNextWindowPos(ImVec2(20, 140), ImGuiCond_Once, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowContentSize (ImVec2(800, 0) );
    if (ImGui::Begin(appName, &m_showDebug, 0
//                     | ImGuiWindowFlags_NoMove
//                     | ImGuiWindowFlags_NoDecoration
//                   |  ImGuiWindowFlags_NoTitleBar
//                     | ImGuiWindowFlags_NoResize
//                     | ImGuiWindowFlags_NoScrollbar
//                     | ImGuiWindowFlags_AlwaysAutoResize
                     | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing
                     | ImGuiWindowFlags_MenuBar
                     | ImGuiWindowFlags_NoNav
                     ))
    {
        // Menu Bar
        if (ImGui::BeginMenuBar())
        {

            if (ImGui::BeginMenu("View"))
            {
                
                ImGui::MenuItem("Preset Playlist", NULL, &m_showPlayList);
                ImGui::MenuItem("Preset Selector", NULL, &m_showPresetSelector);
                
                if (ImGui::MenuItem("Preset Editor"))
                {
                    m_vizualizer->ShowPresetEditor();
                }
                
                if (ImGui::MenuItem("Preset Debugger"))
                {
                    m_vizualizer->ShowPresetDebugger();
                }
                
                ImGui::MenuItem("Audio Analyzer", NULL, &m_showAudioWindow);
                ImGui::MenuItem("Log Window", NULL, &m_showLogWindow);
                ImGui::MenuItem("Profiler", "P", &m_showProfiler);
                ImGui::MenuItem("ImGui Demo Window", NULL, &show_demo_window);
                

                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Debug"))
            {
                
                if (ImGui::MenuItem("Load Empty Preset"))
                {
                    SelectEmptyPreset();
                }
                
//                if (ImGui::MenuItem("Run Unit Tests"))
//                {
//                    RunUnitTests();
//                }
        
                if (ImGui::MenuItem("Capture Screenshot"))
                {
                    CaptureScreenshot();
                }
        
                bool testing = IsTesting();
                if (ImGui::MenuItem("Test All Presets", NULL, &testing))
                {
                    if (!IsTesting())
                    {
                        TestingStart();
                    } else
                    {
                        TestingAbort();
                    }
                }
                
                if (ImGui::MenuItem("Capture All Screenshots"))
                {
                    CaptureAllScreenshots(false);
                }

                if (ImGui::MenuItem("Force Capture All Screenshots"))
                            {
                                CaptureAllScreenshots(true);
                            }

                

                if (ImGui::MenuItem("Generate Preset CPP Code"))
                {
                    GeneratePresetCPPCode();
                }
                

                

                
                ImGui::MenuItem("Use Prebuilt Kernels", NULL, &UsePrecompiledKernels);

                ImGui::MenuItem("Use Quad Line Renderer", NULL, &UseQuadLines);
//                ImGui::MenuItem("Use Correct Blur", NULL, &UseCorrectBlur);

//                ImGui::MenuItem("Metrics", NULL, &show_app_metrics);
//                ImGui::MenuItem("Style Editor", NULL, &show_app_style_editor);
//                ImGui::MenuItem("About Dear ImGui", NULL, &show_app_about);
                ImGui::EndMenu();
            }

//            if (ImGui::BeginMenu("Help"))
//            {
//                ImGui::MenuItem("Metrics", NULL, &show_app_metrics);
//                ImGui::MenuItem("Style Editor", NULL, &show_app_style_editor);
//                ImGui::MenuItem("About Dear ImGui", NULL, &show_app_about);
//                ImGui::EndMenu();
//            }
            ImGui::EndMenuBar();
        }

        
        
        ImGui::Columns(2, "stats_columns", true);
        ImGui::SetColumnWidth(0, 150);
//        ImGui::SetColumnWidth(1, 350);

        //ImGui::TextColumn("App\t%s\t", PlatformGetAppName() );
        ImGui::TextColumn("Platform\t%s\t", PlatformGetName() );
        ImGui::TextColumn("Version\t%s\t", PlatformGetVersion() );
        ImGui::TextColumn("Config\t%s\t",  PlatformGetBuildConfig() );
        ImGui::TextColumn("Device\t%s\t", PlatformGetDeviceModel() );

//        ImGui::TextColumn("Preset Count\t%d\t", m_presetLoadCount );
//        ImGui::TextColumn("Frame Count\t%d\t", m_frame );
        ImGui::TextColumn("Frame Rate\t%3.2f\t", 1000.0f / m_frameTime );
        ImGui::TextColumn("FrameTime\t%3.2fms\t", m_frameTime );
        ImGui::TextColumn("RenderTime\t%3.2fms\t", m_renderTime );
        
        
        PlatformMemoryStats memstats;
        
        if (PlatformGetMemoryStats(memstats)) {
            ImGui::Separator();
            ImGui::TextColumn("Virtual Mem\t%.2fMB\t", (memstats.virtual_size_mb));
            ImGui::TextColumn("Resident Mem\t%.2fMB\t", (memstats.resident_size_mb));
            ImGui::TextColumn("Resident Max\t%.2fMB\t", (memstats.resident_size_max_mb));
        }

        
        ImGui::Separator();
        
        auto texture = m_vizualizer->GetOutputTexture();
        
        ImGui::TextColumn("Video Driver\t%s\t", m_context->GetDriver().c_str() );
        ImGui::TextColumn("Video Device\t%s\t", m_context->GetDesc().c_str() );
        ImGui::TextColumn("Output Texture\t%dx%dx%s\t", texture->GetWidth(), texture->GetHeight(), PixelFormatToString(texture->GetFormat()) );
        
        int screenId = 0;
        for (auto info : m_screens)
        {
            ImGui::TextColumn("Screen[%d]\t%dx%dx%s rate:%.f scale:%.f edr:%.2f %s\t",
                              screenId,
                              info.size.width,
                              info.size.height,
                              PixelFormatToString(info.format),
                              info.refreshRate,
                              info.scale,
                              info.maxEDR,
                              info.colorSpace.c_str()
                              );

            screenId++;
        }
        ImGui::Separator();
        

#if 0
        ImGui::TextColumn("Load Count\t%d\t", m_presetLoadCount );
        ImGui::TextColumn("Count<Shader>\t%d\t",  (int)InstanceCounter<Shader>::GetInstanceCount() );
        ImGui::TextColumn("Count<Preset>\t%d\t",  (int)InstanceCounter<CState>::GetInstanceCount() );
        ImGui::TextColumn("Count<Expr>\t%d\t", Script::mdpx::GetExpressionCount() );
        ImGui::Separator();
#endif
        
        
        
        
//                ImGui::Separator();



        ImGui::TextColumn("Audio Source:\n");
        
        ImGui::NextColumn();
        for (size_t i=0; i < m_audioSourceList.size(); i++) {

            auto source = m_audioSourceList[i];
            
            bool selected = (source == m_audioSource);
            if (ImGui::RadioButton( source->GetDescription().c_str(), selected ))
            {
                m_audioSource = source;
            }
        }
        

        
        ImGui::Columns(1);

        
//        ImGui::Separator();


        
     
        
    }
    ImGui::End();
    
    if (show_demo_window)
    {
        ImGui::ShowDemoWindow(&show_demo_window);
    }
}


std::string VizController::GetScreenshotPath(PresetInfoPtr preset)
{
    std::string spath =  preset->Name + ".png";  // GetScreenshotPath(preset);
    std::string path = PathCombine(m_testRunDir, spath);
    return path;
}

void VizController::CaptureScreenshot()
{
    if (!m_currentPreset)
        return;
    

    std::string spath =  m_currentPreset->Name + ".png";  // GetScreenshotPath(preset);
    std::string path = PathCombine(m_testRunDir, spath);

    CaptureScreenshot(path);
}



static void SwapRGBA(ImageDataPtr image)
{
    for (int y = 0; y < image->height; y++)
    {
        uint8_t *ptr = image->data + y * image->pitch;
        for (int x = 0; x < image->width; x++)
        {
            // swap BGRA -> RGBA
            std::swap(ptr[0], ptr[2]);
            ptr += 4;
        }
    }
}


void VizController::CaptureScreenshot(std::string path)
{
    PROFILE_FUNCTION()
    
    auto texture = m_vizualizer->GetScreenshotTexture();
    
    
    if (texture->GetFormat() != PixelFormat::RGBA8Unorm &&
        texture->GetFormat() != PixelFormat::BGRA8Unorm)
    {
        LogError("ERROR: Unsupported texture format for screen captures\n");
        return;
        
    }
    
    m_context->GetImageDataAsync(texture, [path] (ImageDataPtr image) {
            if (!image) {
                LogError("ERROR: Could not get image data from texture\n");
                return;
            }
        
#if 1
            DispatchVoid([path, image]() -> void {

                StopWatch sw;
              //  assert(image->format == PixelFormat::RGBA8Unorm);
                
        //        if (image->format == PixelFormat::RGBA32Float)
        //        {
        //            ImageWriteToHDR(path, (float *)image->data, image->width, image->height);
        //            LogPrint("Screenshot saved to: %s (%.2fms)\n", path.c_str(), sw.GetElapsedMilliSeconds());
        //        }
                
                // swap BGRA -> RGBA
                if (image->format == PixelFormat::BGRA8Unorm)
                {
                    SwapRGBA(image);
                    image->format = PixelFormat::RGBA8Unorm;
                }
                

                DirectoryCreate( PathGetDirectory(PathGetDirectory(path)) ) ;
                DirectoryCreate( PathGetDirectory(path) ) ;

                if (ImageWriteToFile(path, (const uint32_t *)image->data, image->width, image->height))
                {
                    LogPrint("Screenshot saved to: %s (%.2fms)\n", path.c_str(), sw.GetElapsedMilliSeconds());
                    
//                    std::string serverUrl;
//                    if (Config::TryGetString("SERVER_URL", &serverUrl)) {
////                        std::string name = PathGetFileName(path);
//                        std::string url = PathCombine(PathCombine(serverUrl, "appdata"), subpath);
//                        HttpSendFile("POST", url, fullPath.c_str(), false, "image/png", nullptr);
//                    }
                } else
                {
                    LogError("ERROR saving screenshot to: %s\n", path.c_str());
                   
                }
            });
#endif
        });

    
}


void VizController::DrawPresetPlaylist()
{
    if (m_showPlayList)
    {

//        ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
        
//        ImGui::SetNextWindowContentSize (ImVec2(600, 800), Imguicond_ );
        ImGui::SetNextWindowSize(ImVec2(520,600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Preset Playlist", &m_showPlayList, 0
                         ))
        {
            std::string buffer;
            buffer.reserve(512);
            
            ImGui::Columns(3, "presets", true);
//            ImGui::SetColumnOffset(0, 0);
//            ImGui::SetColumnOffset(1, 40);
//            ImGui::SetColumnOffset(2, 350);

            ImGuiListClipper clipper((int)m_playlist.size(),   ImGui::GetTextLineHeightWithSpacing() );
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {
                    auto preset = m_playlist[i];
                    bool is_selected = m_playlistPos == i;
                    
                    
                    ImGui::Text("%d.", i + 1);
                    ImGui::NextColumn();


                    if (ImGui::Selectable(preset->Name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        SelectPlaylistPreset(i, false);
                    }
                    
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::NextColumn();


                    if (preset->TestResult)
                    {
                        auto profile = preset->TestResult;

                        ImGui::Text("ave:%3.2f fast:%d slow:%d (%.2f%%)",
                                    profile->AveFrameTime,
                                    profile->FastFrameCount,
                                    profile->SlowFrameCount,
                                    profile->SlowPercentage
                                    );
                    }
                    ImGui::NextColumn();

                    
                }
            }
            
            ImGui::Columns(1);

        }
        ImGui::End();
    }
}
    


void VizController::DrawPresetSelector()
{
    if (m_showPresetSelector)
    {

        ImGui::SetNextWindowSize(ImVec2(520,600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Preset Selector", &m_showPresetSelector))
        {
            std::string buffer;
            buffer.reserve(512);
            
            ImGui::Columns(2, "presets", true);
//            ImGui::Columns(3, "presets", true);
//            ImGui::SetColumnOffset(0, 0);
            ImGui::SetColumnOffset(1, 40);
//            ImGui::SetColumnOffset(2, 350);

            ImGuiListClipper clipper((int)m_presetList.size(),   ImGui::GetTextLineHeightWithSpacing() );
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {
                    auto preset = m_presetList[i];
                    bool is_selected = (preset == m_currentPreset);
                    
                    
                    ImGui::Text("[%d]", i);
                    ImGui::NextColumn();


                    if (ImGui::Selectable(preset->Name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        SelectPreset(preset, false);
                    }
                    
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::NextColumn();


                    /*
                    if (preset->TestResult)
                    {
                        auto profile = preset->TestResult;

                        ImGui::Text("ave:%3.2f fast:%d slow:%d (%.2f%%)",
                                    profile->AveFrameTime,
                                    profile->FastFrameCount,
                                    profile->SlowFrameCount,
                                    profile->SlowPercentage
                                    );
                    } 
                    ImGui::NextColumn();
*/
                    
                }
            }
            
            ImGui::Columns(1);

        }
        ImGui::End();
    }
}
    
void LogDrawWindow(bool *popen);
extern void DebugGUIInputDevice();

void VizController::DrawDebugUI()
{
    
    PROFILE_FUNCTION_CAT("ui");
    
//
//    if (ImGui::IsKeyPressed('p'))
//    {
//        m_paused = !m_paused;
//    }
//
//    if (ImGui::IsKeyPressed('f'))
//    {
//        SingleStep();
//    }
//
//    if (ImGui::IsKeyPressed('d'))
//    {
//        ToggleDebugMenu();
//    }
    
    if (ImGui::IsKeyPressed( KEYCODE_ESCAPE))
    {
        m_showDebug = false;
        m_showControls = false;
    }

    if (ImGui::IsAnyMouseDown())
    {
//        m_showDebug = true;
        m_showControls = true;
    }

    
    if (m_showControls)
    {
        DrawTitleWindow();
        DrawControlsWindow();
        

        if (m_showDebug)
        {
            DrawStatusWindow();
            if (m_vizualizer)
                m_vizualizer->DrawDebugUI();
            DrawPresetSelector();
            DrawPresetPlaylist();
            
            if (m_showLogWindow)
                LogDrawWindow(&m_showLogWindow);
            
            
            if (m_showAudioWindow)
                m_audio->DebugDrawUI(&m_showAudioWindow);
            


        }

        if (m_showProfiler)
            TProfiler::OnGUI(&m_showProfiler);

        // invoke all custom debug menus
        for (auto func : m_customDebugMenus)
        {
            func();
        }
    }
    
    
}


static ShaderPtr LoadShaderFromFile(render::ContextPtr context, std::string rootDir, std::string path)
{
    std::string code;
    if (!FileReadAllText(path, code))
    {
        return nullptr;
    }
    
    auto shader = context->CreateShader("hdr");
    shader->CompileAndLink({
        ShaderSource{ShaderType::Vertex,     path, code, "VS", "vs_1_1", "hlsl"},
        ShaderSource{ShaderType::Fragment,   path, code, "PS", "ps_3_0", "hlsl"}
    });
    return shader;
}





static TexturePtr CreateHalfWhiteTexture(ContextPtr context)
{
    int size = 64;
    uint32_t *data = new uint32_t[size * size];
    int i=0;
    for (int y=0; y < size; y++)
    for (int x=0; x < size; x++)
    {
        data[i] = ( (x ^ y) & 1) ? 0xFFFFFFFF : 0xFF000000;
        i++;
    }
    auto texture = context->CreateTexture("half-white", size, size, PixelFormat::RGBA8Unorm, data);
    delete[] data;
    return texture;
}


void VizController::DrawQuad(TexturePtr texture, float x, float y, float w, float h, Color4F c0, Color4F c1)
{
    if (!m_shader_hdr) {
        std::string rootDir = PathCombine(m_assetDir, "data");;
        std::string shaderPath = PathCombine(rootDir, "hdr.fx");
        m_shader_hdr = LoadShaderFromFile(m_context, rootDir, shaderPath);
    }
    
    
    Vertex v[4];
    memset(v, 0, sizeof(v));
    
    float x0 = x + 0.0f;
    float x1 = x + w;
    float y0 = y + 0;
    float y1 = y + h;
    //    y0 =  -y0;
    //    y1 =  -y1;
    
    v[0].x = x0;
    v[0].y = y0;
    v[1].x = x1;
    v[1].y = y0;
    v[2].x = x0;
    v[2].y = y1;
    v[3].x = x1;
    v[3].y = y1;
    
    v[0].tu = 0;
    v[0].tv = 0;
    v[1].tu = 1;
    v[1].tv = 0;
    v[2].tu = 0;
    v[2].tv = 1;
    v[3].tu = 1;
    v[3].tv = 1;
    
    v[0].Diffuse =
    v[2].Diffuse = c0.ToU32();
    v[1].Diffuse = 
    v[3].Diffuse = c1.ToU32();

    
    auto sampler = m_shader_hdr->GetSampler(0);
    sampler->SetTexture(texture);

    m_context->SetShader(m_shader_hdr);

    m_context->DrawArrays(PRIMTYPE_TRIANGLESTRIP, 4,  v);
}


void VizController::DrawHDRTest()
{
    if (!m_texture_halfwhite) {
        m_texture_halfwhite = CreateHalfWhiteTexture(m_context);
    }
    
    m_context->SetRenderTarget(nullptr);
    
    
    Matrix44 old = m_context->GetTransform();

    Size2D size = m_context->GetRenderTargetSize();
    Matrix44 ortho = MatrixOrthoLH(0, (float)size.width, (float)size.height, 0, 0, 1);
    
    m_context->SetTransform(ortho);
    m_context->SetBlendDisable();
    
    
#if 0
    float x = 64;
    float y = 64;
    float w = 64;
    for (int i=0; i < 32; i++)
    {
        float c  = i * 2.0f / 32.0f;
        DrawQuad(nullptr, x, y, w, w, Color4F(c,c,c,1));

        c *= 2;
        DrawQuad( m_texture_halfwhite, x, y+ w, w, w, Color4F(c,c,c,1));
        x += w;
    }
#else
    float x = 64;
    float y = 64;
    float w = 64;
    DrawQuad(nullptr, x, y, w * 16, w, Color4F(0,0,0,1),  Color4F(1,1,1,1) );
    y+= 80;
    DrawQuad(nullptr, x, y, w * 16 * 2, w, Color4F(0,0,0,1),  Color4F(2,2,2,1) );
    y+= 80;
    DrawQuad(nullptr, x, y, w * 16 * 3, w, Color4F(0,0,0,1),  Color4F(3,3,3,1) );
#endif

    
    m_context->SetTransform(old);
}

void VizController::RenderFrame(float dt)
{
    PROFILE_FUNCTION();

    
    // stop audio sources
    for (size_t i=0; i < m_audioSourceList.size(); i++)
    {
       auto source = m_audioSourceList[i];
       if (source != m_audioSource)
       {
           source->StopCapture();
       }
    }
    
    m_audio->Update(m_audioSource, dt);
    
    // see if output texture needs resiing
    Size2D size = m_context->GetDisplaySize();
    m_vizualizer->CheckResize(size);
    
    m_vizualizer->Render(dt);
    
    m_time += dt;
    m_frame++;
    

    if (m_vizualizer->GetPresetProgress() >= 1.0f)
    {
        if (!m_vizualizer->IsLoadingPreset())
        {
            if (IsTesting())
            {
                SelectNextPreset(false);
            }
            else if (!m_selectionLocked)
            {
                NavigateForward();
            }
        }
    }

}


void VizController::CaptureAllScreenshots(bool overwriteExisting)
{

    m_playlist.clear();
    m_playlistPos = 0;
    
    for (auto preset : m_presetList)
    {
        std::string spath =  preset->Name + ".png";  // GetScreenshotPath(preset);
        std::string fullPath = PathCombine(m_testRunDir, spath);

        
        if (overwriteExisting || !FileExists(fullPath))
        {
            m_playlist.push_back(preset);
        }
        else
        {
            LogPrint("Screenshot already exists %s\n", fullPath.c_str());
        }
    }
    

    if (!m_playlist.empty())
    {
        m_captureScreenshotsFast = true;
        m_fTimeBetweenPresets = 1.0f;
        m_selectionLocked = true;


        LogPrint("CaptureAllScreenshots started (count=%d)\n", (int)m_playlist.size());
        SelectNextPreset(false);
    }
}

void VizController::RenderToScreenshot(PresetInfoPtr info, Size2D size, int frameCount)
{
    uint32_t seed = 0x12345678;
    float duration = frameCount * m_deltaTime;
    
    
    m_audioSource =  GetAudioSource(1);
    m_audioSource->Reset();
    m_audio->Reset();

    m_vizualizer->SetOutputSize(size);
    m_vizualizer->SetRandomSeed(seed);
    if (!m_vizualizer->LoadPreset(info->Path.c_str(), 0.0f, duration))
    {
        // error
        LogError("RenderToScreenshotError: '%s'\n", info->Name.c_str());
        return;
    }
    
    LogPrint("LoadPreset: '%s'\n", info->Name.c_str());

    
    for (int i=0; i < frameCount; i++)
    {
        m_audio->Update(m_audioSource, m_deltaTime);
        m_vizualizer->Render(m_deltaTime);
        m_context->Flush();
    }
    
    
    DirectoryCreate(m_testRunDir);
    
    std::string screenshotPath = PathCombine(m_testRunDir, info->Name) + ".png";
    CaptureScreenshot(screenshotPath);
    
//    std::string path = PathCombine(m_testRunDir, info->Name) +  ".trace.json";
//    TProfiler::CaptureToFile(path);
//    TProfiler::Clear();
}


ImageDataPtr VizController::RenderToImageBuffer(std::string presetPath, Size2D size, int frameCount)
{
    uint32_t seed = 0x12345678;
    float duration = frameCount * m_deltaTime;

    
    m_audioSource =  GetAudioSource(1);
    m_audioSource->Reset();
    m_audio->Reset();

    m_vizualizer->SetOutputSize(size);
    m_vizualizer->SetRandomSeed(seed);
    if (!m_vizualizer->LoadPreset(presetPath.c_str(), 0.0f, duration))
    {
        LogError("ERROR: Could not load preset: %s\n", presetPath.c_str());
        return nullptr;
    }
    
    for (int i=0; i < frameCount; i++)
    {
//        printf("frame[%d]\n", i);
        m_audio->Update(m_audioSource, m_deltaTime);
        m_vizualizer->Render(m_deltaTime);
        m_context->Flush();
    }
    
    auto texture = m_vizualizer->GetOutputTexture();
    return nullptr; //m_context->GetImageData(texture);
}

void VizController::CheckDeterminism(PresetInfoPtr preset)
{
    UsePrecompiledKernels = false;
    int frameCount = 2;
    
    Size2D size(1024, 1024);
    
//    size = m_context->GetDisplaySize();
    
    ImageDataPtr imageData[2];
    for (int i=0; i < 2; i++)
    {
        imageData[i] = RenderToImageBuffer(preset->Path, size, frameCount);
//         m_vizualizer->DumpState();
    }
    
    if (memcmp(imageData[0]->data, imageData[1]->data, imageData[0]->size) == 0)
    {
        // LogPrint("ImageData equal\n");
        return;
    }
    
    {
        LogPrint("Determinism error: %s\n", preset->Name.c_str());
        
        std::string subdir = "";
        subdir += PlatformGetName();
        subdir += "-";
        subdir += "errors";
        subdir += "-";
        subdir += m_context->GetDriver();
        
        std::string screenshotDir = PathCombine(m_userDir, subdir);
        DirectoryCreate(screenshotDir);
        
        for (int i=0; i < 2; i++)
        {
            std::string path = PathCombine(screenshotDir, preset->Name);
            path += '-';
            path += std::to_string(i);
            path += ".png";
            ImageWriteToFile(path, (const uint32_t *)imageData[i]->data, imageData[i]->width, imageData[i]->height);
        }
        
        
//        for (int i=0; i < 2; i++)
//        {
//            RenderToImageBuffer(preset->Path, size, 60, imageData[i]);
//            m_vizualizer->DumpState();
//        }
        
    }
}

void VizController::RenderNextScreenshot()
{
    if (m_playlistPos < m_playlist.size())
    {
        PresetInfoPtr info = m_playlist[m_playlistPos];
        RenderToScreenshot(info, Size2D(1024, 1024), 60);
        m_playlistPos++;
    }
    else
    {
        LogPrint("Done with screenshot capture (%d captured)\n", (int)m_playlist.size());
        m_fTimeBetweenPresets = 15.0f;        // <- this is in addition to m_fBlendTimeAuto

        m_captureScreenshotsFast = false;
        // we should quit now
        m_shouldQuit = true;
    }

}


void VizController::Render(int screenId, int screenCount, float dt)
{
    PROFILE_FUNCTION();
    
    m_dispatcher.Process();
    
    m_deltaTime = dt;
    
    m_screens.resize(screenCount);
    m_screens[screenId] = m_context->GetDisplayInfo();

    if (screenId == (screenCount - 1))
    {
        // measure time between frames
        m_frameTime = m_frameTimer.GetElapsedMilliSeconds();
        m_frameTimer.Restart();
        
        m_frameTimes.push_back(m_frameTime);
    }
    

    

    // render visualizer to last screen
    if (screenId == (screenCount - 1))
    {
        
        StopWatch sw;
         
         if (m_captureScreenshotsFast)
         {
             RenderNextScreenshot();
         }
        else
        if (!m_paused || m_singleStep)
        {
            RenderFrame(m_deltaTime);
    
            if (m_singleStep)
            {
                m_singleStep = false;
                m_paused     = true;
            }
        }
        else
        {

            TProfiler::SkipFrame();
        }

        
        // display on screen
        m_context->SetRenderTarget(nullptr, "DrawVisualizer", LoadAction::Clear);
        m_context->SetBlendDisable();
        m_vizualizer->Draw(m_contentMode, 1.0f);
        

        m_renderTime = sw.GetElapsedMilliSeconds();
        
    }

    
    
    if (screenId == 0)
    {
        // ImGui rendering to first screen
        
        ImGuiSupport_NewFrame();
        DrawDebugUI();
        m_context->SetRenderTarget(nullptr, "ImGui", (screenCount > 1) ? LoadAction::Clear : LoadAction::Load);
        ImGuiSupport_Render();
    }
    

//    DrawHDRTest();

    

    

}

void  VizController::ToggleDebugMenu()
{
    m_showDebug = !m_showDebug;
    
}

void  VizController::ToggleControlsMenu()
{
    m_showControls = !m_showControls;
    
}



IAudioSourcePtr OpenNullAudioSource();
IAudioSourcePtr OpenALAudioSource();
IAudioSourcePtr OpenSLESAudioSource();
IAudioSourcePtr OpenUDPAudioSource();
IAudioSourcePtr OpenAVAudioEngineSource();


VizControllerPtr CreateVizController(ContextPtr context, std::string assetDir, std::string userDir)
{
    PROFILE_FUNCTION()

    std::string traceDir = PathCombine(userDir, "trace_data");
    DirectoryCreate(traceDir);
    
    TProfiler::AddCaptureLocation(userDir);
    
    std::string serverUrl;
    if (Config::TryGetString("SERVER_URL", &serverUrl)) {
        std::string url = PathCombine(serverUrl, "trace_data");
        TProfiler::AddCaptureLocation(url);
    }

    auto vizController = std::make_shared<VizController>(context, assetDir, userDir);
    vizController->AddPresetsFromDir("presets", true);
    //    vizController->OpenInputAudioFile("audio.raw");
    
    
    vizController->AddAudioSource(OpenNullAudioSource());
    vizController->OpenInputAudioFile("audio.wav");
    vizController->SelectAudioSource( vizController->GetAudioSource(1) );

#if !defined(WIN32) && !TARGET_OS_TV && !EMSCRIPTEN && !defined(__ANDROID__)
    vizController->AddAudioSource(OpenALAudioSource());
#endif

#if defined(__ANDROID__) || defined(OCULUS)
    vizController->AddAudioSource(OpenSLESAudioSource());
//    vizController->SelectAudioSource( vizController->GetAudioSource(2) );
#endif

#if defined(__APPLE__)
    vizController->AddAudioSource(OpenAVAudioEngineSource());
//    vizController->SelectAudioSource( vizController->GetAudioSource(3) );
#endif



//
//    vizController->SelectAudioSource(0);


    //vizController->SelectPreset(0);
//    vizController->SelectEmptyPreset();
    //vizController->SetSelectionMode(VizController::SelectionMode::);

    
    
    std::string firstPreset;
//
//    firstPreset = "Flexi - age of shading chaos";
//    firstPreset = "Flexi - dawn has broken";
//
    //firstPreset = "Flexi - kaleidoscope template [commented composite shader]";
    
    Config::TryGetString("PRESET", &firstPreset);

    if (!firstPreset.empty())
    {
        vizController->SelectPreset(firstPreset);
        vizController->SetSelectionLock(true);
    }
    else
    {
        vizController->SetSelectionLock(false);
        vizController->SelectRandomPreset();
    }
    
    if (Config::GetBool("TESTMODE", false))
    {
      vizController->TestingStart();
    }

    if (PlatformIsDebug())
    {
      vizController->ToggleControlsMenu();
      vizController->ToggleDebugMenu();
    }

    if (Config::GetBool("CAPTURE_ALL", false))
    {
        vizController->CaptureAllScreenshots(true);
    }

    
#if  defined(OCULUS)
    vizController->SetSelectionLock(false);
    vizController->ToggleControlsMenu();
    vizController->ToggleDebugMenu();
  //  vizController->TestingStart();

#else
//    if (PlatformIsDebug()) {
//        vizController->SetSelectionLock(true);
//        vizController->ToggleControlsMenu();
//        vizController->ToggleDebugMenu();
//
//    } else {
//        vizController->SelectRandomPreset();
//        vizController->SetSelectionLock(false);
//
//    }
#endif
    
//    vizController->SelectNextPreset();

//vizController->TestingStart();
    
//    vizController->SetSelectionMode(VizController::SelectionMode::Sequential);
    
    
    
    
    // vizController->SetAudioSource(OpenNullAudioSource());
   // vizController->SetAudioSource(OpenALAudioSource());
    //  vizController->SetAudioSource(OpenUDPAudioSource());
    
    return vizController;
}




void VizController::GeneratePresetCPPCode()
{
    std::string dir = PathCombine(PathGetDirectory(__FILE__), "../generated/");
    std::string outputPath = PathCombine(dir, "generated-code.cpp");

    
    Script::mdpx::KernelCodeGenerator cg;
    
    for (auto pi : m_presetList)
    {
        m_vizualizer->LoadPresetKernelsFromFile(pi->Path, cg);
    }
    
    cg.GenerateCode(outputPath);
    LogPrint("Generated preset cpp code: %s\n", outputPath.c_str());
}
                                       

