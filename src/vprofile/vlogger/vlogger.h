#ifndef __VLOGGER_H__
#define __VLOGGER_H__

/* Low-level VLogger Interfaces for VProfile Framework */
namespace VLogger {
    template<typename key_t, typename data_t>
    class MetricLogs {
        public:
        MetricLogs();
        MetricLogs(bool thread_merge);

        void increament(key_t key);

        private:

    };
}

bool vlogger_init(void);
void vlogger_exit(void);

#endif