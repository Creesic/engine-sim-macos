#include "../include/ecu_bridge.h"

#include "../include/angees_bridge_proto.h"
#include "../include/simulator.h"
#include "../include/engine.h"
#include "../include/crankshaft.h"
#include "../include/units.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
constexpr size_t kMaxQueuedSparkEvents = 64;

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

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

    m_sparkFd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sparkFd >= 0) {
        int reuse = 1;
        ::setsockopt(m_sparkFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(ANGEES_SPARK_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(m_sparkFd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) < 0) {
            ::close(m_sparkFd);
            m_sparkFd = -1;
        }
        else {
            // Blocking, on its own detached thread - unlike pollControls()
            // this can't wait for a once-per-frame poll; spark timing needs
            // continuous draining at sub-frame resolution.
            std::thread([this] { sparkRxLoop(); }).detach();
        }
    }
}

void EcuBridge::sparkRxLoop() {
    for (;;) {
        AngeesSparkEventV1 pkt;
        const ssize_t n = ::recvfrom(m_sparkFd, &pkt, sizeof(pkt), 0, nullptr, nullptr);
        if (n < 0) {
            if (errno == EINTR) continue;
            break; // socket closed / unrecoverable
        }
        if (n != sizeof(pkt) || pkt.magic != ANGEES_SPARK_MAGIC || pkt.version != ANGEES_SPARK_V)
            continue;

        m_sparkLastRxMs.store(nowMs());

        std::lock_guard<std::mutex> lock(m_sparkMutex);
        if (m_sparkQueue.size() >= kMaxQueuedSparkEvents) {
            m_sparkQueue.pop_front(); // drop oldest on overflow
        }
        m_sparkQueue.push_back(pkt.slot);
    }
}

bool EcuBridge::popSparkEvent(int &cylinderIndexOut) {
    std::lock_guard<std::mutex> lock(m_sparkMutex);
    if (m_sparkQueue.empty()) return false;
    cylinderIndexOut = m_sparkQueue.front();
    m_sparkQueue.pop_front();
    return true;
}

bool EcuBridge::sparkLinkFresh() const {
    const long long last = m_sparkLastRxMs.load();
    return last != 0 && (nowMs() - last) < 500;
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
    if (m_sparkFd >= 0) {
        ::close(m_sparkFd);
        m_sparkFd = -1;
    }
}
