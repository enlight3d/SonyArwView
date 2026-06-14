// Debug.cpp
#include "Debug.h"

#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <stdlib.h>  // _countof

namespace dbg {
namespace {

struct LogState {
    bool             enabled = false;
    wchar_t          path[MAX_PATH] = {};
    CRITICAL_SECTION cs;
    bool             csInit = false;

    LogState() noexcept {
        wchar_t buf[8] = {};
        bool on = false;
        if (GetEnvironmentVariableW(L"SONY_ARW_THUMB_DEBUG", buf, 8) > 0 && buf[0] == L'1')
            on = true;
        if (!on && GetEnvironmentVariableW(L"SONY_ARW_WIC_DEBUG", buf, 8) > 0 && buf[0] == L'1')
            on = true;
        enabled = on;
        if (enabled) {
            wchar_t temp[MAX_PATH] = {};
            const DWORD t = GetTempPathW(MAX_PATH, temp);
            if (t > 0 && t < MAX_PATH) {
                _snwprintf_s(path, _countof(path), _TRUNCATE,
                             L"%sSonyArwThumbnailProvider.log", temp);
                InitializeCriticalSection(&cs);
                csInit = true;
            } else {
                enabled = false;
            }
        }
    }
    ~LogState() noexcept { if (csInit) DeleteCriticalSection(&cs); }
};

LogState& State() noexcept { static LogState s; return s; }

void WriteLine(const wchar_t* line) noexcept {
    LogState& st = State();
    if (!st.enabled || !st.csInit) return;
    EnterCriticalSection(&st.cs);
    HANDLE h = CreateFileW(st.path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        char utf8[2048];
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8,
                                              sizeof(utf8) - 2, nullptr, nullptr);
        if (bytes > 1) {
            int len = bytes - 1;
            if (len > static_cast<int>(sizeof(utf8)) - 2) len = sizeof(utf8) - 2;
            utf8[len++] = '\r';
            utf8[len++] = '\n';
            DWORD wrote = 0;
            WriteFile(h, utf8, static_cast<DWORD>(len), &wrote, nullptr);
        }
        CloseHandle(h);
    }
    LeaveCriticalSection(&st.cs);
}

} // namespace

bool Enabled() noexcept { return State().enabled; }

void Logf(const wchar_t* fmt, ...) noexcept {
    if (!State().enabled) return;
    wchar_t body[1536];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(body, _countof(body), _TRUNCATE, fmt, ap);
    va_end(ap);

    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t line[1792];
    _snwprintf_s(line, _countof(line), _TRUNCATE,
                 L"%04u-%02u-%02u %02u:%02u:%02u.%03u [pid:%lu tid:%lu] %s",
                 t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
                 t.wMilliseconds, GetCurrentProcessId(), GetCurrentThreadId(), body);
    WriteLine(line);
}

void LogHr(const wchar_t* what, HRESULT hr) noexcept {
    Logf(L"%s -> hr=0x%08X", what, static_cast<unsigned>(hr));
}

} // namespace dbg
