#ifndef ATG_ENGINE_SIM_ANGEES_BRIDGE_PROTO_H
#define ATG_ENGINE_SIM_ANGEES_BRIDGE_PROTO_H

// UDP packet spec for the AngeES -> epicEFI simulator bridge.
//
// TWIN COPIES (keep byte-identical; bump ANGEES_PROTO_V on ANY change):
//   AngeES_macos/include/angees_bridge_proto.h        (publisher)
//   simulator/simulator/angees_bridge_proto.h         (receiver)

#include <cstdint>

static constexpr uint32_t ANGEES_MAGIC   = 0x53454541; // "AEES" little-endian
static constexpr uint8_t  ANGEES_PROTO_V = 1;
static constexpr uint16_t ANGEES_PORT    = 29010;

#pragma pack(push, 1)
struct AngeesStateV1 {
    uint32_t magic;          // ANGEES_MAGIC
    uint8_t  version;        // ANGEES_PROTO_V
    uint8_t  flags;          // bit0 dynoEnabled, bit1 dynoHold
    uint16_t seq;            // wrapping sequence number, for drop counting
    float    rpm;            // engine speed [RPM]
    float    crankAngleDeg;  // engine cycle angle [deg 0..720]
    float    throttle;       // throttle position [0..1]
    float    mapKpa;         // manifold absolute pressure [kPa]
    float    exhaustO2;      // exhaust O2 fraction
    float    intakeAfr;      // intake air/fuel ratio
    float    dynoHoldRpm;    // dyno hold target [RPM]
    float    dynoTorqueNm;   // filtered dyno torque [Nm]
    float    dynoPowerKw;    // dyno power [kW]
};
#pragma pack(pop)

static_assert(sizeof(AngeesStateV1) == 44, "AngeesStateV1 must stay 44 bytes");

static constexpr uint8_t ANGEES_FLAG_DYNO_ENABLED = 1 << 0;
static constexpr uint8_t ANGEES_FLAG_DYNO_HOLD    = 1 << 1;

// Reverse channel: ECU GUI -> AngeES plant controls. Absolute state, re-sent
// every GUI frame while remote control is enabled; the plant applies it only
// while packets are fresh (<500 ms) and reverts to keyboard control when they
// stop. No toggles or pulses so lost/duplicated datagrams are harmless.
static constexpr uint32_t ANGEES_CTRL_MAGIC = 0x43434541; // "AECC" little-endian
static constexpr uint8_t  ANGEES_CTRL_V     = 1;
static constexpr uint16_t ANGEES_CTRL_PORT  = 29011;

static constexpr uint8_t ANGEES_CTRL_IGNITION     = 1 << 0;
static constexpr uint8_t ANGEES_CTRL_STARTER      = 1 << 1;
static constexpr uint8_t ANGEES_CTRL_DYNO_ENABLED = 1 << 2;
static constexpr uint8_t ANGEES_CTRL_DYNO_HOLD    = 1 << 3;

#pragma pack(push, 1)
struct AngeesControlV1 {
    uint32_t magic;        // ANGEES_CTRL_MAGIC
    uint8_t  version;      // ANGEES_CTRL_V
    uint8_t  flags;        // ANGEES_CTRL_* bits
    uint16_t seq;          // wrapping sequence number
    float    throttle;     // throttle target [0..1]
    float    dynoHoldRpm;  // dyno hold target [RPM]; plant clamps to its range
};
#pragma pack(pop)

static_assert(sizeof(AngeesControlV1) == 16, "AngeesControlV1 must stay 16 bytes");

#endif /* ATG_ENGINE_SIM_ANGEES_BRIDGE_PROTO_H */
