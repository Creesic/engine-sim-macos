#ifndef ATG_ENGINE_SIM_ECU_BRIDGE_H
#define ATG_ENGINE_SIM_ECU_BRIDGE_H

#include <cstdint>
#include <netinet/in.h>

class Simulator;

// Publishes engine state (RPM, crank angle, throttle, MAP, exhaust O2,
// dyno state) to an external ECU simulator over localhost UDP, once per
// rendered frame. Fire-and-forget: no listener required, send errors ignored.
class EcuBridge {
public:
    void initialize();
    void publish(Simulator *simulator);
    void destroy();

private:
    int m_fd = -1;
    sockaddr_in m_dest{};
    uint16_t m_seq = 0;
};

#endif /* ATG_ENGINE_SIM_ECU_BRIDGE_H */
