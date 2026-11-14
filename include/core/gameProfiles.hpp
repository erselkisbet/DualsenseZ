#pragma once
// Per-game controller profiles. Framework-free: shared by the core and the UI.
// Every path in this header is UTF-8.
#include <string>
#include <vector>

struct GameProfileEntry {
    std::string id;      // unique, and the name of the profile file
    std::string name;
    std::string match;   // the game's executable, or its install folder ending in a slash
    std::string profile; // .dsy file holding this game's controller settings
    std::string cover;   // local image file; empty until one has been found
    int steamAppId = 0;  // 0 when the game did not come from the Steam library
};
struct InstalledGame {
    std::string name;
    std::string folder;  // install folder ending in a slash
    int steamAppId = 0;
};
namespace GameLibrary {
// Games in the local Steam libraries, sorted by name. Reads Steam's own files; no network.
std::vector<InstalledGame> InstalledSteamGames();
// The portrait cover Steam already keeps on disk for a game, or an empty string.
std::string SteamCover(int appId);
// A readable name for an executable: its product name, else its file name.
std::string ExecutableName(const std::string& path);
// Folder for downloaded covers (created on demand).
std::string CoversFolder();
std::vector<std::string> RunningProcessPaths();
// Whether a running process belongs to a game: same executable, or inside its install folder.
bool Matches(const std::string& match,const std::string& processPath);
}
