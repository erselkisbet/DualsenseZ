# Third-party notices

DualsenseZ is built on the work listed below. Each component keeps its own license; the full license texts are in the linked folders or upstream repositories.

| Component | Author | License | Location |
| --- | --- | --- | --- |
| [DualSenseY-v2](https://github.com/WujekFoliarz/DualSenseY-v2) (original project: controller, emulation, audio and UDP core) | WujekFoliarz | MIT | `source/`, `include/` — see [LICENSE](LICENSE) |
| [duaLib](https://github.com/WujekFoliarz/duaLib) | WujekFoliarz | MIT | `thirdparty/duaLib` |
| [HIDAPI](https://github.com/libusb/hidapi) | Alan Ott, Signal 11 Software and contributors | BSD-style (used under the BSD option of its triple license) | `thirdparty/duaLib/src/thirdparty/hidapi` |
| [JSON for Modern C++](https://github.com/nlohmann/json) | Niels Lohmann | MIT | `thirdparty/json` |
| [miniaudio](https://github.com/mackron/miniaudio) | David Reid | Public domain or MIT-0 | `thirdparty/miniaudio` |
| [ViGEmClient](https://github.com/nefarius/ViGEmClient) | Benjamin Höglinger-Stelzer | MIT | `thirdparty/ViGEmClient` |
| [Asio](https://github.com/chriskohlhoff/asio) 1.12.2 | Christopher M. Kohlhoff | Boost Software License 1.0 | `thirdparty/asio-cmake/asio-asio-1-12-2-src` |
| [asio-cmake](https://github.com/kingsamchen/asio-cmake) | Kingsley Chen | MIT | `thirdparty/asio-cmake` |
| [PlatformFolders](https://github.com/sago007/PlatformFolders) | Poul Sander | MIT | `thirdparty/PlatformFolders` |
| BTControl (Bluetooth disconnect helper, shipped as `resources/externals/windows/BTControl.exe`) | DualSenseY-v2 project | Distributed with the original project | source in `thirdparty/externals-non-cpp/BTControl` |
| [Windows App SDK / WinUI 3](https://github.com/microsoft/WindowsAppSDK) | Microsoft | MIT | NuGet package, redistributed with the app |
| [C++/WinRT](https://github.com/microsoft/cppwinrt) | Microsoft | MIT | NuGet package (build time) |

Wireless haptics in the original project build on [SAxense](https://github.com/egormanga/SAxense).

## Game covers

Cover images on the Profiles page are not part of DualsenseZ. They are copied from the local Steam client's cache or downloaded from Steam's public image servers when you add a game, stored under `Documents\DualsenseZ\Covers`, and belong to their respective publishers.

## Optional drivers (not included)

- [ViGEmBus](https://github.com/nefarius/ViGEmBus) — required for Xbox 360 / DualShock 4 emulation.
- [HidHide](https://github.com/nefarius/HidHide) — required to hide the physical controller from games.

## Trademarks

DualsenseZ is not affiliated with or endorsed by Sony Interactive Entertainment. DualSense, DualShock and PlayStation are trademarks of Sony Interactive Entertainment. Xbox is a trademark of Microsoft.
