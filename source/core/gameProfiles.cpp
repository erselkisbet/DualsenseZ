#include "core/gameProfiles.hpp"
#include "core/coreRuntime.hpp"
#include "core/triggerConfiguration.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <platform_folders.h>
#ifdef WINDOWS
#include <Windows.h>
#include <TlHelp32.h>
#pragma comment(lib,"version.lib")
#endif

namespace {
namespace fs=std::filesystem;
#ifdef WINDOWS
std::wstring Wide(const std::string& text) {
    if(text.empty())return {};
    std::wstring out(MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0),L'\0');
    MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),out.data(),int(out.size()));
    return out;
}
std::string Utf8(const std::wstring& text) {
    if(text.empty())return {};
    std::string out(WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr),'\0');
    WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),out.data(),int(out.size()),nullptr,nullptr);
    return out;
}
fs::path Path(const std::string& utf8) {return fs::path(Wide(utf8));}
std::string Text(const fs::path& path) {return Utf8(path.wstring());}
std::wstring SteamRoot() {
    wchar_t buffer[1024]{};DWORD size=sizeof(buffer);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath",RRF_RT_REG_SZ,nullptr,buffer,&size)==ERROR_SUCCESS)return buffer;
    size=sizeof(buffer);
    if(RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\WOW6432Node\\Valve\\Steam",L"InstallPath",RRF_RT_REG_SZ,nullptr,buffer,&size)==ERROR_SUCCESS)return buffer;
    return {};
}
#else
fs::path Path(const std::string& utf8) {return fs::path(utf8);}
std::string Text(const fs::path& path) {return path.string();}
#endif
std::string ReadAll(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);std::ostringstream all;all<<input.rdbuf();return all.str();
}
// First value of a "key" "value" pair in Steam's text format, with its backslash escapes undone.
std::string VdfValue(const std::string& text,const char* key) {
    std::smatch found;
    if(!std::regex_search(text,found,std::regex(std::string("\"")+key+"\"\\s+\"((?:[^\"\\\\]|\\\\.)*)\"")))return {};
    std::string value=found[1].str(),out;
    for(size_t i=0;i<value.size();++i){if(value[i]=='\\'&&i+1<value.size())++i;out+=value[i];}
    return out;
}
fs::path DataFolder() {return Path(sago::getDocumentsFolder())/"DualsenseZ";}
fs::path StoreFile() {return DataFolder()/"GameProfiles.json";}
// File-name-safe and ASCII, so it also works with the profile loader's narrow paths.
std::string SafeName(const std::string& name) {
    std::string out;
    for(unsigned char c:name)if(std::isalnum(c)||c==' '||c=='-'||c=='_')out+=char(c);
    while(!out.empty()&&out.back()==' ')out.pop_back();
    while(!out.empty()&&out.front()==' ')out.erase(0,1);
    if(out.size()>60)out.resize(60);
    return out.empty()?"Game":out;
}
}

namespace GameLibrary {
std::vector<InstalledGame> InstalledSteamGames() {
    std::vector<InstalledGame> games;
#ifdef WINDOWS
    try {
        const fs::path steam(SteamRoot());
        if(steam.empty())return games;
        std::vector<fs::path> libraries{steam};
        const auto folders=ReadAll(steam/"steamapps"/"libraryfolders.vdf");
        const std::regex pathPattern("\"path\"\\s+\"((?:[^\"\\\\]|\\\\.)*)\"");
        for(std::sregex_iterator it(folders.begin(),folders.end(),pathPattern),end;it!=end;++it) {
            std::string value=(*it)[1].str(),unescaped;
            for(size_t i=0;i<value.size();++i){if(value[i]=='\\'&&i+1<value.size())++i;unescaped+=value[i];}
            libraries.push_back(Path(unescaped));
        }
        std::vector<int> seen;
        for(const auto& library:libraries) {
            std::error_code error;
            for(const auto& entry:fs::directory_iterator(library/"steamapps",error)) {
                const auto file=entry.path().filename().wstring();
                if(file.rfind(L"appmanifest_",0)!=0||entry.path().extension()!=L".acf")continue;
                const auto manifest=ReadAll(entry.path());
                InstalledGame game;
                game.name=VdfValue(manifest,"name");
                const auto id=VdfValue(manifest,"appid"),folder=VdfValue(manifest,"installdir");
                if(game.name.empty()||folder.empty()||id.empty()||id.size()>9||!std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isdigit(c)!=0;}))continue;
                game.steamAppId=std::stoi(id);
                // Runtimes and redistributables live in the library too; they are not games.
                if(game.steamAppId==228980||game.name.find("Proton")!=std::string::npos||game.name.find("Steam Linux Runtime")!=std::string::npos||game.name.find("Steamworks")!=std::string::npos)continue;
                if(std::find(seen.begin(),seen.end(),game.steamAppId)!=seen.end())continue;
                const auto install=library/"steamapps"/"common"/Path(folder);
                if(!fs::is_directory(install,error))continue;
                game.folder=Text(install)+"\\";
                seen.push_back(game.steamAppId);games.push_back(std::move(game));
            }
        }
        std::sort(games.begin(),games.end(),[](const InstalledGame& a,const InstalledGame& b){return a.name<b.name;});
    } catch(...) {}
#endif
    return games;
}
std::string SteamCover(int appId) {
#ifdef WINDOWS
    try {
        const fs::path steam(SteamRoot());
        if(steam.empty()||appId<=0)return {};
        const auto cache=steam/"appcache"/"librarycache";
        std::error_code error;
        for(const auto& entry:fs::recursive_directory_iterator(cache/std::to_string(appId),error))
            if(entry.path().filename()==L"library_600x900.jpg")return Text(entry.path());
        const auto legacy=cache/(std::to_string(appId)+"_library_600x900.jpg");
        if(fs::exists(legacy,error))return Text(legacy);
    } catch(...) {}
#endif
    return {};
}
std::string ExecutableName(const std::string& path) {
    const auto fallback=Text(Path(path).stem());
#ifdef WINDOWS
    const auto file=Wide(path);DWORD ignored=0;
    const DWORD size=GetFileVersionInfoSizeW(file.c_str(),&ignored);
    if(!size)return fallback;
    std::vector<unsigned char> data(size);
    if(!GetFileVersionInfoW(file.c_str(),0,size,data.data()))return fallback;
    struct Translation {WORD language,codePage;}* translation=nullptr;UINT length=0;
    if(!VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&translation),&length)||length<sizeof(Translation))return fallback;
    for(const wchar_t* field:{L"ProductName",L"FileDescription"}) {
        wchar_t query[80];swprintf_s(query,L"\\StringFileInfo\\%04x%04x\\%s",translation->language,translation->codePage,field);
        wchar_t* value=nullptr;UINT characters=0;
        if(VerQueryValueW(data.data(),query,reinterpret_cast<void**>(&value),&characters)&&characters>1){auto name=Utf8(value);if(!name.empty())return name;}
    }
#endif
    return fallback;
}
std::string CoversFolder() {
    const auto folder=DataFolder()/"Covers";std::error_code error;fs::create_directories(folder,error);return Text(folder);
}
std::vector<std::string> RunningProcessPaths() {
    std::vector<std::string> paths;
#ifdef WINDOWS
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return paths;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    for(BOOL more=Process32FirstW(snapshot,&entry);more;more=Process32NextW(snapshot,&entry)) {
        // Limited query rights are enough for the image path, and also work for elevated games.
        const HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
        if(!process)continue;
        wchar_t path[1024];DWORD size=1024;
        if(QueryFullProcessImageNameW(process,0,path,&size))paths.push_back(Utf8(std::wstring(path,size)));
        CloseHandle(process);
    }
    CloseHandle(snapshot);
#endif
    return paths;
}
bool Matches(const std::string& match,const std::string& processPath) {
    if(match.empty()||processPath.empty())return false;
#ifdef WINDOWS
    auto normal=[](const std::string& text){auto wide=Wide(text);std::replace(wide.begin(),wide.end(),L'/',L'\\');return wide;};
    const auto pattern=normal(match),path=normal(processPath);
    const bool folder=pattern.back()==L'\\';
    if(folder?path.size()<=pattern.size():path.size()!=pattern.size())return false;
    return CompareStringOrdinal(pattern.c_str(),int(pattern.size()),path.c_str(),int(pattern.size()),TRUE)==CSTR_EQUAL;
#else
    return match.back()=='/'?processPath.rfind(match,0)==0:match==processPath;
#endif
}
}

// ---- CoreRuntime: the list, its file, and switching ----
void CoreRuntime::LoadGames() {
    m_Games.clear();
    try {
        std::ifstream input(StoreFile());
        nlohmann::json store;
        if(input)input>>store;
        if(store.is_object()) {
            m_GameAutoSwitch=store.value("autoSwitch",true);
            if(store.contains("games")&&store["games"].is_array())for(const auto& item:store["games"]) {
                if(!item.is_object())continue;
                GameProfileEntry game;
                game.id=item.value("id",std::string{});game.name=item.value("name",std::string{});game.match=item.value("match",std::string{});
                game.profile=item.value("profile",std::string{});game.cover=item.value("cover",std::string{});game.steamAppId=item.value("steamAppId",0);
                if(!game.id.empty()&&!game.name.empty()&&!game.match.empty()&&!game.profile.empty())m_Games.push_back(std::move(game));
            }
        }
    } catch(...) { LOGE("Failed to load game profiles"); }
    m_GameList=std::make_shared<const std::vector<GameProfileEntry>>(m_Games);
}
void CoreRuntime::StoreGames() {
    m_GameList=std::make_shared<const std::vector<GameProfileEntry>>(m_Games);
    auto games=nlohmann::json::array();
    for(const auto& game:m_Games)games.push_back({{"id",game.id},{"name",game.name},{"match",game.match},{"profile",game.profile},{"cover",game.cover},{"steamAppId",game.steamAppId}});
    fs::create_directories(DataFolder());
    std::ofstream output(StoreFile());
    output<<nlohmann::json{{"autoSwitch",m_GameAutoSwitch},{"games",games}}.dump(4);
    if(!output)throw std::runtime_error("ProfileSaveFailed");
}
std::string CoreRuntime::AddGame(std::string name,std::string match,int steamAppId) {
    if(name.empty()||match.empty())throw std::runtime_error("GameProfileInvalid");
    for(const auto& game:m_Games)if(GameLibrary::Matches(game.match,match)||GameLibrary::Matches(match,game.match)||game.match==match)throw std::runtime_error("GameProfileExists");
    GameProfileEntry game;
    game.name=std::move(name);game.match=std::move(match);game.steamAppId=steamAppId;
    const auto base=SafeName(game.name);game.id=base;
    for(int n=2;std::any_of(m_Games.begin(),m_Games.end(),[&](const GameProfileEntry& other){return other.id==game.id;});++n)game.id=base+" "+std::to_string(n);
    game.profile=sago::getDocumentsFolder()+"/DualsenseZ/GameProfiles/"+game.id+".dsy";
    // A new game starts from the user's own settings, not from another game's while that one is active.
    try { SaveSettingsToFile(m_ActiveGame.empty()?m_Settings[m_SelectedController]:m_BeforeGame,game.profile); }
    catch(...) { throw std::runtime_error("ProfileSaveFailed"); }
    if(steamAppId>0) try {
        // Steam already has this cover on disk; keep our own copy so it survives Steam clearing its cache.
        if(const auto local=GameLibrary::SteamCover(steamAppId);!local.empty()) {
            const auto copy=Path(GameLibrary::CoversFolder())/(std::to_string(steamAppId)+".jpg");
            fs::copy_file(Path(local),copy,fs::copy_options::overwrite_existing);
            game.cover=Text(copy);
        }
    } catch(...) {}
    auto id=game.id;
    m_Games.push_back(std::move(game));StoreGames();
    return id;
}
void CoreRuntime::RemoveGame(const std::string& id) {
    const auto found=std::find_if(m_Games.begin(),m_Games.end(),[&](const GameProfileEntry& game){return game.id==id;});
    if(found==m_Games.end())throw std::runtime_error("GameProfileNotFound");
    if(m_ActiveGame==id)DeactivateGame();
    std::error_code error;fs::remove(Path(found->profile),error);
    m_Games.erase(found);StoreGames();
}
void CoreRuntime::SetGameCover(const std::string& id,std::string cover) {
    for(auto& game:m_Games)if(game.id==id){game.cover=std::move(cover);StoreGames();return;}
    throw std::runtime_error("GameProfileNotFound");
}
void CoreRuntime::SetGameAutoSwitch(bool enabled) {
    m_GameAutoSwitch=enabled;
    if(!enabled&&!m_GameManual)DeactivateGame();
    StoreGames();
}
void CoreRuntime::ToggleGame(const std::string& id) {
    if(m_ActiveGame==id){DeactivateGame();return;}
    const auto found=std::find_if(m_Games.begin(),m_Games.end(),[&](const GameProfileEntry& game){return game.id==id;});
    if(found==m_Games.end())throw std::runtime_error("GameProfileNotFound");
    ActivateGame(*found,true);
}
void CoreRuntime::ActivateGame(const GameProfileEntry& game,bool manual) {
    DeactivateGame();
    m_GameSlot=m_SelectedController;
    auto& settings=m_Settings[m_GameSlot];
    m_BeforeGame=settings;
    s_scePadSettings loaded=settings;
    if(LoadSettingsFromFile(&loaded,game.profile))settings=loaded;
    else { try { SaveSettingsToFile(settings,game.profile); } catch(...) {} settings.profilePath=game.profile; }
    settings.WasHidHideRanAfterLoad=false;
    ForceControllerToNotLoadDefault(m_GameSlot);
    ApplyTriggerParameters(settings);
    m_ActiveGame=game.id;m_GameManual=manual;
}
void CoreRuntime::DeactivateGame() {
    if(m_ActiveGame.empty())return;
    // Whatever was changed while the game's profile was active belongs to that game.
    for(const auto& game:m_Games)if(game.id==m_ActiveGame) { try { SaveSettingsToFile(m_Settings[m_GameSlot],game.profile); } catch(...) { LOGE("Failed to save a game profile"); } }
    m_Settings[m_GameSlot]=m_BeforeGame;
    m_Settings[m_GameSlot].WasHidHideRanAfterLoad=false;
    ApplyTriggerParameters(m_Settings[m_GameSlot]);
    m_ActiveGame.clear();m_GameManual=false;
}
void CoreRuntime::ScanGames() {
    const auto now=std::chrono::steady_clock::now();
    if(now<m_NextGameScan)return;
    m_NextGameScan=now+std::chrono::seconds(3);
    if(!m_GameAutoSwitch||m_Games.empty())return;
    const auto processes=GameLibrary::RunningProcessPaths();
    const GameProfileEntry* running=nullptr;bool activeRunning=false;
    for(const auto& game:m_Games) {
        const bool found=std::any_of(processes.begin(),processes.end(),[&](const std::string& path){return GameLibrary::Matches(game.match,path);});
        if(!found)continue;
        if(game.id==m_ActiveGame)activeRunning=true;
        if(!running)running=&game;
    }
    if(activeRunning)return;
    if(running)ActivateGame(*running,false);
    else if(!m_GameManual)DeactivateGame(); // a profile picked by hand stays until it is picked again
}
