
#include "VizController.h"
#include "StopWatch.h"
#include "vis_milk2/IAudioAnalyzer.h"
#include "vis_milk2/state.h"
#include <algorithm>
#include "ArchiveReader.h"
#include "imgui_support.h"
#include "ImageWriter.h"
#include "../external/imgui/imgui_internal.h"
#include "json.h"

#include <chrono>
#include <cstdio>

#if !defined(EMSCRIPTEN)
#include "../external/cpp-httplib/httplib.h"
#endif

using namespace render;

IAudioSourcePtr OpenNullAudioSource();
IAudioSourcePtr OpenALAudioSource();
IAudioSourcePtr OpenSLESAudioSource();
IAudioSourcePtr OpenUDPAudioSource();
IAudioSourcePtr OpenAVAudioEngineSource();


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
            
            
            std::string str = writer.ToCString();
            
            res.set_content(str, "application/json");

        });
    });

    
    svr.Get("/api/control", [this](const Request &req, Response &res) {
        m_dispatcher.Invoke([this, &req, &res]() {
            
            std::string action = req.get_param_value("action");
            if (action == "next") {
                NavigateNext();
            } else if (action == "prev") {
                NavigatePrevious();
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




class TextureSet : public ITextureSet
{
protected:
    std::mutex _mutex;
    TexturePtr _selectedTexture;
    std::map<std::string, render::TexturePtr> _map;
    

public:
    virtual render::TexturePtr LookupTexture(const std::string &name) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _map.find(name);
        if (it == _map.end())
            return nullptr;
        return it->second;
    }
    
    virtual void AddTexture(const std::string &name, render::TexturePtr texture) override
    {
        if (!texture) return;
        
        std::lock_guard<std::mutex> lock(_mutex);
        _map[name] = texture;
        
        LogPrint("Loaded texture '%s' (%dx%d)\n", name.c_str(), texture->GetWidth(), texture->GetHeight());
    }
    
    virtual void GetTextureListWithPrefix(const std::string &prefix, std::vector<TexturePtr> &list) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        
        list.clear();
        list.reserve(_map.size());
        for (const auto &it : _map)
        {
            if (prefix.empty() || it.first.find(prefix) == 0)
            {
                list.push_back(it.second);
            }
        }
    }


    virtual void ShowUI() override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        
        ImGui::BeginChild("left pane", ImVec2(400, 0), true);
        for (const auto &it : _map)
        {
            if (ImGui::Selectable( it.first.c_str(), it.second == _selectedTexture)) {
                _selectedTexture = it.second;
            }
        }
        ImGui::EndChild();
    
        ImGui::SameLine();
    
        ImGui::BeginChild("right pane", ImVec2(0, 0));
        if (_selectedTexture)
        {
            ImVec2 sz( _selectedTexture->GetWidth(), _selectedTexture->GetHeight());
            ImGui::Image( _selectedTexture.get(), sz);
        }
        ImGui::EndChild();
    }

};

VizController::VizController(ContextPtr context, std::string assetDir, std::string userDir)
	: m_context(context), m_assetDir(assetDir), m_userDir(userDir)
{
    
#if !defined(EMSCRIPTEN) && 0
    std::thread thread( [this]() { this->TestServer(); });
    
    thread.detach();
#endif
    
    // seed with random number
    std::random_device device;
    m_random_generator.seed( device() );
    
    {
        std::string subdir = PathCombine(m_userDir, "runs/");
  
        subdir += PlatformGetPlatformName();
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
    
    m_settingsPath = PathCombine(m_userDir, "settings.json");
    
    m_audio_null = OpenNullAudioSource();
    m_audio_wavfile = m_audio_null;
    m_audio_microphone = m_audio_null;


    m_audio = CreateAudioAnalyzer();
    
    m_texture_map = std::make_shared<TextureSet>();
    
    m_vizualizer = CreateVisualizer(context, m_audio, m_texture_map, assetDir);
    m_vizualizer->LoadEmptyPreset();
    
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



void VizController::OnTestingComplete()
{
    TestingDump();

    m_testing =  false;
    LogPrint("Testing complete\n");
    
    m_fTimeBetweenPresets = 8.0f;        // <- this is in addition to m_fBlendTimeAuto
}
//
//bool VizController::SelectPlaylistPreset(int index, bool blend)
//{
//    if (index >= (int)m_playlist.size())
//    {
//        std::uniform_int_distribution<int> dist(0, (int)(m_presetList.size() - 1) ) ;
//        for (int i=0; i < 64; i++)
//        {
//            int preset_index = dist(m_random_generator);
//            auto preset = m_presetList[preset_index];
//            if (!preset->Blacklist)
//            {
//                index = (int)m_playlist.size();
//                m_playlist.push_back(preset);
//                break;
//            }
//        }
//    }
//
//    // select next preset from playlist
//    if (index >= 0 && index < (int)m_playlist.size() )
//    {
//        PresetInfoPtr info = m_playlist[index];
//        m_playlistPos = index;
//        LoadPreset(info, blend, false);
//        return true;
//    }
//    return false;
//}
//

//
//void VizController::SelectNextPreset(bool blend)
//{
//
//
//    if (!SelectPlaylistPreset( m_playlistPos + 1, blend))
//    {
//        // ran out of playlist entries
//        if (IsTesting())
//         {
//             m_testing = false;
//             OnTestingComplete();
//         }
//        else
//        {
//        }
//
//    }
//}

bool VizController::SelectPreset(const std::string &name)
{
    auto it = m_presetLookup.find(name);
    if (it == m_presetLookup.end())
        return false;
    
    bool blend = false;
    
    LoadPreset(it->second, blend, true, true);
    return true;
}


void VizController::CaptureTestResult(PresetTestResult &profile)
{
    profile.FrameTimes = m_frameTimes;
    profile.FastFrameCount = 0;
    profile.SlowFrameCount = 0;
    
    float threshold = m_deltaTime * 1.10f * 1000.0f;
    
    float total = 0;
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::min();
    
    for (auto time : m_frameTimes) {
        if (time > threshold) profile.SlowFrameCount++;
        else profile.FastFrameCount++;
        total += time;
        min = std::min(min, time );
        max = std::max(max, time );
    }
    float average = total / (float)m_frameTimes.size();
    profile.MinFrameTime = min;
    profile.MaxFrameTime = max;
    profile.AveFrameTime = average;
    profile.SlowPercentage = 100.0f * (float)profile.SlowFrameCount / (float)profile.FrameTimes.size();
}

bool VizController::LoadPreset(PresetInfoPtr presetInfo, PresetLoadArgs args)
{
    
    std::string errors;
    PresetPtr preset = m_vizualizer->LoadPresetFromFile(presetInfo->PresetText, presetInfo->Path, presetInfo->Name, errors);
    if (!preset)
    {
        LogError("Could not load preset '%s'", presetInfo->Path.c_str());
        LogError("%s", errors.c_str());
        return false;
    }

    SetPreset(presetInfo, preset, args);
    return true;
}


void VizController::LoadPresetAsync(PresetInfoPtr presetInfo, PresetLoadArgs args)
{
#if PLATFORM_SUPPORTS_THREADS
    if (m_LoadPresetFuture.valid()) {
        m_LoadPresetFuture.wait();
    }
    
    m_LoadPresetFuture = std::async( std::launch::async,
              [this, presetInfo, args]() {

                std::string errors;
                PresetPtr preset = m_vizualizer->LoadPresetFromFile(presetInfo->PresetText, presetInfo->Path, presetInfo->Name, errors);
                if (preset)
                {
                    m_dispatcher.InvokeAsync([=]() {
                        SetPreset(presetInfo, preset, args);
                    });
                }
              }
          );
#else
    LoadPreset(presetInfo, args);
#endif

}

bool VizController::LoadPreset(PresetInfoPtr presetInfo, bool blend, bool addToHistory, bool async)
{
    if (!presetInfo) {
        return false;
    }
    
    // determine load arguments
    float timeDelta = 0.40f;
    float timeMin =     m_fTimeBetweenPresets * (1.0f - timeDelta);
    float timeMax =     m_fTimeBetweenPresets * (1.0f + timeDelta);

    std::uniform_real_distribution<float> dist(timeMin, timeMax);

    PresetLoadArgs args;
    args.addToHistory = addToHistory;
    args.blendTime = blend ? m_fBlendTimeAuto : 0.0f;
    args.duration = args.blendTime + dist(m_random_generator);
    if (m_testing)
    {
        args.blendTime = 0.0f;
        args.duration = 4 / 60.0f;
    }
    
    if (async) {
        LoadPresetAsync(presetInfo, args);
        return true;
    } else {
        return LoadPreset(presetInfo, args);
    }
    
//
//

    

}


void VizController::SetPreset(PresetInfoPtr presetInfo, PresetPtr preset, PresetLoadArgs args)
{
    StopWatch sw;
    if (m_currentPreset && !m_frameTimes.empty())
    {
        if (IsTesting() && m_captureScreenshots)
            CaptureScreenshot();
        
        if (IsTesting() && m_captureProfiles)
        {
            DirectoryCreate(m_testRunDir);
            std::string path = PathCombine(m_testRunDir, presetInfo->Name);
            path += ".trace.json";
            
            TProfiler::CaptureToFile(path);
            TProfiler::Clear();
        }

        // store information for testing
        PresetTestResult result;
        CaptureTestResult(result);
        m_currentPreset->TestResult = std::make_shared<PresetTestResult>(result);
    }
    m_frameTimes.clear();

    m_vizualizer->SetPreset(preset, args);
    m_currentPreset = presetInfo;

    if (args.addToHistory)
    {
        // add to preset history...
        m_presetHistoryPos = (int)m_presetHistory.size();
        
        HistoryEntry entry {
            .index = m_presetHistoryPos,
            .time = m_time,
            .preset = presetInfo,
            .args = args
        };
        m_presetHistory.push_back(entry);
    }
    
    
    LogPrint("LoadPreset: (%d/%d) '%s' (load: %fms blend:%fs duration:%.1fs)\n",
             presetInfo->Index,
             (int)m_presetList.size(),
             presetInfo->Name.c_str(),
             sw.GetElapsedMilliSeconds(),
             args.blendTime, args.duration );
    
    
    SaveSettings();
}



void VizController::SelectEmptyPreset()
{
    m_vizualizer->LoadEmptyPreset();
    m_vizualizer->LoadEmptyPreset();
}
//
//void VizController::TestAllPresets(std::function<void (std::string name, std::string error)> callback)
//{
//    int errorCount = 0;
//    int totalCount = 0;
//    for (auto preset : m_presetList)
//    {
//        //printf("preset[%03d] %s\n", (int)index, name.c_str());
//        std::string error;
//        if (!m_vizualizer->TestPreset(preset->Path, error)) {
//            if (callback)
//            {
//                callback(preset->Path, error);
//                errorCount++;
//            }
//        }
//        
//        totalCount++;
//    }
//    
//    LogPrint("TestAllPresets (%d/%d)\n", errorCount, totalCount);
//    
//}

PresetGroupPtr VizController::AddPresetGroup(std::string name)
{
    // group presets based on directory name
    PresetGroupPtr pgroup;
    auto it = m_presetGroupMap.find(name);
    if (it == m_presetGroupMap.end())
    {
        // add new preset group
        pgroup = std::make_shared<PresetGroup>();
        pgroup->Name = name;
        m_presetGroupMap[name] = pgroup;
        m_presetGroups.push_back(pgroup);

        if (!name.empty())
        {
            // resolve preset group
            auto parent = AddPresetGroup( PathGetDirectory(name) );
            pgroup->Parent = parent;
            parent->ChildGroups.push_back(pgroup);
        }
    }
    else
    {
        pgroup = it->second;
    }
    return pgroup;
}

void VizController::AddPreset(std::string path, std::string name, std::string presetText)
{
    auto pi = std::make_shared<PresetInfo>(path, name );
    
    // set preset text...
    pi->PresetText = presetText;
    
    m_presetList.push_back(pi);
    
    std::string dirname = PathGetDirectory(pi->Name);
    auto pgroup = AddPresetGroup(dirname);
    
    pgroup->Presets.push_back(pi);
}


void VizController::AddPresetsFromArchive(std::string archivePath)
{
    using namespace Archive;
    
    StopWatch sw;
    
    std::string archiveName = PathGetFileName(archivePath);
    
    int count = 0;
    ArchiveReaderPtr ar = OpenArchive(archivePath);
    if (!ar) {
        return;
    }
    
    ArchiveFileInfo fi;
    while (ar->NextFile(fi))
    {
        const std::string &name = fi.name;
    
        if (name.empty()) continue;
        if (fi.is_directory) continue;
        
        // directory?
        if (name.back() == '/')
        {
            continue;
        }
        
        // hidden file?
        if (name[0] == '.' || name.find("/.") != std::string::npos)
        {
            continue;
        }
        
        std::string ext = PathGetExtensionLower(name);
        if (ext == ".milk")
        {
            std::string text;
            if (ar->ExtractText(text))
            {
                std::string path = archiveName + "/" + name;
                AddPreset(path, PathRemoveExtension(name), text);
                count++;
            }
            continue;
        }
        
        if (name.find("textures/") != std::string::npos)
        {
            // load a texture..
            std::vector<uint8_t> data;
            if (ar->ExtractBinary(data))
            {
                std::string texname = PathRemoveExtension(PathGetFileName(name));
                auto texture = m_context->CreateTextureFromFileInMemory(texname.c_str(),
                                                         data.data(),
                                                         data.size()
                                                         );
                
                if (texture)
                {
                    if (!m_texture_map->LookupTexture(texname))
                    {
                        m_texture_map->AddTexture(texname, texture);
                    }
                }
            }

            continue;
        }

//        printf("%s\n", filename);
        
    };
    
     

    LogPrint("Added presets from archive '%s' (presets:%d time:%fs)\n",
             archiveName.c_str(),
             count,
             sw.GetElapsedSeconds());
     
}


void VizController::AddPresetsFromDir(const char *presetdir, bool recurse)
{
	std::vector<std::string> files;
	std::string dir = PathCombine(m_assetDir, presetdir);
	DirectoryGetFiles(dir, files, true);

    
    
	for (auto file : files)
	{
        std::string ext = PathGetExtensionLower(file);

        if (ext == ".zip")
        {
            AddPresetsFromArchive(file);
            continue;
        }

        if (ext == ".7z")
        {
            AddPresetsFromArchive(file);
            continue;
        }
        
		if (ext == ".milk")
		{
            std::string path = file;
            std::string name = path.substr(dir.size());
            name = PathRemoveLeadingSlash(name);
            name = PathRemoveExtension(name);
            
            AddPreset(path, name, "");
            continue;

		}
	}

    std::sort(m_presetGroups.begin(), m_presetGroups.end(),
              [](const PresetGroupPtr &a, const PresetGroupPtr &b) -> bool{ return a->Name < b->Name; }
          );

    // sort them by name
    std::sort(m_presetList.begin(), m_presetList.end(),
              [](const PresetInfoPtr &a, const PresetInfoPtr &b) -> bool{ return a->SortKey < b->SortKey; }
          );
    
    for (auto group : m_presetGroups)
    {
        std::sort( group->Presets.begin(), group->Presets.end(),
                  [](const PresetInfoPtr &a, const PresetInfoPtr &b) -> bool{ return a->SortKey < b->SortKey; }
              );

    }
    
    // set indices
    for (size_t i=0; i < m_presetList.size(); i++)
    {
        auto preset =        m_presetList[i];
        preset->Index = (int)(i+1);
       // preset->NameWithIndex = StringFormat("[%d] %s", preset->Index, preset->Name.c_str());
        m_presetLookup[preset->Name] = preset;
    }
    
//    SelectPreset(0);
}

void VizController::TestingStart()
{
//    m_testing = true;
//    m_captureScreenshots = false;
//    m_captureProfiles = true;
////    m_selectionLocked = false;
//
//    m_playlist.clear();
////    m_playlistPos = 0;
//
//    if (m_playlist.empty())
//    {
//        for (auto preset : m_presetList)
//        {
//            if (!preset->TestResult)
//            {
//                m_playlist.push_back(preset);
//            }
//        }
//    }
//
//    m_fTimeBetweenPresets = 1.0f;
//
//
//    LogPrint("Testing started\n");
//    SelectNextPreset(false);
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
    writer.String(PlatformGetPlatformName());
    writer.Key("config");
    writer.String(PlatformGetBuildConfig());
    writer.Key("version");
    writer.String(PlatformGetAppVersion());
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
    name += PlatformGetPlatformName();
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
        m_audio_wavfile = source;
    }
    else if (ext == ".wav")
    {
        IAudioSourcePtr source = OpenWavFileAudioSource(fullPath.c_str());
        m_audio_wavfile = source;
    }
    
    m_audioSource = m_audio_wavfile;
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
    NavigateRandom();
}


void VizController::NavigatePrevious()
{
    if (m_presetHistory.empty())
        return;
    
    m_paused = false;
    SetSelectionLock(true);
    
    int index = m_presetHistoryPos - 1;
    if (index < 0 || index >= m_presetHistory.size())
        return;
    
    auto preset = m_presetHistory[index].preset;
    LoadPreset(preset, false, false, false);
    
    m_presetHistoryPos = index;
}

void VizController::NavigateNext()
{
    int index = m_presetHistoryPos + 1;
    if (index >= m_presetHistory.size())
    {
        NavigateRandom();
        return;
    }
 
    
    m_paused = false;
    SetSelectionLock(true);
    auto preset = m_presetHistory[index].preset;
    LoadPreset(preset, false, false, false);
    
    m_presetHistoryPos = index;
}


void VizController::NavigateRandom()
{
    m_paused = false;

    SetSelectionLock(false);

    if (m_presetList.empty())
        return;
    
    std::uniform_int_distribution<int> dist(0, (int)(m_presetList.size() - 1) ) ;
    for (int i=0; i < 64; i++)
    {
        int preset_index = dist(m_random_generator);
        auto preset = m_presetList[preset_index];
        if (!preset->Blacklist)
        {
            LoadPreset(preset, true, true, true);
            break;
        }
    }
}


extern bool UseQuadLines;


static bool ToolbarButton(const char *id, const char *tooltip)
{
    float size = 48;
//    ImVec2 bsize(64, 64);
    ImVec2 bsize(size,size);

//    const ImVec2 label_size = ImGui::CalcTextSize(id, NULL, true);
    
    ImGui::SameLine();
    
//    ImGui::BeginChild(id, bsize, true, ImGuiWindowFlags_NoScrollbar);
    
    auto fy = ImGui::GetStyle().FramePadding;
    auto bta = ImGui::GetStyle().ButtonTextAlign;
//    auto fr = ImGui::GetStyle().FrameRounding;
    ImGui::GetStyle().FramePadding = ImVec2(10, 10);
    ImGui::GetStyle().ButtonTextAlign = ImVec2(0.5, 1.0);

//    ImGui::GetStyle().ButtonTextAlign.x = 0.5;
//    ImGui::GetStyle().ButtonTextAlign.y = 0.90;

    
    
//    ImGui::GetStyle().FrameRounding = 0.5;
//    ImGui::GetStyle().FramePadding.y = 0;
    bool result = ImGui::ButtonEx(id, bsize, ImGuiButtonFlags_None) ;//ImGuiButtonFlags_AlignTextBaseLine);
    
//    ImGui::GetStyle().FrameRounding = fr;
    ImGui::GetStyle().FramePadding = fy;
    ImGui::GetStyle().ButtonTextAlign = bta;
    
//    bool result = ImGui::SmallButton(id);
    if (ImGui::IsItemHovered())
         ImGui::SetTooltip("%s", tooltip);
    
//    ImGui::EndChild();
    return result;
}

//static bool ToolbarToggleButton(const char *id, bool toggle, const char *tooltip)
//{
//    if (toggle)
//          ImGui::PushStyleColor(ImGuiCol_Button,
//                                       ImGui::GetColorU32(ImGuiCol_ButtonActive)
//                                       );
//
//    bool result = ToolbarButton(id, tooltip);
//
//    if (toggle)
//        ImGui::PopStyleColor(1);
//
//    return result;
//
//}

static void ToolbarSeparator(float width = 64)
{
    ImVec2 bsize(width,64);
    ImGui::SameLine();
    ImGui::InvisibleButton("seperator2", ImVec2(bsize.x / 2,  -1.0f));

}

void VizController::DrawTitleWindow()
{
    #if 1
        {
            
            ImVec2 pos(20, 10);
            ImVec2 margin(20, 20);
            ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize (ImVec2(ImGui::GetIO().DisplaySize.x - pos.x - margin.x, 0) );
//            ImGui::SetNextWindowContentSize (ImVec2(ImGui::GetIO().DisplaySize.x - 80, 0) );
            if (ImGui::Begin("MilkdropStatus", &m_showUI, 0
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
  //                  auto &presetName = m_vizualizer->GetPresetName();
//                    ImGui::TextWrapped("%s\n", presetName.c_str());

                    if (m_currentPreset)
                    {
                        ImGui::TextWrapped("(%d/%d) %s\n",
                                           m_currentPreset->Index,
                                           (int)m_presetList.size(),
                                           m_currentPreset->Name.c_str());

                    }
                }
                
            }
            ImGui::End();
            
        }
    #endif
  

}


void VizController::DrawControlsWindow()
{
    {

        
        ImVec2 pos(20, 50);
        ImVec2 margin(20, 20);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize (ImVec2(ImGui::GetIO().DisplaySize.x - pos.x - margin.x, 0) );

        
        //        ImGui::SetNextWindowContentWidth(  ImGui::GetIO().DisplaySize.x - 80);

//        ImGui::SetNextWindowPos(ImVec2(20, ImGui::GetIO().DisplaySize.y - 60), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
//        ImGui::SetNextWindowContentWidth(800);
        if (ImGui::Begin(
                         //m_currentPreset->Name.c_str(),
                         "MilkdropControls",
                         &m_showUI, 0
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
            
            ImGui::SetWindowFontScale(1.0f);
            
            
            if (ToolbarButton(ICON_FA_COG,"Toggle settings"))
            {
                ToggleSettingsMenu();
            }
            
            ToolbarSeparator();
            
            if (ToolbarButton(m_selectionLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK,"Lock preset switching"))
            {
                m_selectionLocked = !m_selectionLocked;
                m_paused = false;
            }
            

            
            if (ToolbarButton(ICON_FA_ARROW_LEFT,"Select previous preset"))
            {
                NavigatePrevious();
                
            }

            
            if (ToolbarButton(ICON_FA_ARROW_RIGHT,"Select next preset"))
                
            {
                NavigateNext();
            }
            
            if (ToolbarButton(ICON_FA_RANDOM,"Select random preset"))
                
            {
                NavigateRandom();
            }
            
            
            if (ToolbarButton(!m_paused ? ICON_FA_PAUSE : ICON_FA_PLAY,"Pause visualizer"))
            {
                TogglePause();
            }

            ToolbarSeparator();

            if (ToolbarButton(ICON_FA_THUMBS_DOWN,"Blacklist preset"))
            {
                BlacklistCurrentPreset();
            }

            if (ToolbarButton(ICON_FA_THUMBS_UP,"Add preset to favorites"))
            {
//                BlacklistCurrentPreset();
            }


            if (ToolbarButton(ICON_FA_BUG, "Flag preset as bugged"))
            {
//                BlacklistCurrentPreset();
            }


            ToolbarSeparator();

//            if (ToolbarButton(ICON_FA_THUMBS_UP,""))
//            {
////                TogglePause();
//            }
//

            
//
//
//
//            if (ToolbarButton(ICON_FA_STEP_FORWARD,"Step a single preset frame"))
//            {
//                SingleStep();
//            }

            
            //
            //        ImGui::SameLine();
            //
            //        if (ImGui::Button("Select"))
            //        {
            //            m_showPresetSelector = !m_showPresetSelector;
            //        }
            
            
            
            ToolbarSeparator();
            
//
//            if (ToolbarToggleButton(ICON_FA_SIGN_IN, IsTesting(), "Toggle testing mode"))
//            {
//                if (!IsTesting())
//                    TestingStart();
//                else
//                    TestingAbort();
//            }
//
//            if (ToolbarButton(ICON_FA_CAMERA,"Capture screenshot"))
//            {
//                CaptureScreenshot();
//            }
//
////            if (ToolbarButton(ICON_FA_CLOCK_O, "Capture performance profile"))
//            if (ToolbarButton(ICON_FA_CLOCK_O, "Profiler"))
//            {
//                m_showProfiler = !m_showProfiler;
////                TProfiler::CaptureNextFrame();
//            }
//
//
//
//
            
            
            {
                ImGui::SameLine();
                bool microphone = m_audioSource == m_audio_microphone;
                if (ToolbarButton(microphone ? ICON_FA_MICROPHONE : ICON_FA_MICROPHONE_SLASH, "Enable microphone"))
                {
                    if (microphone) {
                        m_audioSource = m_audio_wavfile;
                    } else {
                        m_audioSource = m_audio_microphone;
                    }
                }
            }
             
            
            
          
            
//            auto pos = ImGui::GetCursorPos();
//            auto sz = ImGui::GetWindowSize();
//
            
//            ToolbarSeparator(sz.x - pos.x - 64);
            
            
//            pos.x = sz.x - 80;
//            ImGui::SetCursorPos(pos);
//
            if (ToolbarButton(ICON_FA_TIMES,"Close controls"))
            {
                ToggleControlsMenu();
            }

            
            ImGui::SetWindowFontScale(1.0f);


        }
        ImGui::End();
    }
    
}



static void TableNameValue(const char *name, const char *format, ...)
{
    char str[64 * 1024];
    
    va_list arglist;
    va_start(arglist, format);
    int count = vsnprintf(str, sizeof(str), format, arglist);
    va_end(arglist);
    (void)count;
    str[sizeof(str) - 1] = '\0';

    
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(name);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(str);
}



void VizController::DrawPresetSelector()
{
    PresetInfoPtr &selected = m_currentPreset;
    
    bool presetFilterUpdate = false;

    if (m_presetFilter.Draw("Filter")) presetFilterUpdate = true;

    
    if (presetFilterUpdate || m_presetListFiltered.empty())
    {
        if (!m_presetFilter.IsActive())
        {
            m_presetListFiltered = m_presetList;
        }
        else
        {
            m_presetListFiltered.clear();
            for (auto preset : m_presetList)
            {
                if (m_presetFilter.PassFilter(preset->Name.c_str()))
                {
                    m_presetListFiltered.push_back(preset);
                }
            }
            
        }

    }
   
    ImGuiTableFlags flags =
    ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV
    | ImGuiTableFlags_ScrollY
         | ImGuiTableFlags_RowBg

    //        | ImGuiTableFlags_ColumnsWidthFixed
    ; // | ImGuiTableFlags_ContextMenuInBody;
    
//    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (ImGui::BeginTable("##presets", 3, flags))
    {
        
        ImGui::TableSetColumnWidth(0, 40 );
        //                ImGui::TableSetColumnWidth(1, 800 );
        
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Results", ImGuiTableColumnFlags_None);
        ImGui::TableHeadersRow();

        
        ImGuiListClipper clipper((int)m_presetListFiltered.size(),   ImGui::GetTextLineHeightWithSpacing() );
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                auto preset = m_presetListFiltered[i];
                bool is_selected = (preset == selected);
                
                
                ImGui::TableNextColumn();
                ImGui::Text("%d",  preset->Index );
                
                
                ImGui::TableNextColumn();
                if (ImGui::Selectable(preset->Name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    LoadPreset(preset, false, true, true);
                    selected = preset;
                }
                
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
                
                
                ImGui::TableNextColumn();
                
                if (is_selected)
                {
                    float progress = m_vizualizer->GetPresetProgress();
                    
                    ImGui::ProgressBar(progress, ImVec2(128.0f, ImGui::GetTextLineHeight() ));

                    ImGui::SameLine();
//                    ImGui::Text(" *** active *** (%3.1f%%)",
//                                progress * 100.0f
//                                );

                } else
                
                if (preset->TestResult)
                {
                    auto profile = preset->TestResult;
                    
                    ImGui::Text("frametime:%3.2f fast:%d slow:%d (%.2f%%)",
                                profile->AveFrameTime,
                                profile->FastFrameCount,
                                profile->SlowFrameCount,
                                profile->SlowPercentage
                                );
                }
                
            }
        }
        
        ImGui::EndTable();
    }
    
//            ImGui::EndChild();


}


void VizController::DrawSettingsWindow()
{

    const char *appName = PlatformGetAppName();
    
    ImVec2 pos(20, 120);
    ImVec2 margin(20, 20
                  );
    ImGui::SetNextWindowPos(pos, ImGuiCond_Once, ImVec2(0.0f, 0.0f));
//    ImGui::SetNextWindowContentSize (ImVec2(800, 0) );
//    ImGui::SetNextWindowContentSize (ImVec2(ImGui::GetIO().DisplaySize.x - 80, ImGui::GetIO().DisplaySize.y - 180) );

    ImGui::SetNextWindowSize (ImVec2(ImGui::GetIO().DisplaySize.x - pos.x - margin.x, ImGui::GetIO().DisplaySize.y - pos.y - margin.y) );

    if (!ImGui::Begin(appName, &m_showSettings, 0
                     | ImGuiWindowFlags_NoMove
//                     | ImGuiWindowFlags_NoDecoration
                   |  ImGuiWindowFlags_NoTitleBar
                     | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_NoScrollbar
//                     | ImGuiWindowFlags_AlwaysAutoResize
                     | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing
//                     | ImGuiWindowFlags_MenuBar
                     | ImGuiWindowFlags_NoNav
                     ))
    {
        ImGui::End();
        return;
    }
    
    static bool showMenu = false;
    
    // Menu Bar
    if (showMenu && ImGui::BeginMenuBar())
    {

        if (ImGui::BeginMenu("View"))
        {
            
             
            if (ImGui::MenuItem("Preset Editor"))
            {
                m_vizualizer->ShowPresetEditor();
            }
            
            if (ImGui::MenuItem("Preset Debugger"))
            {
                m_vizualizer->ShowPresetDebugger();
            }
            
            ImGui::MenuItem("Profiler", "P", &m_showProfiler);
            ImGui::MenuItem("ImGui Demo Window", NULL, &m_showDemoWindow);
            

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
    
    DrawSettingsTabs();
    
    
    
    
    
    ImGui::End();
   
}



void VizController::DrawPanel_About()
{
    int table_flags =
    ImGuiTableFlags_BordersInnerV|
    ImGuiTableFlags_ColumnsWidthFixed
    ;
    
    if (ImGui::BeginTable("##stats_columns", 2, table_flags))
    {
        //            ImGui::TableNextRow();
        ImGui::TableNextColumn();
        
        //TableNameValue("App", "%s", PlatformGetAppName() );
        TableNameValue("Platform", "%s", PlatformGetPlatformName() );
        TableNameValue("App Version", "%s", PlatformGetAppVersion() );
        TableNameValue("Build Config", "%s",  PlatformGetBuildConfig() );
        TableNameValue("Device Model", "%s", PlatformGetDeviceModel() );
        TableNameValue("Architecture", "%s", PlatformGetDeviceArch() );
        
        TableNameValue("IMGUI Version", "%s", IMGUI_VERSION );
        
        ImGui::Separator();
        
        TableNameValue("Elapsed Time", "%.2fs",  m_time );
        
        TableNameValue("Presets Loaded", "%d", (int)m_presetHistory.size() );
        TableNameValue("Presets Total", "%d", (int)m_presetList.size() );
        PlatformMemoryStats memstats;
        
        if (PlatformGetMemoryStats(memstats)) {
            
            ImGui::Separator();
            
            //            TableSeparator();
            TableNameValue("Virtual Mem", "%.2fMB", (memstats.virtual_size_mb));
            TableNameValue("Resident Mem", "%.2fMB", (memstats.resident_size_mb));
            TableNameValue("Resident Max", "%.2fMB", (memstats.resident_size_max_mb));
        }
        
        ImGui::Separator();

        TableNameValue("Frame Rate", "%3.2f", 1000.0f / m_frameTime );
        TableNameValue("FrameTime", "%3.2fms", m_frameTime );
        TableNameValue("RenderTime", "%3.2fms", m_renderTime );
        
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("");
        ImGui::TableNextColumn();
        
        {
            
            
//                    IMGUI_API void          PlotHistogram(const char* label, const float* values, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0, 0), int stride = sizeof(float));

          ImGui::PlotHistogram("",
                               m_frameTimeHistory.data(),
                               (int)m_frameTimeHistory.size(),
                                0, // values offset
                               "", // overlay text
                               0.0f, 64.0f, // scale_min scale_msx
                               ImVec2(512,80)
                               );
        }
        ImGui::TableNextColumn();

        
        
        ImGui::EndTable();
    }
    
}

void VizController::DrawPanel_Video()
{
    int table_flags =
    ImGuiTableFlags_BordersInnerV|
    ImGuiTableFlags_ColumnsWidthFixed
    ;
    if (ImGui::BeginTable("##video_table", 2, table_flags))
    {
//                ImGui::TableSetColumnWidth(0, 240 );
//                ImGui::TableSetColumnWidth(1, 800 );

//            ImGui::TableNextRow();
//                ImGui::TableNextColumn();
    
        
        TableNameValue("Video Driver", "%s", m_context->GetDriver().c_str() );
        TableNameValue("Video Device", "%s", m_context->GetDesc().c_str() );

        auto internal_texture = m_vizualizer->GetInternalTexture();
        TableNameValue("Internal Texture", "%dx%dx%s", internal_texture->GetWidth(), internal_texture->GetHeight(), PixelFormatToString(internal_texture->GetFormat()) );

        auto texture = m_vizualizer->GetOutputTexture();
        TableNameValue("Output Texture", "%dx%dx%s", texture->GetWidth(), texture->GetHeight(), PixelFormatToString(texture->GetFormat()) );


        ImGui::Separator();

        int screenId = 0;
        for (auto info : m_screens)
        {
            

            ImGui::TableNextRow();
            
            TableNameValue("Screen", "#%d", screenId);
            TableNameValue("Size", "%dx%d", info.size.width,
                           info.size.height
                           );
            TableNameValue("Format", "%s", PixelFormatToString(info.format));
            TableNameValue("RefreshRate", "%.fhz", info.refreshRate);
            TableNameValue("Scale", "%.1f", info.scale);
            TableNameValue("HDR Enabled", "%s", info.hdr ? "true" : "false");
            TableNameValue("MaxEDR", "%.2f", info.maxEDR);
            TableNameValue("ColorSpace", "%s", info.colorSpace.c_str());

            ImGui::Separator();

            screenId++;
        }


        
        ImGui::EndTable();
    }

}

void VizController::DrawPanel_History()
{
    ImGuiTableFlags flags =
    ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV
    | ImGuiTableFlags_RowBg
    | ImGuiTableFlags_ScrollY
    //        | ImGuiTableFlags_ColumnsWidthFixed
    ; // | ImGuiTableFlags_ContextMenuInBody;
    
    //    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (ImGui::BeginTable("##presets", 3, flags))
    {
        
        ImGui::TableSetColumnWidth(0, 40 );
        //                ImGui::TableSetColumnWidth(1, 800 );
        
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Results", ImGuiTableColumnFlags_None);
        ImGui::TableHeadersRow();
        
        const auto &list = m_presetHistory;
        
        
        ImGuiListClipper clipper((int)list.size(),   ImGui::GetTextLineHeightWithSpacing() );
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const auto &entry = list[i];
                bool is_selected = (m_presetHistoryPos == i);
                
                
                ImGui::TableNextColumn();
                ImGui::Text("%d",  entry.preset->Index );
                
                
                ImGui::TableNextColumn();
                if (ImGui::Selectable(entry.preset->Name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    LoadPreset(entry.preset, false, false, true);
                    m_presetHistoryPos = i;
                    
                }
                
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
                
                
                ImGui::TableNextColumn();
                
                if (is_selected)
                {
                    float progress = m_vizualizer->GetPresetProgress();
                    
                    ImGui::ProgressBar(progress, ImVec2(128.0f, ImGui::GetTextLineHeight() ));
                    
                    ImGui::SameLine();
                    //                    ImGui::Text(" *** active *** (%3.1f%%)",
                    //                                progress * 100.0f
                    //                                );
                    
                } else
                    
                    if (entry.preset->TestResult)
                    {
                        auto profile = entry.preset->TestResult;
                        
                        ImGui::Text("frametime:%3.2f fast:%d slow:%d (%.2f%%)",
                                    profile->AveFrameTime,
                                    profile->FastFrameCount,
                                    profile->SlowFrameCount,
                                    profile->SlowPercentage
                                    );
                    }
                
            }
        }
        
        ImGui::EndTable();
    }
    
}

void VizController::DrawPanel_Presets()
{
    
    //            ImGui::BeginChild("left pane", ImVec2(400, 0), true);
    //
    //            static PresetGroupPtr selectedGroup;
    //            for (auto group : m_presetGroups)
    //            {
    //                if (!group->Presets.empty())
    //                {
    //                    bool enabled = true;
    //                    ImGui::Checkbox("", &enabled);
    //                    ImGui::SameLine();
    //                    if (ImGui::Selectable(group->Name.c_str(), group == selectedGroup))
    //                        selectedGroup = group;
    //                }
    //            }
    //            ImGui::EndChild();
    //
    //            ImGui::SameLine();
    
    ImGui::BeginGroup();
    
    /*
     ImGui::BeginChild("right pane", ImVec2(0, 0));
     
     
     if (selectedGroup)
     {
     static PresetInfoPtr selectedPreset;
     for(auto info : selectedGroup->Presets)
     {
     if (ImGui::Selectable(info->ShortName.c_str(), info == selectedPreset ))
     {
     SelectPreset(info, false);
     selectedPreset = info;
     }
     
     }
     }
     ImGui::EndChild();
     */
    
    //            auto selected = m_currentPreset;
    //            show_preset_table(m_presetList, selected);
    //            if (selected != m_currentPreset)
    //            {
    //                SelectPreset(selected, false);
    //            }
    
    DrawPresetSelector();
    
    ImGui::EndGroup();
    

}

void VizController::DrawPanel_Audio()
{
    int table_flags =
    ImGuiTableFlags_BordersInnerV|
    ImGuiTableFlags_ColumnsWidthFixed
    ;
    
    if (ImGui::BeginTable("##audio_table", 2, table_flags))
    {
//                ImGui::TableSetColumnWidth(0, 240 );
//                ImGui::TableSetColumnWidth(1, 800 );
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::Text("Audio Source");
        ImGui::TableNextColumn();
        
        IAudioSourcePtr sources[] = {m_audio_null,m_audio_wavfile, m_audio_microphone, nullptr};

        for (int i=0; sources[i]; i++)
        {
            auto source = sources[i];
            bool selected = (source == m_audioSource);
            if (ImGui::RadioButton( source->GetDescription().c_str(), selected ))
            {
                m_audioSource = source;
            }
        }

        
        ImGui::TableNextColumn();

        ImGui::Separator();

        ImGui::Text("Gain");
        ImGui::TableNextColumn();
        ImGui::SliderFloat("", &m_audioGain, 0.0f, 8.0f);
        ImGui::TableNextColumn();

        
        ImGui::Separator();

        TableNameValue("Description", "%s", m_audioSource->GetDescription().c_str());
        TableNameValue("SampleRate", "%.fhz", m_audio->GetSampleRate());
        TableNameValue("BlockSize", "%d samples\n",  m_audio->GetBlockSize() );

        
        const char *channels[] = {"Left", "Right", nullptr};
        for (int i=0; channels[i]; i++)
        {
            
            ImGui::Separator();
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", channels[i]);
            ImGui::TableNextColumn();
            m_audio->DrawChannelUI(i);
            ImGui::TableNextColumn();
        }
        
        ImGui::Separator();
        
        ImGui::EndTable();
    }
    

}

void VizController::DrawPanel_Log()
{
    LogDrawPanel();
}


void VizController::DrawSettingsTabs()
{


    if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("About"))
        {
            DrawPanel_About();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("History"))
        {
            DrawPanel_History();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Presets"))
        {
            DrawPanel_Presets();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Video"))
        {
            DrawPanel_Video();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Audio"))
        {
            DrawPanel_Audio();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Log"))
        {
            DrawPanel_Log();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Textures"))
        {
            if (m_texture_map)
                m_texture_map->ShowUI();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Profiler"))
        {
            TProfiler::OnGUIPanel();
            ImGui::EndTabItem();
        }
        
        
        

        
        ImGui::EndTabBar();
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

    

    
extern void DebugGUIInputDevice();

void VizController::DrawDebugUI()
{
    
    PROFILE_FUNCTION_CAT("ui");
    
    if (!ImGui::IsAnyItemActive())
    {
        if (ImGui::IsKeyPressed( KEYCODE_L))
        {
            SetSelectionLock(!m_selectionLocked);
        }
        
        
        if (ImGui::IsKeyPressed( KEYCODE_P))
        {
            TogglePause();
        }
        
        if (ImGui::IsKeyPressed( KEYCODE_LEFT, false))
        {
            NavigatePrevious();
        }
        
        if (ImGui::IsKeyPressed( KEYCODE_RIGHT, false))
        {
            NavigateNext();
        }
        
        if (ImGui::IsKeyPressed( KEYCODE_SPACE, false))
        {
            NavigateRandom();
        }

        
        if (ImGui::IsKeyPressed( KEYCODE_ESCAPE))
        {
            ToggleControlsMenu();
        }
    }

    if (ImGui::IsMouseClicked(0))
    {
//        m_showSettings = true;
        if (!m_showUI)
        {
            m_showUI = true;
        }
        else if (!ImGui::GetIO().WantCaptureMouse ) //if (!ImGui::IsAnyItemActive() && !ImGui::IsAnyItemHovered())
        {
            m_showUI = false;
        }
    }
    

    
    if (m_showUI)
    {
        DrawTitleWindow();
        DrawControlsWindow();
        

        if (m_showSettings)
        {
            DrawSettingsWindow();
            if (m_vizualizer)
                m_vizualizer->DrawDebugUI();
        }

        if (m_showProfiler)
            TProfiler::OnGUI(&m_showProfiler);

        
        
        if (m_showDemoWindow)
        {
            ImGui::ShowDemoWindow(&m_showDemoWindow);
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

    if (m_audio_microphone && m_audioSource != m_audio_microphone) {
        m_audio_microphone->StopCapture();
    }
    
    if (!m_audioSource) {
        m_audioSource = m_audio_null;
    }
    
    m_audio->Update(m_audioSource, dt, m_audioGain);
    
    float scale = 1.0f;
    // see if output texture needs resiing
    Size2D size = m_context->GetDisplaySize();
    size.width  *= scale;
    size.height *= scale;
    m_vizualizer->CheckResize(size);
    
    m_vizualizer->Render(dt);
    
    m_time += dt;
    m_frame++;
    

    float progress = m_vizualizer->GetPresetProgress();
    if (m_lastProgress < 1.0f &&  progress >= 1.0f)
    {
        // prset is complete
        /*
        if (IsTesting())
        {
            SelectNextPreset(false);
        }
        else */ if (!m_selectionLocked)
        {
            NavigateNext();
        }
    }
    m_lastProgress = progress;

}


void VizController::CaptureAllScreenshots(bool overwriteExisting)
{

    m_playlist.clear();
//    m_playlistPos = 0;
    
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
//        SelectNextPreset(false);
    }
}

void VizController::RenderToScreenshot(PresetInfoPtr info, Size2D size, int frameCount)
{
    uint32_t seed = 0x12345678;
    float duration = frameCount * m_deltaTime;
    
    PresetLoadArgs args = {0.0f, duration, false};
    
    m_audioSource =  m_audio_wavfile;
    m_audioSource->Reset();
    m_audio->Reset();

    m_vizualizer->SetOutputSize(size);
    m_vizualizer->SetRandomSeed(seed);
    if (!LoadPreset(info, args))
    {
        // error
        LogError("RenderToScreenshotError: '%s'\n", info->Name.c_str());
        return;
    }
    
    LogPrint("LoadPreset: '%s'\n", info->Name.c_str());

    
    for (int i=0; i < frameCount; i++)
    {
        m_audio->Update(m_audioSource, m_deltaTime, m_audioGain);
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


ImageDataPtr VizController::RenderToImageBuffer(PresetInfoPtr info, Size2D size, int frameCount)
{
    uint32_t seed = 0x12345678;
    float duration = frameCount * m_deltaTime;

    
    
    PresetLoadArgs args = {0.0f, duration, false};
    
    m_audioSource =  m_audio_wavfile;
    m_audioSource->Reset();
    m_audio->Reset();

    m_vizualizer->SetOutputSize(size);
    m_vizualizer->SetRandomSeed(seed);
    if (!LoadPreset(info, args))
    {
        LogError("ERROR: Could not load preset: %s\n", info->Path.c_str());
        return nullptr;
    }
    
    for (int i=0; i < frameCount; i++)
    {
//        printf("frame[%d]\n", i);
        m_audio->Update(m_audioSource, m_deltaTime, m_audioGain);
        m_vizualizer->Render(m_deltaTime);
        m_context->Flush();
    }
    
    auto texture = m_vizualizer->GetOutputTexture();
    return nullptr; //m_context->GetImageData(texture);
}

void VizController::CheckDeterminism(PresetInfoPtr preset)
{
    int frameCount = 2;
    
    Size2D size(1024, 1024);
    
//    size = m_context->GetDisplaySize();
    
    ImageDataPtr imageData[2];
    for (int i=0; i < 2; i++)
    {
        imageData[i] = RenderToImageBuffer(preset, size, frameCount);
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
        subdir += PlatformGetPlatformName();
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
        
        
        
    }
}

void VizController::RenderNextScreenshot()
{
//    if (m_playlistPos < m_playlist.size())
//    {
//        PresetInfoPtr info = m_playlist[m_playlistPos];
//        RenderToScreenshot(info, Size2D(1024, 1024), 60);
//        m_playlistPos++;
//    }
//    else
//    {
//        LogPrint("Done with screenshot capture (%d captured)\n", (int)m_playlist.size());
//        m_fTimeBetweenPresets = 15.0f;        // <- this is in addition to m_fBlendTimeAuto
//
//        m_captureScreenshotsFast = false;
//        // we should quit now
//        m_shouldQuit = true;
//    }

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
        
        if (m_frameTimeHistory.empty())
            m_frameTimeHistory.resize(256);
        
        m_frameTimeHistory.push_back(m_frameTime);
        m_frameTimeHistory.erase(m_frameTimeHistory.begin());
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

void  VizController::ToggleSettingsMenu()
{
    m_showSettings = !m_showSettings;
    
}

void  VizController::ToggleControlsMenu()
{
    m_showUI = !m_showUI;
    
}


void VizController::LoadSettings()
{
    
}


void VizController::SaveSettings()
{
#if 0
    JsonStringWriter writer;
    writer.StartObject();
    
    writer.WriteArray("history");
    for (const auto &entry : m_presetHistory)
    {
        writer.Write( entry.preset->Name );

    }
    writer.EndArray();
    
    writer.EndObject();
    
    
    if (FileWriteAllText( m_settingsPath, writer.ToCString() )) {
        LogPrint("Settings saved to '%s'\n", m_settingsPath.c_str());
    }
    
#endif
    
}



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
    //    vizController->OpenInputAudioFile("audio/audio.raw");
    
    
    vizController->OpenInputAudioFile("audio/audio.wav");
   
#if !defined(WIN32) && !TARGET_OS_TV && !EMSCRIPTEN && !defined(__ANDROID__)
    vizController->SetMicrophoneAudioSource(OpenALAudioSource());
#endif

#if defined(__ANDROID__) || defined(OCULUS)
    vizController->SetMicrophoneAudioSource(OpenSLESAudioSource());
//    vizController->SelectAudioSource( vizController->GetAudioSource(2) );
#endif

//#if defined(__APPLE__)
//    vizController->SetMicrophoneAudioSource(OpenAVAudioEngineSource());
////    vizController->SelectAudioSource( vizController->GetAudioSource(3) );
//#endif



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
        vizController->NavigateRandom();
    }
    
    if (Config::GetBool("TESTMODE", false))
    {
      vizController->TestingStart();
    }

    if (PlatformIsDebug())
    {
      vizController->ToggleControlsMenu();
      vizController->ToggleSettingsMenu();
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
    
    vizController->LoadSettings();
    
    return vizController;
}




