#ifndef FF777_RMP_PROFILE_H
#define FF777_RMP_PROFILE_H

#include "product-rmp.h"
#include "rmp-aircraft-profile.h"

#include <string>
#include <vector>

class FF777RMPProfile : public RMPAircraftProfile {
    private:
        std::vector<std::string> _displayDatarefs;
        std::unordered_map<RMPLed, std::string> _ledDatarefs;

        const char *rmpName() const;
        const char *sideName() const;
        const char *swapCommand() const;

        // LOAD toggles a transponder mode: the left window shows TCAS, the right
        // one the squawk. The large knob steps the thousands, the small knob the
        // hundreds; a knob press switches them to tens and units. The code is dialled
        // into the ATC panel's digit knobs after 3 s idle. The swap key shows the
        // transponder mode switch on the right window, stepped without wrapping.
        bool tcasMode = false;
        bool tcasSwitchSelected = false;
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
        FF777RMPProfile(ProductRMP *product);

        static bool IsEligible();
        const std::vector<std::string> &displayDatarefs() const override;
        const std::unordered_map<uint16_t, RMPButtonDef> &buttonDefs() const override;

        void buttonPressed(const RMPButtonDef *button, XPLMCommandPhase phase) override;

        void updateDisplays() override;
};

#endif
