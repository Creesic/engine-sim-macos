#include "../include/ecu_bridge.h"

#include "../include/angees_bridge_proto.h"
#include "../include/simulator.h"
#include "../include/engine.h"
#include "../include/crankshaft.h"
#include "../include/units.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

void EcuBridge::initialize() {
    m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_fd < 0) return;

    m_dest.sin_family = AF_INET;
    m_dest.sin_port = htons(ANGEES_PORT);
    m_dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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
}
