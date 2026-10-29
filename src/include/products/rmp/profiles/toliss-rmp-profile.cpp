#include "toliss-rmp-profile.h"

#include "appstate.h"
#include "dataref.h"
#include "product-rmp.h"
#include "rmp-tcas-config.h"
#include "xplane-version.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
    constexpr const char *kTcasModeToggle = "winctrl/rmp/tcas_mode_toggle";
    constexpr const char *kKnobDepress = "winctrl/rmp/knob_depress";
    constexpr const char *kTcasSwitchLabels[] = {"STBY", "TA", "TARA"};
    constexpr const char *kSquawkCommitTask = "toliss-rmp-squawk-commit";
    constexpr int kSquawkCommitDelayMs = 5000;

    // The segment parser right-aligns, so pad both sides to centre in the six digit window.
    std::string centred(const std::string &text) {
        if (text.length() >= 6) {
            return text;
        }
        size_t left = (6 - text.length()) / 2;
        return std::string(left, ' ') + text + std::string(6 - text.length() - left, ' ');
    }
}

const char *TolissRMPProfile::rmpName() const {
    switch (product->deviceVariant) {
        case RMPDeviceVariant::VARIANT_CAPTAIN:
            return "RMP1";
        case RMPDeviceVariant::VARIANT_STBY:
            return "RMP3";
        case RMPDeviceVariant::VARIANT_FIRSTOFFICER:
            return "RMP2";
    }
    return "RMP1";
}

const char *TolissRMPProfile::sideName() const {
    switch (product->deviceVariant) {
        case RMPDeviceVariant::VARIANT_CAPTAIN:
            return "Capt";
        case RMPDeviceVariant::VARIANT_STBY:
            return "RMP3";
        case RMPDeviceVariant::VARIANT_FIRSTOFFICER:
            return "Co";
    }
    return "Capt";
}

const char *TolissRMPProfile::swapCommand() const {
    switch (product->deviceVariant) {
        case RMPDeviceVariant::VARIANT_CAPTAIN:
            return "AirbusFBW/RMPSwapCapt";
        case RMPDeviceVariant::VARIANT_STBY:
            return "AirbusFBW/RMP3Swap";
        case RMPDeviceVariant::VARIANT_FIRSTOFFICER:
            return "AirbusFBW/RMPSwapCo";
    }
    return "AirbusFBW/RMPSwapCapt";
}

TolissRMPProfile::TolissRMPProfile(ProductRMP *product) : RMPAircraftProfile(product) {
    std::string rmpAvailRef = std::string("AirbusFBW/") + rmpName() + "Available";

    // ToLiss builds predating the window strings (A321 v1.3.2 and earlier, XP11)
    // only publish the raw frequencies, so fall back to formatting those.
    useFrequencyFallback = !Dataref::getInstance()->exists((std::string("AirbusFBW/") + rmpName() + "/ActiveWindowString").c_str());

    _displayDatarefs = {
        std::string("AirbusFBW/") + rmpName() + "/ActiveWindowString",
        std::string("AirbusFBW/") + rmpName() + "/StandbyWindowString",
        rmpAvailRef,
        "AirbusFBW/XPDRString",
        "AirbusFBW/XPDR1",
        "AirbusFBW/XPDR2",
        "AirbusFBW/XPDR3",
        "AirbusFBW/XPDR4",
        "AirbusFBW/XPDRPower",
        "AirbusFBW/XPDRTCASMode",
    };

    if (useFrequencyFallback) {
        _displayDatarefs.push_back(std::string("AirbusFBW/") + rmpName() + "Freq");
        _displayDatarefs.push_back(std::string("AirbusFBW/") + rmpName() + "StbyFreq");
    }

    Dataref::getInstance()->monitorExistingDataref<float>("AirbusFBW/PanelBrightnessLevel", [product, rmpAvailRef](float brightness) {
        bool available = Dataref::getInstance()->getCached<int>(rmpAvailRef.c_str()) != 0;
        bool hasPower = Dataref::getInstance()->getCached<bool>("sim/cockpit/electrical/avionics_on");
        uint8_t backlightBrightness = hasPower ? brightness * 255 : 0;

        product->setLedBrightness(RMPLed::BACKLIGHT, backlightBrightness);
        product->setLedBrightness(RMPLed::LCD_BRIGHTNESS, hasPower ? 255 : 0);
        product->setLedBrightness(RMPLed::OVERALL_LEDS_BRIGHTNESS, available ? 255 : 0);

        product->forceStateSync();
    },
        this);

    Dataref::getInstance()->monitorExistingDataref<int>(rmpAvailRef.c_str(), [](int available) {
        Dataref::getInstance()->executeChangedCallbacksForDataref("AirbusFBW/PanelBrightnessLevel");
    },
        this);

    Dataref::getInstance()->monitorExistingDataref<bool>("sim/cockpit/electrical/avionics_on", [this](bool poweredOn) {
        Dataref::getInstance()->executeChangedCallbacksForDataref("AirbusFBW/PanelBrightnessLevel");
        updateDisplays();
    },
        this);

    // XP11 only publishes the non-_Raw array, with brightness-scaled values
    std::string lightsRef = std::string("AirbusFBW/") + rmpName() + ifXPlane11<std::string>("Lights", "Lights_Raw");

    Dataref::getInstance()->monitorExistingDataref<std::vector<float>>(lightsRef.c_str(), [product](const std::vector<float> &brightness) {
        if (brightness.size() < 14) {
            return;
        }

        product->setLedBrightness(RMPLed::VHF1, brightness[1] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::VHF2, brightness[2] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::VHF3, brightness[3] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::HF1, brightness[4] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::HF2, brightness[5] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::AM, brightness[6] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::VOR, brightness[7] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::ILS, brightness[8] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::ADF, brightness[10] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::GLS, brightness[11] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::SEL, brightness[12] > std::numeric_limits<float>::epsilon() ? 255 : 0);
        product->setLedBrightness(RMPLed::NAV, brightness[13] > std::numeric_limits<float>::epsilon() ? 255 : 0);
    },
        this);

    std::string activeRef = std::string("AirbusFBW/") + rmpName() + "/ActiveWindowString";
    std::string stbyRef = std::string("AirbusFBW/") + rmpName() + "/StandbyWindowString";

    Dataref::getInstance()->monitorExistingDataref<std::string>(activeRef.c_str(), [this](std::string s) {
        updateDisplays();
    },
        this);

    Dataref::getInstance()->monitorExistingDataref<std::string>(stbyRef.c_str(), [this](std::string s) {
        updateDisplays();
    },
        this);

    if (useFrequencyFallback) {
        std::string activeFreqRef = std::string("AirbusFBW/") + rmpName() + "Freq";
        std::string stbyFreqRef = std::string("AirbusFBW/") + rmpName() + "StbyFreq";

        Dataref::getInstance()->monitorExistingDataref<float>(activeFreqRef.c_str(), [this](float freq) {
            updateDisplays();
        },
            this);

        Dataref::getInstance()->monitorExistingDataref<float>(stbyFreqRef.c_str(), [this](float freq) {
            updateDisplays();
        },
            this);
    }
}

std::string TolissRMPProfile::formatFrequency(float megahertz) const {
    if (megahertz < std::numeric_limits<float>::epsilon()) {
        return "";
    }

    // ToLiss steps VHF in 6.25 kHz internally and shows the value floored to
    // 5 kHz, so 122.76875 reads as 122.765. The offset absorbs float noise
    // on exact channels; VOR, ILS, ADF and HF are unaffected.
    float channel = std::floor(megahertz / 0.005f + 0.1f) * 0.005f;

    // Both windows are six digits. VHF, VOR and ILS arrive as MHz above 100, HF below it.
    int decimals = channel >= 100.0f ? 3 : 4;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, channel);

    return buffer;
}

bool TolissRMPProfile::IsEligible() {
    return Dataref::getInstance()->exists("AirbusFBW/PanelBrightnessLevel");
}

const std::vector<std::string> &TolissRMPProfile::displayDatarefs() const {
    return _displayDatarefs;
}

const std::unordered_map<uint16_t, RMPButtonDef> &TolissRMPProfile::buttonDefs() const {
    static std::unordered_map<RMPDeviceVariant, std::unordered_map<uint16_t, RMPButtonDef>> cache;

    if (cache.find(product->deviceVariant) == cache.end()) {
        cache[product->deviceVariant] = {
            {0, {"Flip frequencies", swapCommand()}},
            {1, {"VHF 1", std::string("AirbusFBW/VHF1") + sideName()}},
            {2, {"VHF 2", std::string("AirbusFBW/VHF2") + sideName()}},
            {3, {"VHF 3", std::string("AirbusFBW/VHF3") + sideName()}},
            {4, {"LOAD", kTcasModeToggle}},
            {5, {"HF1", std::string("AirbusFBW/HF1") + sideName()}},
            {6, {"HF2", std::string("AirbusFBW/HF2") + sideName()}},
            {7, {"AM", std::string("AirbusFBW/AM") + sideName()}},
            {8, {"NAV", std::string("AirbusFBW/") + rmpName() + "/BackupNavPress"}},
            {9, {"VOR", std::string("AirbusFBW/") + rmpName() + "/BackupNavVORPress"}},
            {10, {"ILS", std::string("AirbusFBW/") + rmpName() + "/BackupNavILSPress"}},
            {11, {"GLS", std::string("AirbusFBW/") + rmpName() + "/BackupNavBFOPress"}},
            {12, {"MLS", kRmpIdent}},
            {13, {"ADF", std::string("AirbusFBW/") + rmpName() + "/BackupNavADFPress"}},
            {14, {"Knob Large L", std::string("AirbusFBW/") + rmpName() + "FreqDownLrg"}},
            {15, {"Knob Large R", std::string("AirbusFBW/") + rmpName() + "FreqUpLrg"}},
            {16, {"Knob Small L", std::string("AirbusFBW/") + rmpName() + "FreqDownSml"}},
            {17, {"Knob Small R", std::string("AirbusFBW/") + rmpName() + "FreqUpSml"}},
            {18, {"Knob depress", kKnobDepress}},
            {19, {"Switch On", std::string("AirbusFBW/") + rmpName() + "Switch", RMPDatarefType::SET_VALUE, 1}},
            {20, {"Switch Off", std::string("AirbusFBW/") + rmpName() + "Switch", RMPDatarefType::SET_VALUE, 0}},
        };
    }

    return cache[product->deviceVariant];
}

void TolissRMPProfile::buttonPressed(const RMPButtonDef *button, XPLMCommandPhase phase) {
    if (!button || button->dataref.empty() || phase == xplm_CommandContinue) {
        return;
    }

    if (button->dataref == kRmpIdent) {
        if (rmpTcasEnabled("toliss")) {
            rmpIdent(product, this, "sim/transponder/transponder_ident", phase);
        }
        return;
    }

    if (button->dataref == kTcasModeToggle) {
        if (phase == xplm_CommandBegin && (tcasMode || rmpTcasEnabled("toliss"))) {
            setTcasMode(!tcasMode);
        }
        return;
    }

    if (tcasMode) {
        if (phase != xplm_CommandBegin) {
            return;
        }

        if (button->name == "Flip frequencies") {
            tcasSwitchSelected = !tcasSwitchSelected;
            updateDisplays();
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
                setTcasSwitchPosition(std::clamp(tcasSwitchPosition() + delta, 0, 2));
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

void TolissRMPProfile::updateDisplays() {
    if (!product) {
        return;
    }

    std::string rmpAvailRef = std::string("AirbusFBW/") + rmpName() + "Available";
    bool available = Dataref::getInstance()->getCached<int>(rmpAvailRef.c_str()) != 0;
    if (!available) {
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

    std::string activeHz;
    std::string stbyHz;

    // The window strings can register after this profile was built.
    if (useFrequencyFallback && Dataref::getInstance()->exists((std::string("AirbusFBW/") + rmpName() + "/ActiveWindowString").c_str())) {
        useFrequencyFallback = false;
    }

    if (useFrequencyFallback) {
        std::string activeFreqRef = std::string("AirbusFBW/") + rmpName() + "Freq";
        std::string stbyFreqRef = std::string("AirbusFBW/") + rmpName() + "StbyFreq";
        activeHz = formatFrequency(Dataref::getInstance()->getCached<float>(activeFreqRef.c_str()));
        stbyHz = formatFrequency(Dataref::getInstance()->getCached<float>(stbyFreqRef.c_str()));
    } else {
        std::string activeRef = std::string("AirbusFBW/") + rmpName() + "/ActiveWindowString";
        std::string stbyRef = std::string("AirbusFBW/") + rmpName() + "/StandbyWindowString";
        activeHz = Dataref::getInstance()->getCached<std::string>(activeRef.c_str());
        stbyHz = Dataref::getInstance()->getCached<std::string>(stbyRef.c_str());
    }

    product->setDisplayText(activeHz, stbyHz);
}

// Same source as the ToLiss TCAS panel profile: XPDRString where published,
// otherwise one dataref per digit, least significant first.
std::string TolissRMPProfile::squawkCode() const {
    auto datarefManager = Dataref::getInstance();
    if (datarefManager->exists("AirbusFBW/XPDRString")) {
        return datarefManager->getCached<std::string>("AirbusFBW/XPDRString");
    }

    std::string code;
    for (int digit = 4; digit >= 1; --digit) {
        const std::string name = "AirbusFBW/XPDR" + std::to_string(digit);
        const int value = datarefManager->getCached<int>(name.c_str());

        code += (value >= 0 && value <= 9) ? static_cast<char>('0' + value) : ' ';
    }

    return code;
}

// The A340-600 and A330-900 drive the switch through XPDRTCASMode 0-2, the
// A319/A320/A321 through XPDRPower, where STBY, TA and TA/RA are 0, 3 and 4.
static bool usesTcasModeDataref() {
    std::string icao = Dataref::getInstance()->getCached<std::string>("sim/aircraft/view/acf_ICAO");
    return icao == "A339" || icao == "A346";
}

int TolissRMPProfile::tcasSwitchPosition() const {
    if (usesTcasModeDataref()) {
        return std::clamp(Dataref::getInstance()->get<int>("AirbusFBW/XPDRTCASMode"), 0, 2);
    }

    switch (Dataref::getInstance()->get<int>("AirbusFBW/XPDRPower")) {
        case 3:
            return 1;
        case 4:
            return 2;
        default:
            return 0;
    }
}

void TolissRMPProfile::setTcasSwitchPosition(int position) {
    if (usesTcasModeDataref()) {
        Dataref::getInstance()->set<int>("AirbusFBW/XPDRTCASMode", position);
        // The ATC mode switch follows: STBY with TCAS STBY, AUTO with TA or TA/RA.
        Dataref::getInstance()->set<int>("AirbusFBW/XPDRPower", position == 0 ? 0 : 1);
    } else {
        static constexpr int powerValues[] = {0, 3, 4};
        Dataref::getInstance()->set<int>("AirbusFBW/XPDRPower", powerValues[position]);
    }

    updateDisplays();
}

void TolissRMPProfile::setTcasMode(bool enabled) {
    if (!enabled && squawkEditing) {
        commitSquawk();
    }

    tcasMode = enabled;
    tcasSwitchSelected = false;
    squawkLowerDigits = false;
    product->setLedBrightness(RMPLed::LOAD, enabled ? 255 : 0);
    updateDisplays();
}

void TolissRMPProfile::stepSquawk(bool large, int delta) {
    if (!squawkEditing) {
        // Start from the code currently set; a blank or partial entry starts at 0000.
        std::string code = squawkCode();
        for (int i = 0; i < 4; ++i) {
            squawkDigits[i] = (code.length() == 4 && code[i] >= '0' && code[i] <= '7') ? code[i] - '0' : 0;
        }
        squawkEditing = true;

        // Four presses empty the entry whether CLR removes one digit or the whole code.
        for (int i = 0; i < 4; ++i) {
            Dataref::getInstance()->executeCommand("AirbusFBW/ATCCodeKeyCLR");
        }
    }

    // Large knob: thousands (tens after a knob press). Small knob: hundreds (units).
    int index = (squawkLowerDigits ? 2 : 0) + (large ? 0 : 1);
    squawkDigits[index] = (squawkDigits[index] + delta + 8) % 8;

    AppState::getInstance()->executeAfterDebounced(kSquawkCommitTask, kSquawkCommitDelayMs, this, [this]() {
        commitSquawk();
    });

    updateDisplays();
}

void TolissRMPProfile::commitSquawk() {
    if (!squawkEditing) {
        return;
    }
    squawkEditing = false;

    for (int digit : squawkDigits) {
        std::string command = "AirbusFBW/ATCCodeKey" + std::to_string(digit);
        Dataref::getInstance()->executeCommand(command.c_str());
    }

    updateDisplays();
}
