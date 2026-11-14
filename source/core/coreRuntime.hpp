#pragma once
#include "appSettings.hpp"
#include "scePadSettings.hpp"
#include "strings.hpp"
#include "audioPassthrough.hpp"
#include "controllerEmulation.hpp"
#include "udp.hpp"
#include "keyboardMouseMapper.hpp"
#include <memory>
#include <future>
#include "core/workerExchange.hpp"
#include "core/gameProfiles.hpp"

// Internal, owner-thread-only backend implementation.
// Single host-thread API. Tick and the temporary legacy references must not be
// used concurrently. Scheduling belongs to the host, not to this runtime.
class CoreRuntime {
public:
    enum class StartResult { Running, ForwardedToExistingInstance };
    struct TickResult { bool bringToFront = false; };
    CoreRuntime() = default;
    ~CoreRuntime();
    CoreRuntime(const CoreRuntime&) = delete;
    CoreRuntime& operator=(const CoreRuntime&) = delete;
    StartResult Start(const std::string& profilePath = "");
    void Stop();
    void SelectController(int index);
    int SelectedController() const { return m_SelectedController; }
    TickResult Tick();
    bool AudioFailed(int index) const { return index>=0 && index<4 && m_AudioFailed[index]; }
    bool HidingAvailable() const { return m_HidingAvailable; }

    // Internal owner-thread access; never a UI binding API.
    s_scePadSettings* LegacyControllerSettings() { return m_Settings; }
    AppSettings& Settings() { return m_AppSettings; }
    Strings& Translations() { return m_Strings; }
    AudioPassthrough& Audio() { return *m_Audio; }
    Vigem& Emulation() { return *m_Vigem; }
    UDP& ModConnection() { return *m_Udp; }

    // Per-game profiles. One game's profile can be active at a time, on the controller selected when it started.
    const std::shared_ptr<const std::vector<GameProfileEntry>>& GameList() const { return m_GameList; }
    const std::string& ActiveGame() const { return m_ActiveGame; }
    bool GameAutoSwitch() const { return m_GameAutoSwitch; }
    std::string AddGame(std::string name,std::string match,int steamAppId);
    void RemoveGame(const std::string& id);
    void SetGameCover(const std::string& id,std::string cover);
    void SetGameAutoSwitch(bool enabled);
    void ToggleGame(const std::string& id);
    void ApplyGame(const std::string& id,bool manual);
    // A running game from an installed library that has no profile yet, or null.
    const std::shared_ptr<const InstalledGame>& SuggestedGame() const { return m_Suggestion; }
private:
    void Synchronize();
    void LoadGames();
    void StoreGames();
    void ActivateGame(const GameProfileEntry& game,bool manual);
    void DeactivateGame();
    void ScanGames();
    s_scePadSettings m_Settings[4]{};
    WorkerExchange m_Workers;
    AppSettings m_AppSettings{};
    Strings m_Strings;
    std::unique_ptr<UDP> m_Udp;
    std::unique_ptr<AudioPassthrough> m_Audio;
    std::unique_ptr<Vigem> m_Vigem;
    std::unique_ptr<KeyboardMouseMapper> m_Mapper;
    bool m_OwnsSession=false;
    bool m_Initialized=false, m_Running=false, m_Started=false;
    bool m_IsAdminWindows=false;
    bool m_HidingAvailable=false;
    bool m_AudioStarted[4]{}, m_AudioFailed[4]{};
    bool m_LastConnected[4]{};
    std::chrono::steady_clock::time_point m_ConnectedAt[4]{};
    int m_LightbarResends[4]{};
    bool m_LightbarNudged[4]{};
    bool m_LightingRestored[4]{};
    bool m_SessionRestored[4]{}; // every setting loaded from the last session // lighting loaded from the last session's saved preferences
    std::array<int,3> m_LastLightbar[4]{{-1,-1,-1},{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}};
    int m_SelectedController=0;
    std::vector<GameProfileEntry> m_Games;
    std::shared_ptr<const std::vector<GameProfileEntry>> m_GameList=std::make_shared<const std::vector<GameProfileEntry>>();
    std::string m_ActiveGame;
    s_scePadSettings m_BeforeGame{}; // the user's own settings while a game's profile is active
    int m_GameSlot=0;
    bool m_GameManual=false, m_GameAutoSwitch=true;
    std::chrono::steady_clock::time_point m_NextGameScan{}, m_NextLibraryScan{};
    std::vector<InstalledGame> m_Library;
    std::future<std::vector<InstalledGame>> m_LibraryScan;
    std::shared_ptr<const InstalledGame> m_Suggestion;
};
