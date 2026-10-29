#include "ff777-rmp-profile.h"

#include "appstate.h"
#include "dataref.h"
#include "product-rmp.h"
#include "rmp-tcas-config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
    constexpr const char *kKnobDepress = "winctrl/rmp/knob_depress";
    constexpr const char *kSquawkDataref = "sim/cockpit/radios/transponder_code";
    constexpr const char *kModeSwitchDataref = "1-sim/ckpt/xpndrModeSwitch/anim";
    // Positions of the 777 transponder mode switch, as read from its anim dataref.
    constexpr const char *kTcasSwitchLabels[] = {"STBY", "ALTOFF", "XPNDR", "TA", "TARA"};
    constexpr int kTcasSwitchPositions = 5;
    constexpr const char *kSquawkCommitTask = "ff777-rmp-squawk-commit";
    constexpr const char *kSquawkVerifyTask = "ff777-rmp-squawk-verify";
    constexpr int kSquawkCommitDelayMs = 3000;
    constexpr int kSquawkVerifyDelayMs = 500;

    // The segment parser right-aligns, so pad both sides to centre in the six digit window.
    std::string centred(const std::string &text) {
        if (text.length() >= 6) {
            return text;
        }
        size_t left = (6 - text.length()) / 2;
        return std::string(left, ' ') + text + std::string(6 - text.length() - left, ' ');
    }
}

const char *FF777RMPProfile::rmpName() const {
    switch (product->deviceVariant) {
        case RMPDeviceVariant::VARIANT_CAPTAIN:
            return "L";
        case RMPDeviceVariant::VARIANT_STBY:
            return "B";
        case RMPDeviceVariant::VARIANT_FIRSTOFFICER:
            return "R";
    }
    return "L";
}

const char *FF777RMPProfile::sideName() const {
    switch (product->deviceVariant) {
        case RMPDeviceVariant::VARIANT_CAPTAIN:
            return "left";
        case RMPDeviceVariant::VARIANT_STBY:
            return "center";
        case RMPDeviceVariant::VARIANT_FIRSTOFFICER:
            return "right";
    }
    return "left";
}

const char *FF777RMPProfile::swapCommand() const {
    switch (product->deviceVariant) {
        case RMPDeviceVariant::VARIANT_CAPTAIN:
            return "1-sim/command/radLSwapSwitch_button";
        case RMPDeviceVariant::VARIANT_STBY:
            return "1-sim/command/radBSwapSwitch_button";
        case RMPDeviceVariant::VARIANT_FIRSTOFFICER:
            return "1-sim/command/radRSwapSwitch_button";
    }
    return "1-sim/command/radLSwapSwitch_button";
}

FF777RMPProfile::FF777RMPProfile(ProductRMP *product) : RMPAircraftProfile(product) {
    tcasSettings = loadRmpTcasSettings("ff777");

    _displayDatarefs = {
        std::string("1-sim/output/radio/") + sideName() + "Active",
        std::string("1-sim/output/radio/") + sideName() + "Stby",
        std::string("1-sim/ckpt/lamps/rad") + rmpName() + "Off",
        kSquawkDataref,
        kModeSwitchDataref,
    };

    _ledDatarefs = {
        {RMPLed::BACKLIGHT, "1-sim/ckpt/lights/aisle"},
        {RMPLed::VHF1, std::string("1-sim/ckpt/lamps/rad") + rmpName() + "VhfL"},
        {RMPLed::VHF2, std::string("1-sim/ckpt/lamps/rad") + rmpName() + "VhfR"},
        {RMPLed::VHF3, std::string("1-sim/ckpt/lamps/rad") + rmpName() + "VhfC"},
        {RMPLed::HF1, std::string("1-sim/ckpt/lamps/rad") + rmpName() + "HfL"},
        {RMPLed::HF2, std::string("1-sim/ckpt/lamps/rad") + rmpName() + "HfR"},
        {RMPLed::AM, std::string("1-sim/ckpt/lamps/rad") + rmpName() + "Am"},
        {RMPLed::SEL, std::string("1-sim/ckpt/lamps/rad") + rmpName() + "OfsdTun"},
    };

    for (const auto &pair : _ledDatarefs) {
        Dataref::getInstance()->monitorExistingDataref<float>(pair.second.c_str(), [product, pair](float brightness) {
            uint8_t ledBrightness = brightness * 255;

            product->setLedBrightness(pair.first, ledBrightness);
        },
            this);
    }

    Dataref::getInstance()->monitorExistingDataref<bool>("sim/cockpit/electrical/avionics_on", [this](bool poweredOn) {
        updateDisplays();
    },
        this);
}

bool FF777RMPProfile::IsEligible() {
    // FF777 datarefs that don't exist on the FF767
    return Dataref::getInstance()->exists("1-sim/ckpt/mcpApLButton/anim") &&
           Dataref::getInstance()->exists("1-sim/output/mcp/ok");
}

const std::vector<std::string> &FF777RMPProfile::displayDatarefs() const {
    return _displayDatarefs;
}

const std::unordered_map<uint16_t, RMPButtonDef> &FF777RMPProfile::buttonDefs() const {
    static std::unordered_map<RMPDeviceVariant, std::unordered_map<uint16_t, RMPButtonDef>> cache;

    if (cache.find(product->deviceVariant) == cache.end()) {
        cache[product->deviceVariant] = {
            {0, {"Flip frequencies", swapCommand()}},
            {1, {"VHF 1", std::string("1-sim/command/rad") + rmpName() + "LvhfButton_button"}},
            {2, {"VHF 2", std::string("1-sim/command/rad") + rmpName() + "RvhfButton_button"}},
            {3, {"VHF 3", std::string("1-sim/command/rad") + rmpName() + "CvhfButton_button"}},
            {4, {"LOAD", ""}},
            {5, {"HF1", std::string("1-sim/command/rad") + rmpName() + "LhfButton_button"}},
            {6, {"HF2", std::string("1-sim/command/rad") + rmpName() + "RhfButton_button"}},
            {7, {"AM", std::string("1-sim/command/rad") + rmpName() + "amButton_button"}},
            {8, {"NAV", ""}},
            {9, {"VOR", ""}},
            {10, {"ILS", ""}},
            {11, {"GLS", ""}},
            {12, {"MLS", ""}},
            {13, {"ADF", ""}},
            {14, {"Knob Large L", std::string("1-sim/command/rad") + rmpName() + "frBigRotary_rotary-"}},
            {15, {"Knob Large R", std::string("1-sim/command/rad") + rmpName() + "frBigRotary_rotary+"}},
            {16, {"Knob Small L", std::string("1-sim/command/rad") + rmpName() + "frSmallRotary_rotary-"}},
            {17, {"Knob Small R", std::string("1-sim/command/rad") + rmpName() + "frSmallRotary_rotary+"}},
            {18, {"Knob depress", kKnobDepress}},
            {19, {"Switch On", ""}},
            {20, {"Switch Off", ""}},
        };
    }

    return cache[product->deviceVariant];
}

void FF777RMPProfile::buttonPressed(const RMPButtonDef *button, XPLMCommandPhase phase) {
    if (!button || phase == xplm_CommandContinue) {
        return;
    }

    // The key picked in the installer toggles the mode and loses its radio function.
    if (tcasSettings.enabled && button->name == tcasSettings.buttonName) {
        if (phase == xplm_CommandBegin) {
            setTcasMode(!tcasMode);
        }
        return;
    }

    if (button->dataref.empty()) {
        return;
    }

    if (tcasMode) {
        if (button->name == "Flip frequencies") {
            if (phase == xplm_CommandBegin) {
                swapHold.begin(this, []() {
                    Dataref::getInstance()->executeCommand(XPLMFindCommand("1-sim/command/xpndrIdentButton_button") ? "1-sim/command/xpndrIdentButton_button" : "sim/transponder/transponder_ident");
                });
            } else if (phase == xplm_CommandEnd && swapHold.end()) {
                tcasSwitchSelected = !tcasSwitchSelected;
                updateDisplays();
            }
            return;
        }

        if (phase != xplm_CommandBegin) {
            return;
        }

        if (button->dataref == kKnobDepress) {
            squawkLowerDigits = !squawkLowerDigits;
            return;
        }

        int delta = 0;
        if (button->name == "Knob Large L" || button->name == "Knob Small L") {
            delta = -1;
        } else if (button->name == "Knob Large R" || button->name == "Knob Small R") {
            delta = 1;
        }

        if (delta != 0) {
            if (tcasSwitchSelected) {
                // A rotary switch with end stops: TA/RA does not wrap back to STBY.
                setTcasSwitchPosition(std::clamp(tcasSwitchPosition() + delta, 0, kTcasSwitchPositions - 1));
            } else {
                stepSquawk(button->name.starts_with("Knob Large"), delta);
            }
            return;
        }
    }

    if (button->dataref == kKnobDepress) {
        return;
    }

    auto datarefManager = Dataref::getInstance();
    if (button->datarefType == RMPDatarefType::SET_VALUE) {
        if (phase != xplm_CommandBegin) {
            return;
        }

        datarefManager->set<int>(button->dataref.c_str(), static_cast<int>(button->value));
    } else {
        datarefManager->executeCommand(button->dataref.c_str(), phase);
    }
}

void FF777RMPProfile::updateDisplays() {
    if (!product) {
        return;
    }

    std::string rmpOffRef = std::string("1-sim/ckpt/lamps/rad") + rmpName() + "Off";
    bool isOff = Dataref::getInstance()->getCached<float>(rmpOffRef.c_str()) > 0.01;
    if (isOff) {
        product->setDisplayText("      ", "      ");
        return;
    }

    if (tcasMode) {
        std::string code = squawkCode();
        if (squawkEditing) {
            code.clear();
            for (int digit : squawkDigits) {
                code += static_cast<char>('0' + digit);
            }
        }

        product->setDisplayText(centred("TCAS"), centred(tcasSwitchSelected ? kTcasSwitchLabels[tcasSwitchPosition()] : code));
        return;
    }

    std::string activeRef = std::string("1-sim/output/radio/") + sideName() + "Active";
    std::string stbyRef = std::string("1-sim/output/radio/") + sideName() + "Stby";
    std::string activeHz = Dataref::getInstance()->getCached<std::string>(activeRef.c_str());
    std::string stbyHz = Dataref::getInstance()->getCached<std::string>(stbyRef.c_str());

    auto formatHz = [](std::string s) -> std::string {
        if (s == "DATA") {
            return " DATA ";
        }
        if (s.find('.') == std::string::npos && s.length() > 3) {
            s.insert(s.length() - 3, ".");
        }
        return s;
    };

    product->setDisplayText(formatHz(activeHz), formatHz(stbyHz));
}

// The FF777 mirrors its ATC panel code into the default transponder dataref.
std::string FF777RMPProfile::squawkCode() const {
    int code = std::clamp(Dataref::getInstance()->getCached<int>(kSquawkDataref), 0, 7777);

    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%04d", code);
    return buffer;
}

int FF777RMPProfile::tcasSwitchPosition() const {
    int position = static_cast<int>(std::lround(Dataref::getInstance()->getCached<float>(kModeSwitchDataref)));
    return std::clamp(position, 0, kTcasSwitchPositions - 1);
}

// The mode switch has no _set_N commands, only steps. Builds differ on the suffix, so take the one that exists.
void FF777RMPProfile::setTcasSwitchPosition(int position) {
    int steps = position - tcasSwitchPosition();
    if (steps != 0) {
        const char *direction = steps > 0 ? "+" : "-";
        std::string command = std::string("1-sim/command/xpndrModeSwitch_switch") + direction;
        if (!XPLMFindCommand(command.c_str())) {
            command = std::string("1-sim/command/xpndrModeSwitch_rotary") + direction;
        }
        for (int n = 0; n < std::abs(steps); ++n) {
            Dataref::getInstance()->executeCommand(command.c_str());
        }
    }

    updateDisplays();
}

void FF777RMPProfile::setTcasMode(bool enabled) {
    if (!enabled && squawkEditing) {
        commitSquawk();
    }

    tcasMode = enabled;
    tcasSwitchSelected = false;
    squawkLowerDigits = false;
    product->setLedBrightness(tcasSettings.led, enabled ? 255 : 0);
    updateDisplays();
}

void FF777RMPProfile::stepSquawk(bool large, int delta) {
    if (!squawkEditing) {
        // Start from the code currently set.
        std::string code = squawkCode();
        for (int i = 0; i < 4; ++i) {
            squawkDigits[i] = (code[i] >= '0' && code[i] <= '7') ? code[i] - '0' : 0;
        }
        squawkEditing = true;
    }

    // Large knob: thousands (tens after a knob press). Small knob: hundreds (units).
    int index = (squawkLowerDigits ? 2 : 0) + (large ? 0 : 1);
    squawkDigits[index] = (squawkDigits[index] + delta + 8) % 8;

    AppState::getInstance()->executeAfterDebounced(kSquawkCommitTask, kSquawkCommitDelayMs, this, [this]() {
        commitSquawk();
    });

    updateDisplays();
}

void FF777RMPProfile::commitSquawk() {
    if (!squawkEditing) {
        return;
    }
    squawkEditing = false;

    // The ATC panel has one knob per digit (xpndrSelDig1Rotary is the leftmost).
    // Step each towards its target without passing 7/0, so it works whether the knob wraps or stops.
    std::string current = squawkCode();
    int target = 0;
    for (int i = 0; i < 4; ++i) {
        int from = current[i] - '0';
        int steps = squawkDigits[i] - from;
        std::string command = "1-sim/command/xpndrSelDig" + std::to_string(i + 1) + "Rotary_rotary" + (steps > 0 ? "+" : "-");
        for (int n = 0; n < std::abs(steps); ++n) {
            Dataref::getInstance()->executeCommand(command.c_str());
        }
        target = target * 10 + squawkDigits[i];
    }

    // Should the knobs not take, set the code on the default transponder instead.
    AppState::getInstance()->executeAfterDebounced(kSquawkVerifyTask, kSquawkVerifyDelayMs, this, [this, target]() {
        if (Dataref::getInstance()->get<int>(kSquawkDataref) != target) {
            Dataref::getInstance()->set<int>(kSquawkDataref, target);
        }
        updateDisplays();
    });

    updateDisplays();
}
