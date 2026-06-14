// Debug.h - opt-in diagnostic logging for the WIC decoder.
//
// Logging is OFF by default. Enable by setting the environment variable
//   SONY_ARW_WIC_DEBUG=1
// Output is appended to:
//   %TEMP%\SonyArwWicDecoder.log
//
// The logger never throws and is safe to call from any COM entry point.
#pragma once

#include <windows.h>

namespace dbg {

// True if SONY_ARW_WIC_DEBUG=1 (evaluated once, cached).
bool Enabled() noexcept;

// Append a formatted line (with timestamp + PID/TID) to the log file.
// No-op when logging is disabled.
void Logf(_In_z_ _Printf_format_string_ const wchar_t* fmt, ...) noexcept;

// Convenience: log an HRESULT with context.
void LogHr(const wchar_t* what, HRESULT hr) noexcept;

} // namespace dbg
