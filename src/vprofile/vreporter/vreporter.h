#ifndef __VREPORTER_H__
#define __VREPORTER_H__

#include "drcctlib.h"
#include "vlogger/vlogger.h"
/* Low-level VReporter Interfaces for VProfile Framework */

enum {
  TEXT_REPORT = 0,
  JSON_REPORT
};

enum {
    VREPORTER_NATIVE=0x00,
    VREPORTER_APPEND_PID=0x01,
    VREPORTER_ASYNC=0x02
};

#define VREPORTER_DEFAULT (VREPORTER_APPEND_PID)

class VReporter {
public:
  VReporter(const char *base);
  ~VReporter();

  void section_enter(const char *name);
  void section_exit(const char *name);

  void record_enter(const char *name);
  void record_exit(const char *name);

  void metric(const char *name, const char *metric);
  void metric(const char *name, uint64_t local, uint64_t global);
  void metric(const char *name, uint64_t val);
  void metric(const char *name, float val);
  void metric(const char *name, double val);
  void calling_context(context_handle_t ctxt_hndl);
  void data_object(int32_t data_obj);

  void message(const char *msg);

private:
};

// Generate text report with dr_fprintf
class VReporterText : public VReporter;

// Generate Result with Format for VProfile In-Editor Visualizer (.json)
class VReporterJSON : public VReporter{};

typedef VReporter vreporter_t;

bool vreporter_init(const char *dir, int flags);
void vreporter_exit();

vreporter_t *vreporter_create(const char *base, int type);
void vreporter_destroy(vreporter_t *vreporter);

template <typename key_t, typename data_t>
void vreporter_register_section(vreporter_t *vreporter, const char *name,
                                logger_t logger,
                                void (*record_handler)(vreporter_t *, key_t &,
                                                       data_t &, data_t &),
                                bool sorted, int topN, bool merge);

#endif