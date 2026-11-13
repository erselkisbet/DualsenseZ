#pragma once
// Per-user "start with Windows" entry. The registry value is the source of truth;
// nothing is written unless the user changes the preference.
#ifdef WINDOWS
#include <windows.h>
#include <string>
namespace StartupRegistration {
inline constexpr wchar_t RunKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr wchar_t ValueName[]=L"DualsenseZ";
inline bool IsEnabled() {
    HKEY key;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,RunKey,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return false;
    const bool found=RegQueryValueExW(key,ValueName,nullptr,nullptr,nullptr,nullptr)==ERROR_SUCCESS;
    RegCloseKey(key);return found;
}
inline bool Set(bool enabled) {
    HKEY key;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,RunKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    LSTATUS status;
    if(enabled) {
        wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
        const std::wstring command=L"\""+std::wstring(exe)+L"\"";
        status=RegSetValueExW(key,ValueName,0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),DWORD((command.size()+1)*sizeof(wchar_t)));
    } else {
        status=RegDeleteValueW(key,ValueName);
        if(status==ERROR_FILE_NOT_FOUND)status=ERROR_SUCCESS;
    }
    RegCloseKey(key);return status==ERROR_SUCCESS;
}
}
#endif
