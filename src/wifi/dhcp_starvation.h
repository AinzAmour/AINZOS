#ifndef DHCP_STARVATION_H
#define DHCP_STARVATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file dhcp_starvation.h
 * @brief DHCP starvation attack interface
 * 
 * DHCP starvation attack floods a DHCP server with discover requests
 * to exhaust its IP address pool.
 */

/**
 * @brief Start DHCP starvation attack
 * @param threads Number of threads (currently ignored, single-threaded)
 */
void dhcp_starvation_start(int threads);

/**
 * @brief Stop DHCP starvation attack
 */
void dhcp_starvation_stop(void);

/**
 * @brief Display current attack statistics
 */
void dhcp_starvation_display(void);

/**
 * @brief Display help information
 */
void dhcp_starvation_help(void);

/**
 * @brief Check if DHCP starvation attack is currently running
 */
bool dhcp_starvation_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // DHCP_STARVATION_H
