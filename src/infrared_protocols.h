#ifndef INFRARED_PROTOCOLS_H
#define INFRARED_PROTOCOLS_H

#include "infrared_common.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const InfraredCommonProtocolSpec infrared_protocol_nec;
extern const InfraredCommonProtocolSpec infrared_protocol_kaseikyo;
extern const InfraredCommonProtocolSpec infrared_protocol_pioneer;
extern const InfraredCommonProtocolSpec infrared_protocol_rca;
extern const InfraredCommonProtocolSpec infrared_protocol_samsung;
extern const InfraredCommonProtocolSpec infrared_protocol_sirc;
extern const InfraredCommonProtocolSpec infrared_protocol_sirc15;
extern const InfraredCommonProtocolSpec infrared_protocol_sirc20;
extern const InfraredCommonProtocolSpec infrared_protocol_rc5;
extern const InfraredCommonProtocolSpec infrared_protocol_rc6;
extern const InfraredCommonProtocolSpec infrared_protocol_necext;

#ifdef __cplusplus
}
#endif

#endif // INFRARED_PROTOCOLS_H
