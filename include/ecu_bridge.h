#ifndef ATG_ENGINE_SIM_ECU_BRIDGE_H
#define ATG_ENGINE_SIM_ECU_BRIDGE_H

#include "angees_bridge_proto.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <netinet/in.h>

class Simulator;

// Publishes engine state (RPM, crank angle, throttle, MAP, exhaust O2,
// dyno state) to an external ECU simulator over localhost UDP, once per
// rendered frame. Fire-and-forget: no listener required, send errors ignored.
//
// Also receives remote plant controls (ignition/starter/throttle/dyno) on a
// second, non-blocking socket. The application applies them only while
// controlsFresh() — when packets stop, keyboard control resumes.
//
// Closed-loop ignition (M4a) is a third channel with different timing needs:
// spark events must be applied at sub-frame, sub-degree crank-angle
// resolution, so they can't be polled once per rendered frame like the
// others. A dedicated background thread drains the spark socket continuously
// into a small queue; popSparkEvent() is meant to be called once per physics
// sub-step (via IgnitionModule's external-ignition source), not once per frame.
class EcuBridge {
public:
    void initialize();
    void publish(Simulator *simulator);
    void pollControls();
    bool controlsFresh() const;
    const AngeesControlV1 &controls() const { return m_ctrl; }

    bool popSparkEvent(int &cylinderIndexOut);
    bool sparkLinkFresh() const;

    void destroy();

private:
    int m_fd = -1;
    sockaddr_in m_dest{};
    uint16_t m_seq = 0;

    int m_ctrlFd = -1;
    AngeesControlV1 m_ctrl{};
    std::chrono::steady_clock::time_point m_ctrlRx{};
    bool m_ctrlEverRx = false;

    int m_sparkFd = -1;
    std::mutex m_sparkMutex;
    std::deque<int> m_sparkQueue;
    std::atomic<long long> m_sparkLastRxMs{0};

    void sparkRxLoop();
};

#endif /* ATG_ENGINE_SIM_ECU_BRIDGE_H */
