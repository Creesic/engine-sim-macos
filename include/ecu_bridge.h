#ifndef ATG_ENGINE_SIM_ECU_BRIDGE_H
#define ATG_ENGINE_SIM_ECU_BRIDGE_H

#include "angees_bridge_proto.h"

#include <chrono>
#include <cstdint>
#include <netinet/in.h>

class Simulator;

// Publishes engine state (RPM, crank angle, throttle, MAP, exhaust O2,
// dyno state) to an external ECU simulator over localhost UDP, once per
// rendered frame. Fire-and-forget: no listener required, send errors ignored.
//
// Also receives remote plant controls (ignition/starter/throttle/dyno) on a
// second, non-blocking socket. The application applies them only while
// controlsFresh() — when packets stop, keyboard control resumes.
class EcuBridge {
public:
    void initialize();
    void publish(Simulator *simulator);
    void pollControls();
    bool controlsFresh() const;
    const AngeesControlV1 &controls() const { return m_ctrl; }
    void destroy();

private:
    int m_fd = -1;
    sockaddr_in m_dest{};
    uint16_t m_seq = 0;

    int m_ctrlFd = -1;
    AngeesControlV1 m_ctrl{};
    std::chrono::steady_clock::time_point m_ctrlRx{};
    bool m_ctrlEverRx = false;
};

#endif /* ATG_ENGINE_SIM_ECU_BRIDGE_H */
