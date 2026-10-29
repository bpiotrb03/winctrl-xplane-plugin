#ifndef ZIBO_RMP_PROFILE_H
#define ZIBO_RMP_PROFILE_H

#include "rmp-aircraft-profile.h"
#include "rmp-tcas-config.h"

#include <string>
#include <vector>

class ZiboRMPProfile : public RMPAircraftProfile {
    private:
        std::vector<std::string> _displayDatarefs;
        const char *rtpName() const;

        // LOAD toggles a transponder mode: the left window shows TCAS, the right
        // one the squawk. The large knob steps the thousands, the small knob the
        // hundreds; a knob press switches them to tens and units. The code is set
        // on the transponder after 3 s idle. A short press of the swap key shows the transponder
        // mode switch on the right window, stepped without wrapping.
        // Holding the swap key for 3 s in this mode sends the transponder IDENT.
        bool tcasMode = false;
        bool tcasSwitchSelected = false;
        RMPHoldButton swapHold;
        bool squawkEditing = false;
        bool squawkLowerDigits = false;
        int squawkDigits[4] = {0, 0, 0, 0};
        std::string squawkCode() const;
        int tcasSwitchPosition() const;
        void setTcasSwitchPosition(int position);
        void setTcasMode(bool enabled);
        void stepSquawk(bool large, int delta);
        void commitSquawk();

    public:
        ZiboRMPProfile(ProductRMP *product);

        static bool IsEligible();
        const std::vector<std::string> &displayDatarefs() const override;
        const std::unordered_map<uint16_t, RMPButtonDef> &buttonDefs() const override;

        void buttonPressed(const RMPButtonDef *button, XPLMCommandPhase phase) override;

        void updateDisplays() override;
};

#endif
