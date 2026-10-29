#ifndef RMP_TCAS_CONFIG_H
#define RMP_TCAS_CONFIG_H

#include "appstate.h"
#include "dataref.h"
#include "path.hpp"
#include "product-rmp.h"
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

// MLS has no function on any of these panels, so it carries the transponder IDENT.
// The press is passed through with its phase; MLS lights for the ~18 s an ident lasts.
inline constexpr const char *kRmpIdent = "winctrl/rmp/ident";

inline void rmpIdent(ProductRMP *product, void *owner, const char *command, XPLMCommandPhase phase) {
    Dataref::getInstance()->executeCommand(command, phase);
    if (phase != xplm_CommandBegin) {
        return;
    }

    product->setLedBrightness(RMPLed::MLS, 255);
    AppState::getInstance()->executeAfterDebounced("rmp-ident-led", 18000, owner, [product]() {
        product->setLedBrightness(RMPLed::MLS, 0);
    });
}

#endif
