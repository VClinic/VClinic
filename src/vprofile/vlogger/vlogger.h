#ifndef __VLOGGER_H__
#define __VLOGGER_H__

/* Low-level VLogger Interfaces for VProfile Framework */

typedef int logger_t;

namespace VLogger {
    template<typename key_t, typename data_t>
    logger_t register_logger();
    void unregister_logger(int logger);

    void* getLoggerTLS();

    // logger_tls->logger[key] += data
    template<typename key_t, typename data_t>
    void accumulate(void* logger_tls, logger_t logger, key_t& key, data_t& data);

    // logger_tls->logger[key]++
    template<typename key_t>
    void increament(void* logger_tls, logger_t logger, key_t& key);
}



bool vlogger_init(void);
void vlogger_exit(void);

#endif