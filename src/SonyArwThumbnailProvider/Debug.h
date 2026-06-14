// Debug.h - opt-in diagnostic logging for the thumbnail provider.
//
// Enable with SONY_ARW_THUMB_DEBUG=1 (or SONY_ARW_WIC_DEBUG=1). Logs to:
//   %TEMP%\SonyArwThumbnailProvider.log
#pragma once

#include <windows.h>

namespace dbg {
bool Enabled() noexcept;
void Logf(_In_z_ _Printf_format_string_ const wchar_t* fmt, ...) noexcept;
void LogHr(const wchar_t* what, HRESULT hr) noexcept;
} // namespace dbg
