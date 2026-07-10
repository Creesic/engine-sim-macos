#include "../include/ecu_bridge.h"

#include "../include/angees_bridge_proto.h"
#include "../include/simulator.h"
#include "../include/engine.h"
#include "../include/crankshaft.h"
#include "../include/units.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

void EcuBridge::initialize() {
    m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_fd >= 0) {
        m_dest.sin_family = AF_INET;
        m_dest.sin_port = htons(ANGEES_PORT);
        m_dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    m_ctrlFd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_ctrlFd >= 0) {
        int reuse = 1;
        ::setsockopt(m_ctrlFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(ANGEES_CTRL_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(m_ctrlFd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) < 0) {
            ::close(m_ctrlFd);
            m_ctrlFd = -1;
        }
        else {
            ::fcntl(m_ctrlFd, F_SETFL, O_NONBLOCK);
        }
    }
}

void EcuBridge::pollControls() {
    if (m_ctrlFd < 0) return;

    // drain everything pending; keep the newest valid packet
    for (;;) {
        AngeesControlV1 pkt;
        const ssize_t n = ::recvfrom(m_ctrlFd, &pkt, sizeof(pkt), 0, nullptr, nullptr);
        if (n < 0) break; // EAGAIN/EWOULDBLOCK: nothing left
        if (n != sizeof(pkt) || pkt.magic != ANGEES_CTRL_MAGIC || pkt.version != ANGEES_CTRL_V)
            continue;

        m_ctrl = pkt;
        m_ctrlRx = std::chrono::steady_clock::now();
        m_ctrlEverRx = true;
    }
}

bool EcuBridge::controlsFresh() const {
    if (!m_ctrlEverRx) return false;
    const auto age = std::chrono::steady_clock::now() - m_ctrlRx;
    return age < std::chrono::milliseconds(500);
}

void EcuBridge::publish(Simulator *simulator) {
    if (m_fd < 0 || simulator == nullptr) return;

    Engine *engine = simulator->getEngine();
    if (engine == nullptr) return;

    AngeesStateV1 pkt{};
    pkt.magic = ANGEES_MAGIC;
    pkt.version = ANGEES_PROTO_V;
    pkt.seq = m_seq++;

    pkt.rpm = static_cast<float>(engine->getRpm());
    pkt.crankAngleDeg = static_cast<float>(
        units::convert(engine->getOutputCrankshaft()->getCycleAngle(), units::deg));
    pkt.throttle = static_cast<float>(engine->getThrottle());
    pkt.mapKpa = static_cast<float>(
        units::convert(engine->getManifoldPressure(), units::kPa));
    pkt.exhaustO2 = static_cast<float>(engine->getExhaustO2());
    pkt.intakeAfr = static_cast<float>(engine->getIntakeAfr());

    if (simulator->m_dyno.m_enabled) pkt.flags |= ANGEES_FLAG_DYNO_ENABLED;
    if (simulator->m_dyno.m_hold) pkt.flags |= ANGEES_FLAG_DYNO_HOLD;
    pkt.dynoHoldRpm = static_cast<float>(units::toRpm(simulator->m_dyno.m_rotationSpeed));
    pkt.dynoTorqueNm = static_cast<float>(
        units::convert(simulator->getFilteredDynoTorque(), units::Nm));
    pkt.dynoPowerKw = static_cast<float>(
        units::convert(simulator->getDynoPower(), units::kW));

    ::sendto(m_fd, &pkt, sizeof(pkt), 0,
        reinterpret_cast<const sockaddr *>(&m_dest), sizeof(m_dest));
}

void EcuBridge::destroy() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    if (m_ctrlFd >= 0) {
        ::close(m_ctrlFd);
        m_ctrlFd = -1;
    }
}
