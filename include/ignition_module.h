#ifndef ATG_ENGINE_SIM_IGNITION_MODULE_H
#define ATG_ENGINE_SIM_IGNITION_MODULE_H

#include "part.h"

#include "crankshaft.h"
#include "function.h"
#include "units.h"

#include <functional>

class IgnitionModule : public Part {
    public:
        struct Parameters {
            int cylinderCount;
            Crankshaft *crankshaft;
            Function *timingCurve;
            double revLimit = units::rpm(6000.0);
            double limiterDuration = 0.5 * units::sec;
        };

        struct SparkPlug {
            double angle = 0;
            bool ignitionEvent = false;
            bool enabled = false;
        };

    public:
        IgnitionModule();
        virtual ~IgnitionModule();

        virtual void destroy();

        void initialize(const Parameters &params);
        void setFiringOrder(int cylinderIndex, double angle);
        void reset();
        void update(double dt);

        bool getIgnitionEvent(int index) const;
        void resetIgnitionEvents();

        double getTimingAdvance();

        // Closed-loop ignition (M4a): when enabled, the normal advance-curve
        // firing below is skipped entirely for all cylinders, and firing is
        // driven solely by whatever the external source provides - each call
        // should return true and set cylinderIndexOut once per pending event,
        // false once drained. Called every physics sub-step from update(),
        // so timing fidelity is whatever the source itself provides (the ECU
        // bridge forwards firmware spark events essentially in real time).
        using ExternalIgnitionSource = std::function<bool(int &cylinderIndexOut)>;
        void setExternalIgnitionSource(ExternalIgnitionSource source) { m_externalSource = source; }
        void setExternalIgnitionEnabled(bool enabled) { m_externalControl = enabled; }

        bool m_enabled;

    protected:
        SparkPlug *getPlug(int i);

        Function *m_timingCurve;
        SparkPlug *m_plugs;
        Crankshaft *m_crankshaft;
        int m_cylinderCount;

        double m_lastCrankshaftAngle;
        double m_revLimit;
        double m_revLimitTimer;
        double m_limiterDuration;

        bool m_externalControl = false;
        ExternalIgnitionSource m_externalSource;
};

#endif /* ATG_ENGINE_SIM_IGNITION_MODULE_H */
