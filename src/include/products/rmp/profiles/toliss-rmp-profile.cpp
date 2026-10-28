#include "toliss-rmp-profile.h"

#include "appstate.h"
#include "dataref.h"
#include "product-rmp.h"
#include "xplane-version.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {
    constexpr const char *kTcasModeToggle = "winctrl/rmp/tcas_mode_toggle";
    constexpr const char *kSquawkCommitTask = "toliss-rmp-squawk-commit";
    constexpr int kSquawkCommitDelayMs = 5000;
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
            {12, {"MLS", ""}},
            {13, {"ADF", std::string("AirbusFBW/") + rmpName() + "/BackupNavADFPress"}},
            {14, {"Knob Large L", std::string("AirbusFBW/") + rmpName() + "FreqDownLrg"}},
            {15, {"Knob Large R", std::string("AirbusFBW/") + rmpName() + "FreqUpLrg"}},
            {16, {"Knob Small L", std::string("AirbusFBW/") + rmpName() + "FreqDownSml"}},
            {17, {"Knob Small R", std::string("AirbusFBW/") + rmpName() + "FreqUpSml"}},
            {18, {"Knob depress", ""}},
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

    if (button->dataref == kTcasModeToggle) {
        if (phase == xplm_CommandBegin) {
            setTcasMode(!tcasMode);
        }
        return;
    }

    if (tcasMode && button->name.starts_with("Knob ")) {
        if (phase != xplm_CommandBegin) {
            return;
        }

        if (button->name == "Knob Large L") {
            stepSquawk(true, -1);
        } else if (button->name == "Knob Large R") {
            stepSquawk(true, 1);
        } else if (button->name == "Knob Small L") {
            stepSquawk(false, -1);
        } else if (button->name == "Knob Small R") {
            stepSquawk(false, 1);
        }
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
        if (squawkEditing) {
            char code[8];
            snprintf(code, sizeof(code), "%d%d%d%d", squawkHigh / 8, squawkHigh % 8, squawkLow / 8, squawkLow % 8);
            product->setDisplayText("TCAS", code);
        } else {
            product->setDisplayText("TCAS", squawkCode());
        }
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

void TolissRMPProfile::setTcasMode(bool enabled) {
    if (!enabled && squawkEditing) {
        commitSquawk();
    }

    tcasMode = enabled;
    product->setLedBrightness(RMPLed::LOAD, enabled ? 255 : 0);
    updateDisplays();
}

void TolissRMPProfile::stepSquawk(bool large, int delta) {
    if (!squawkEditing) {
        // Start from the code currently set; a blank or partial entry starts at 0000.
        std::string code = squawkCode();
        int digits[4] = {0, 0, 0, 0};
        if (code.length() == 4) {
            for (int i = 0; i < 4; ++i) {
                digits[i] = (code[i] >= '0' && code[i] <= '7') ? code[i] - '0' : 0;
            }
        }
        squawkHigh = digits[0] * 8 + digits[1];
        squawkLow = digits[2] * 8 + digits[3];
        squawkEditing = true;

        // Four presses empty the entry whether CLR removes one digit or the whole code.
        for (int i = 0; i < 4; ++i) {
            Dataref::getInstance()->executeCommand("AirbusFBW/ATCCodeKeyCLR");
        }
    }

    // Two octal digits per knob, 00-77, wrapping around.
    int &pair = large ? squawkHigh : squawkLow;
    pair = (pair + delta + 64) % 64;

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

    const int digits[4] = {squawkHigh / 8, squawkHigh % 8, squawkLow / 8, squawkLow % 8};
    for (int digit : digits) {
        std::string command = "AirbusFBW/ATCCodeKey" + std::to_string(digit);
        Dataref::getInstance()->executeCommand(command.c_str());
    }

    updateDisplays();
}
