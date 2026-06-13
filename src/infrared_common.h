#ifndef INFRARED_COMMON_H
#define INFRARED_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "infrared_timings.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct InfraredCommonEncoder InfraredCommonEncoder;
typedef struct InfraredCommonProtocolSpec InfraredCommonProtocolSpec;
typedef struct InfraredMessage InfraredMessage;

typedef struct {
    char name[32];
    bool is_raw;
    union {
        struct {
            uint32_t *timings;
            size_t timings_size;
            uint32_t frequency;
            float duty_cycle;
        } raw;
        struct {
            char protocol[32];
            uint32_t address;
            uint32_t command;
        } message;
    } payload;
} infrared_signal_t;

typedef enum {
    InfraredStatusOk,
    InfraredStatusDone,
    InfraredStatusError,
} InfraredStatus;

typedef enum {
    InfraredCommonEncoderStateSilence,
    InfraredCommonEncoderStatePreamble,
    InfraredCommonEncoderStateEncode,
    InfraredCommonEncoderStateEncodeRepeat,
} InfraredCommonEncoderState;

// Encoder function types
typedef InfraredStatus (*InfraredEncoderEncode)(InfraredCommonEncoder* encoder, uint32_t* duration, bool* level);
typedef InfraredStatus (*InfraredEncoderEncodeRepeat)(InfraredCommonEncoder* encoder, uint32_t* duration, bool* level);
typedef void (*InfraredEncoderReset)(InfraredCommonEncoder* encoder, const InfraredMessage* message);

typedef struct {
    uint32_t preamble_mark;
    uint32_t preamble_space;
    uint32_t bit1_mark;
    uint32_t bit1_space;
    uint32_t bit0_mark;
    uint32_t bit0_space;
    uint32_t silence_time;
    uint32_t repeat_mark;
    uint32_t repeat_space;
    uint32_t preamble_tolerance;
    uint32_t bit_tolerance;
    uint32_t min_split_time;
} InfraredTimings;

struct InfraredCommonProtocolSpec {
    const InfraredTimings timings;
    bool manchester_start_from_space;
    const uint8_t databit_len[4];
    InfraredEncoderReset reset;
    InfraredEncoderEncode encode;
    InfraredEncoderEncodeRepeat encode_repeat;
    uint32_t carrier_frequency;
    float duty_cycle;
};

struct InfraredCommonEncoder {
    const InfraredCommonProtocolSpec* protocol;
    InfraredCommonEncoderState state;
    uint32_t timings_encoded;
    uint64_t timings_sum;
    uint8_t bits_to_encode;
    uint8_t bits_encoded;
    uint8_t data[];
};

struct InfraredMessage {
    char protocol[32];
    uint32_t address;
    uint32_t command;
};

// Encoder function declarations
void* infrared_common_encoder_alloc(const InfraredCommonProtocolSpec* protocol);
void infrared_common_encoder_free(InfraredCommonEncoder* encoder);
void infrared_common_encoder_reset(InfraredCommonEncoder* encoder);
InfraredStatus infrared_common_encode(InfraredCommonEncoder* encoder, uint32_t* duration, bool* level);
InfraredStatus infrared_common_encode_manchester(InfraredCommonEncoder* encoder, uint32_t* duration, bool* level);
InfraredStatus infrared_common_encode_pdwm(InfraredCommonEncoder* encoder, uint32_t* duration, bool* level);

#ifdef __cplusplus
}
#endif

#endif // INFRARED_COMMON_H
