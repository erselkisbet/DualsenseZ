# Changelog

## 1.0.0-beta.2

### Added
- Game profiles: give a game its own controller settings and switch to them automatically while the game runs, or by clicking its cover. Installed games from the Steam, Epic Games, EA and Xbox libraries are listed for you; any other game can be added through its .exe. A game you start that has no profile yet is offered on the shelf and added with one click.
- Profiles page holding the local profiles (moved from the Controller page) and the game profiles.
- Update notice: on launch the app checks GitHub for a newer release and shows a link to its download page. It can be turned off in Settings; nothing is downloaded automatically.

### Changed
- The Controller page now holds keyboard shortcuts, input settings and controller visibility only.

## 1.0.0-beta.1

First public beta. DualsenseZ starts from [DualSenseY-v2](https://github.com/WujekFoliarz/DualSenseY-v2) and replaces its ImGui interface with a native WinUI 3 one.

### Added
- WinUI 3 interface: Overview, Controller, Adaptive Triggers, Sound & Haptics, Lighting, Motion & Touch, Diagnostics and Settings pages.
- English and Turkish interface; the first launch follows the Windows display language.
- "Start with Windows" option.
- "Hide to tray" choice: never, when minimized, when closed, or both.
- Lighting page with battery-based color, automatic color cycle and static color with presets and a picker with hex and RGB input.
- Controller settings are saved on exit and restored on the next launch.
- Live stick, trigger, haptics, motion and touchpad view on the Diagnostics page.
- UDP mod status and listening port on the Diagnostics page.

### Changed
- Light and dark title bar icons; new app icon.
- `.dsy` file association moved to Settings.

### Removed
- The online server and controller-sharing feature of the original project. UDP mod support is unchanged.
