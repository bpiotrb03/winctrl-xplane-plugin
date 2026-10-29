#ifndef RMP_TCAS_CONFIG_H
#define RMP_TCAS_CONFIG_H

#include "appstate.h"
#include "dataref.h"
#include "path.hpp"
#include "product-rmp.h"
#include "SimpleIni.h"

#include <functional>
#include <string>

// The installer writes <plugin>/rmp-tcas.ini: [aircraft] holds one key per
// aircraft family (toliss, ff777, b737) and [rmp] tcas_button names the key that
// toggles the TCAS mode. A missing file or key means on, toggled by LOAD, so
// builds installed without those pages keep working. Read once per profile.
struct RMPTcasSettings {
        bool enabled = true;
        std::string buttonName = "LOAD"; // As named in the profiles' buttonDefs
        RMPLed led = RMPLed::LOAD;
};

inline RMPTcasSettings loadRmpTcasSettings(const char *aircraft) {
    static const struct {
            const char *key;
            const char *buttonName;
            RMPLed led;
    } buttons[] = {
        {"LOAD", "LOAD", RMPLed::LOAD},
        {"VHF1", "VHF 1", RMPLed::VHF1},
        {"VHF2", "VHF 2", RMPLed::VHF2},
        {"VHF3", "VHF 3", RMPLed::VHF3},
        {"HF1", "HF1", RMPLed::HF1},
        {"HF2", "HF2", RMPLed::HF2},
        {"AM", "AM", RMPLed::AM},
        {"NAV", "NAV", RMPLed::NAV},
        {"VOR", "VOR", RMPLed::VOR},
        {"ILS", "ILS", RMPLed::ILS},
        {"GLS", "GLS", RMPLed::GLS},
        {"MLS", "MLS", RMPLed::MLS},
        {"ADF", "ADF", RMPLed::ADF},
    };

    RMPTcasSettings settings;
    CSimpleIniA ini;
    ini.SetUnicode();
    std::string path = AppState::getInstance()->getPluginDirectory() + "/rmp-tcas.ini";
    if (ini.LoadFile(utf8ToPath(path).c_str()) < 0) {
        return settings;
    }

    settings.enabled = ini.GetBoolValue("aircraft", aircraft, true);
    std::string key = ini.GetValue("rmp", "tcas_button", "LOAD");
    for (const auto &button : buttons) {
        if (key == button.key) {
            settings.buttonName = button.buttonName;
            settings.led = button.led;
        }
    }

    return settings;
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
