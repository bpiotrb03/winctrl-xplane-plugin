#ifndef RMP_TCAS_CONFIG_H
#define RMP_TCAS_CONFIG_H

#include "appstate.h"
#include "path.hpp"
#include "SimpleIni.h"

#include <string>

// The installer writes <plugin>/rmp-tcas.ini with one key per aircraft family
// (toliss, ff777, zibo) under [aircraft]. A missing file or key leaves the
// TCAS mode on, so builds installed without the page keep working.
inline bool rmpTcasEnabled(const char *aircraft) {
    CSimpleIniA ini;
    ini.SetUnicode();
    std::string path = AppState::getInstance()->getPluginDirectory() + "/rmp-tcas.ini";
    if (ini.LoadFile(utf8ToPath(path).c_str()) < 0) {
        return true;
    }

    return ini.GetBoolValue("aircraft", aircraft, true);
}

#endif
