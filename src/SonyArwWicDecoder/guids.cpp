// guids.cpp - the single translation unit that allocates storage for our GUIDs.
//
// Including <initguid.h> before "guids.h" turns each DEFINE_GUID in that header
// into an actual definition (instead of an extern declaration). Every other
// translation unit includes "guids.h" without <initguid.h> and just references
// these symbols.
#include <windows.h>
#include <initguid.h>
#include "guids.h"
