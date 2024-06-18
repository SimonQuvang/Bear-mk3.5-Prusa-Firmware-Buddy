#pragma once

#include <logging/log.hpp>

namespace logging {

/// Initialize the Syslog handler
void syslog_initialize();

/// Format an event according to RFC5424
///
/// https://tools.ietf.org/html/rfc5424
void syslog_format_event(Event *event, void (*out_fn)(char character, void *arg), void *arg);

/// Log an event using Syslog
///
/// The SYSLOG destination does not guarantee delivery of all events.
/// Events from ISRs are not being delivered, syslog won't try to send an event if running low on stack,
/// the UDP packet can be lost, etc.
void syslog_log_event(FormattedEvent *event);

/**
 * @brief Configure syslog handler address and port.
 * @param ip IP address or short hostname
 * @param port port number
 */
void syslog_configure(const char *ip, uint16_t port);

/**
 * @brief Get address of the metrics syslog host.
 * @return IP address or short hostname
 */
const char *syslog_get_host();

/**
 * @brief Get port of the metrics syslog host.
 * @return port number
 */
uint16_t syslog_get_port();

} // namespace logging
