#ifndef TOLISS_RMP_PROFILE_H
#define TOLISS_RMP_PROFILE_H

#include "rmp-aircraft-profile.h"
#include "rmp-tcas-config.h"

#include <string>
#include <vector>

class TolissRMPProfile : public RMPAircraftProfile {
    private:
        std::vector<std::string> _displayDatarefs;
        bool useFrequencyFallback = false;
        const char *rmpName() const;
        const char *sideName() const;
        const char *swapCommand() const;
        std::string formatFrequency(float megahertz) const;

        // LOAD (or the key picked in the installer) toggles a transponder mode: the left window shows TCAS, the right
        // one the squawk. The large knob steps the thousands, the small knob the
        // hundreds; a knob press switches them to tens and units. The code is keyed
        // into the ATC panel after 3 s idle. A short press of the swap key shows the TCAS mode switch
        // (STBY, TA, TA/RA) on the right window, stepped by either knob without wrapping.
        // Holding the swap key for 3 s in this mode sends the transponder IDENT.
        RMPTcasSettings tcasSettings;
        bool tcasMode = false;
        bool tcasSwitchSelected = false;
        RMPHoldButton swapHold;
        bool identShown = false; // IDENT on the right window for 3 s after sending it
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
        TolissRMPProfile(ProductRMP *product);

        static bool IsEligible();
        const std::vector<std::string> &displayDatarefs() const override;
        const std::unordered_map<uint16_t, RMPButtonDef> &buttonDefs() const override;

        void buttonPressed(const RMPButtonDef *button, XPLMCommandPhase phase) override;

        void updateDisplays() override;
};

#endif
