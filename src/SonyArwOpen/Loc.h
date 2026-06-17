// Loc.h - minimal English/French localization for SonyArwView's user-facing text.
//
// The display language follows the Windows UI language (French -> French, anything
// else -> English). Strings are plain C++ literals selected at runtime, so there
// is no resource/loader dependency. This file is UTF-8; the SonyArwOpen target is
// compiled with /utf-8 so the accented French literals get the right codepoints.
#pragma once

#include <windows.h>

namespace loc {

inline bool IsFrench() {
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_FRENCH;
}

// Settings window ----------------------------------------------------------
inline const wchar_t* GrpDefault() {
    return IsFrench() ? L"Application par d\x00E9" L"faut pour .arw" : L"Default app for .arw";
}
inline const wchar_t* StatusIsDefault() {
    return IsFrench() ? L"\x2713  SonyArwView est votre application .arw par d\x00E9" L"faut."
                      : L"\x2713  SonyArwView is your default for .arw.";
}
inline const wchar_t* StatusNotDefault() {
    return IsFrench() ? L"\x26A0  SonyArwView n'est pas encore l'application .arw par d\x00E9" L"faut."
                      : L"\x26A0  SonyArwView is not the default for .arw yet.";
}
inline const wchar_t* BtnSetDefault() {
    return IsFrench() ? L"D\x00E9" L"finir SonyArwView par d\x00E9" L"faut\x2026"
                      : L"Make SonyArwView the default\x2026";
}
inline const wchar_t* GrpOpen() {
    return IsFrench() ? L"Quand j'ouvre un fichier .arw" : L"When I open an .arw file";
}
inline const wchar_t* RadioPhotos() {
    return IsFrench() ? L"Afficher l'aper\x00E7u dans Photos Windows"
                      : L"Show the preview in Windows Photos";
}
inline const wchar_t* RadioViewer() {
    return IsFrench() ? L"L'ouvrir dans ma visionneuse (le fichier d'origine) :"
                      : L"Open it in my own viewer (the original file):";
}
inline const wchar_t* BtnBrowse() {
    return IsFrench() ? L"Parcourir\x2026" : L"Browse\x2026";
}
inline const wchar_t* ThumbsNote() {
    return IsFrench() ? L"Les miniatures de l'Explorateur fonctionnent dans les deux cas."
                      : L"Explorer thumbnails work either way.";
}
inline const wchar_t* BtnSave() { return IsFrench() ? L"Enregistrer" : L"Save"; }
inline const wchar_t* BtnClose() { return IsFrench() ? L"Fermer" : L"Close"; }

// File picker (Browse) -----------------------------------------------------
inline const wchar_t* BrowseTitle() {
    return IsFrench() ? L"Choisir l'application pour ouvrir les fichiers .arw"
                      : L"Choose the app to open .arw files with";
}
// Double-null-terminated OPENFILENAME filter.
inline const wchar_t* BrowseFilter() {
    return IsFrench() ? L"Programmes\0*.exe\0Tous les fichiers\0*.*\0"
                      : L"Programs\0*.exe\0All files\0*.*\0";
}
inline const wchar_t* NeedViewer() {
    return IsFrench()
        ? L"Choisissez une visionneuse avec Parcourir, ou s\x00E9lectionnez "
          L"\x00AB Afficher l'aper\x00E7u dans Photos Windows \x00BB."
        : L"Pick a viewer with Browse, or choose \"Show the preview in Windows Photos\".";
}

// Open handler -------------------------------------------------------------
inline const wchar_t* ExtractFailed() {
    return IsFrench() ? L"SonyArwView n'a pas trouv\x00E9 d'aper\x00E7u utilisable dans ce fichier .ARW."
                      : L"SonyArwView couldn't find a usable preview inside this .ARW file.";
}

} // namespace loc
