#include "zibo-rmp-profile.h"

#include "appstate.h"
#include "dataref.h"
#include "product-rmp.h"
#include "rmp-tcas-config.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {
    constexpr const char *kTcasModeToggle = "winctrl/rmp/tcas_mode_toggle";
    constexpr const char *kKnobDepress = "winctrl/rmp/knob_depress";
    constexpr const char *kSquawkDataref = "sim/cockpit2/radios/actuators/transponder_code";
    constexpr const char *kModeSwitchDataref = "laminar/B738/knob/transponder_pos";
    // Mode switch positions from STBY on; transponder_pos counts TEST as 0 before them.
    constexpr const char *kTcasSwitchLabels[] = {"STBY", "ALTOFF", "XPNDR", "TA", "TARA"};
    constexpr const char *kTcasSwitchCommands[] = {
        "laminar/B738/knob/transponder_stby",
        "laminar/B738/knob/transponder_altoff",
        "laminar/B738/knob/transponder_alton",
        "laminar/B738/knob/transponder_ta",
        "laminar/B738/knob/transponder_tara",
    };
    constexpr int kTcasSwitchPositions = 5;
    constexpr const char *kSquawkCommitTask = "zibo-rmp-squawk-commit";
    constexpr int kSquawkCommitDelayMs = 3000;

    // The segment parser right-aligns, so pad both sides to centre in the six digit window.
    std::string centred(const std::string &text) {
        if (text.length() >= 6) {
            return text;
        }
        size_t left = (6 - text.length()) / 2;
        return std::string(left, ' ') + text + std::string(6 - text.length() - left, ' ');
    }

    // VHF1/VHF2 use the standard 8.33 datarefs where the value is the frequency in
    // kHz (e.g. 122800 -> "122.800").
    std::string formatVhfFreq(int hz833) {
        if (hz833 <= 0) {
            return "      ";
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", hz833 / 1000.0);
        return std::string(buf);
    }

    // COM3 exposes the frequency split into an integer MHz part and a kHz part
    // (e.g. MHz=118, kHz=25 -> "118.025").
    std::string formatCom3Freq(float mhz, float khz) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f.%03.0f", mhz, khz);
        return std::string(buf);
    }
} // namespace

const char *ZiboRMPProfile::rtpName() const {
    switch (product->deviceVariant) {
        case RMPDeviceVariant::VARIANT_CAPTAIN:
            return "L";
        case RMPDeviceVariant::VARIANT_STBY:
            return "D";
        case RMPDeviceVariant::VARIANT_FIRSTOFFICER:
            return "R";
    }
    return "L";
}

ZiboRMPProfile::ZiboRMPProfile(ProductRMP *product) : RMPAircraftProfile(product) {
    std::string statusBase = std::string("laminar/B738/comm/rtp_") + rtpName() + "/";

    _displayDatarefs = {
        "sim/cockpit/electrical/avionics_on",
        statusBase + "off_status",
        statusBase + "vhf_1_status",
        statusBase + "vhf_2_status",
        statusBase + "vhf_3_status",
        "sim/cockpit2/radios/actuators/com1_frequency_hz_833",
        "sim/cockpit2/radios/actuators/com1_standby_frequency_hz_833",
        "sim/cockpit2/radios/actuators/com2_frequency_hz_833",
        "sim/cockpit2/radios/actuators/com2_standby_frequency_hz_833",
        "laminar/B738/comm/com3/act_freq_MHz",
        "laminar/B738/comm/com3/act_freq_kHz",
        "laminar/B738/comm/com3/stdby_freq_MHz",
        "laminar/B738/comm/com3/stdby_freq_kHz",
        kSquawkDataref,
        kModeSwitchDataref,
    };

    Dataref::getInstance()->monitorExistingDataref<std::vector<float>>("laminar/B738/electric/panel_brightness", [product](const std::vector<float> &brightness) {
        bool hasPower = Dataref::getInstance()->getCached<bool>("sim/cockpit/electrical/avionics_on");
        uint8_t backlight = (hasPower && brightness.size() > 3) ? static_cast<uint8_t>(brightness[3] * 255) : 0;

        product->setLedBrightness(RMPLed::BACKLIGHT, backlight);
        product->setLedBrightness(RMPLed::LCD_BRIGHTNESS, backlight);
        product->setLedBrightness(RMPLed::OVERALL_LEDS_BRIGHTNESS, backlight);

        product->forceStateSync();
    },
        this);

    Dataref::getInstance()->monitorExistingDataref<bool>("sim/cockpit/electrical/avionics_on", [this](bool poweredOn) {
        Dataref::getInstance()->executeChangedCallbacksForDataref("laminar/B738/electric/panel_brightness");
        updateDisplays();
    },
        this);

    // Button LEDs driven by the selected-mode status datarefs on this panel.
    std::vector<std::pair<RMPLed, std::string>> ledRefs = {
        {RMPLed::VHF1, "vhf_1_status"},
        {RMPLed::VHF2, "vhf_2_status"},
        {RMPLed::VHF3, "vhf_3_status"},
        {RMPLed::HF1, "hf_1_status"},
        {RMPLed::HF2, "hf_2_status"},
        {RMPLed::AM, "am_status"},
        {RMPLed::SEL, "offside_tuning_status"},
    };

    for (const auto &ref : ledRefs) {
        std::string full = statusBase + ref.second;
        RMPLed led = ref.first;
        Dataref::getInstance()->monitorExistingDataref<float>(full.c_str(), [product, led](float status) {
            product->setLedBrightness(led, status > 0.5f ? 255 : 0);
        },
            this);
    }
}

bool ZiboRMPProfile::IsEligible() {
    return Dataref::getInstance()->exists("zibomod/Aircraft_Path");
}

const std::vector<std::string> &ZiboRMPProfile::displayDatarefs() const {
    return _displayDatarefs;
}

const std::unordered_map<uint16_t, RMPButtonDef> &ZiboRMPProfile::buttonDefs() const {
    static std::unordered_map<RMPDeviceVariant, std::unordered_map<uint16_t, RMPButtonDef>> cache;

    if (cache.find(product->deviceVariant) == cache.end()) {
        std::string p = std::string("laminar/B738/rtp_") + rtpName() + "/";

        cache[product->deviceVariant] = {
            {0, {"Flip frequencies", p + "freq_txfr/sel_switch"}},
            {1, {"VHF 1", p + "vhf_1/sel_switch"}},
            {2, {"VHF 2", p + "vhf_2/sel_switch"}},
            {3, {"VHF 3", p + "vhf_3/sel_switch"}},
            {4, {"LOAD", kTcasModeToggle}},
            {5, {"HF1", p + "hf_1/sel_switch"}},
            {6, {"HF2", p + "hf_2/sel_switch"}},
            {7, {"AM", p + "am/sel_switch"}},
            // NAV/VOR/ILS/GLS/MLS/ADF have no equivalent on the 737 radio panel.
            {8, {"NAV", ""}},
            {9, {"VOR", ""}},
            {10, {"ILS", ""}},
            {11, {"GLS", ""}},
            {12, {"MLS", ""}},
            {13, {"ADF", ""}},
            {14, {"Knob Large L", p + "freq_MHz/sel_dial_dn"}},
            {15, {"Knob Large R", p + "freq_MHz/sel_dial_up"}},
            {16, {"Knob Small L", p + "freq_khz/sel_dial_dn"}},
            {17, {"Knob Small R", p + "freq_khz/sel_dial_up"}},
            {18, {"Knob depress", kKnobDepress}},
            // The 737 panel has no separate ON; selecting a radio re-powers it.
            {19, {"Switch On", ""}},
            {20, {"Switch Off", p + "off_switch"}},
        };
    }

    return cache[product->deviceVariant];
}

void ZiboRMPProfile::buttonPressed(const RMPButtonDef *button, XPLMCommandPhase phase) {
    if (!button || button->dataref.empty() || phase == xplm_CommandContinue) {
        return;
    }

    if (button->dataref == kTcasModeToggle) {
        if (phase == xplm_CommandBegin && (tcasMode || rmpTcasEnabled("zibo"))) {
            setTcasMode(!tcasMode);
        }
        return;
    }

    if (tcasMode) {
        if (button->name == "Flip frequencies") {
            if (phase == xplm_CommandBegin) {
                swapHold.begin(this, []() {
                    Dataref::getInstance()->executeCommand("laminar/B738/push_button/transponder_ident_dn");
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

void ZiboRMPProfile::updateDisplays() {
    if (!product) {
        return;
    }

    auto dm = Dataref::getInstance();
    std::string statusBase = std::string("laminar/B738/comm/rtp_") + rtpName() + "/";

    bool hasPower = dm->getCached<bool>("sim/cockpit/electrical/avionics_on");
    bool off = dm->getCached<float>((statusBase + "off_status").c_str()) > 0.5f;

    if (!hasPower || off) {
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

    std::string active = "      ";
    std::string stby = "      ";

    if (dm->getCached<float>((statusBase + "vhf_1_status").c_str()) > 0.5f) {
        active = formatVhfFreq(dm->getCached<int>("sim/cockpit2/radios/actuators/com1_frequency_hz_833"));
        stby = formatVhfFreq(dm->getCached<int>("sim/cockpit2/radios/actuators/com1_standby_frequency_hz_833"));
    } else if (dm->getCached<float>((statusBase + "vhf_2_status").c_str()) > 0.5f) {
        active = formatVhfFreq(dm->getCached<int>("sim/cockpit2/radios/actuators/com2_frequency_hz_833"));
        stby = formatVhfFreq(dm->getCached<int>("sim/cockpit2/radios/actuators/com2_standby_frequency_hz_833"));
    } else if (dm->getCached<float>((statusBase + "vhf_3_status").c_str()) > 0.5f) {
        active = formatCom3Freq(dm->getCached<float>("laminar/B738/comm/com3/act_freq_MHz"), dm->getCached<float>("laminar/B738/comm/com3/act_freq_kHz"));
        stby = formatCom3Freq(dm->getCached<float>("laminar/B738/comm/com3/stdby_freq_MHz"), dm->getCached<float>("laminar/B738/comm/com3/stdby_freq_kHz"));
    }
    // HF/AM modes have no simple frequency source; the display stays blank.

    product->setDisplayText(active, stby);
}

std::string ZiboRMPProfile::squawkCode() const {
    int code = std::clamp(Dataref::getInstance()->getCached<int>(kSquawkDataref), 0, 7777);

    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%04d", code);
    return buffer;
}

int ZiboRMPProfile::tcasSwitchPosition() const {
    int position = static_cast<int>(std::lround(Dataref::getInstance()->getCached<float>(kModeSwitchDataref))) - 1;
    return std::clamp(position, 0, kTcasSwitchPositions - 1);
}

void ZiboRMPProfile::setTcasSwitchPosition(int position) {
    Dataref::getInstance()->executeCommand(kTcasSwitchCommands[position]);

    updateDisplays();
}

void ZiboRMPProfile::setTcasMode(bool enabled) {
    if (!enabled && squawkEditing) {
        commitSquawk();
    }

    tcasMode = enabled;
    tcasSwitchSelected = false;
    squawkLowerDigits = false;
    product->setLedBrightness(RMPLed::LOAD, enabled ? 255 : 0);
    updateDisplays();
}

void ZiboRMPProfile::stepSquawk(bool large, int delta) {
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

// The Zibo reads its code from the default transponder, as the WinWing TCAS panel profile sets it.
void ZiboRMPProfile::commitSquawk() {
    if (!squawkEditing) {
        return;
    }
    squawkEditing = false;

    int code = 0;
    for (int digit : squawkDigits) {
        code = code * 10 + digit;
    }
    Dataref::getInstance()->set<int>(kSquawkDataref, code);

    updateDisplays();
}
