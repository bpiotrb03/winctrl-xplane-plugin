#ifndef TOLISS_RMP_PROFILE_H
#define TOLISS_RMP_PROFILE_H

#include "rmp-aircraft-profile.h"

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

        // LOAD toggles a transponder mode: the left window shows TCAS, the right
        // one the squawk. The large knob steps the first two octal digits, the small
        // knob the last two. The code is keyed into the ATC panel after 5 s idle.
        bool tcasMode = false;
        bool squawkEditing = false;
        int squawkHigh = 0;
        int squawkLow = 0;
        std::string squawkCode() const;
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
