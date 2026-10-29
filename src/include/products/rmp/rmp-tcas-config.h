#ifndef RMP_TCAS_CONFIG_H
#define RMP_TCAS_CONFIG_H

#include "appstate.h"
#include "dataref.h"
#include "path.hpp"
#include "product-rmp.h"
#include "SimpleIni.h"

#include <functional>
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

// In TCAS mode the swap key does two jobs: a short press flips between the squawk
// and the mode switch, holding it for 3 s sends the transponder IDENT. The short
// press therefore acts on release, and only when the hold has not fired.
class RMPHoldButton {
    private:
        int pressId = 0;
        bool held = false;
        bool fired = false;

    public:
        static constexpr int kHoldMs = 3000;

        void begin(void *owner, std::function<void()> onHold) {
            int id = ++pressId;
            held = true;
            fired = false;
            AppState::getInstance()->executeAfter(kHoldMs, owner, [this, id, onHold]() {
                if (held && pressId == id) {
                    fired = true;
                    onHold();
                }
            });
        }

        // True when the press was short, so the caller runs its tap action.
        bool end() {
            bool wasHeld = held;
            held = false;
            return wasHeld && !fired;
        }
};

#endif
