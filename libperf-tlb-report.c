#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>

#include "libperf-tlb-report.h"
#include "queue.h"

// The only good-ish resource on this topic I could find is the manpage for perf_event_open(): http://man7.org/linux/man-pages/man2/perf_event_open.2.html
// On using groups: https://stackoverflow.com/a/42092180/128240 butit doesn't seem to work well. In theory, we want our performance events to be grouped, so that they represent the same period of code execution. In reality, this seems to cause the events to be almost never actually run. Couldn't find anything online that'd explain this or propose a fix.

#if DEBUG
  #define LOG(...) fprintf(stderr, "[libperf-tlb-report] " __VA_ARGS__)
#else
  #define LOG(...) /* empty */
#endif

#define __CONCAT2(A, B) A##B
#define CONCAT2(A, B) __CONCAT2(A, B)

#define PRINT(...) fprintf(stderr, __VA_ARGS__)

typedef void (* perf_event_config_func)(struct perf_event_attr *this);

struct perf_event_desc {
  const char *name;
  int fd;
  uint64_t id;
  bool initialized;
  perf_event_config_func config_func;
  struct perf_event_attr attrs;

  LIST_ENTRY(perf_event_desc) list;
};

#if LIBPERF_TLB_REPORT_USE_GROUPS
  struct perf_event_data {
    uint64_t value;      /* The value of the event */
    uint64_t id;         /* if PERF_FORMAT_ID */
  };

  struct perf_event_group_data {
    uint64_t nr;            /* The number of events */
    uint64_t time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
    uint64_t time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
    struct perf_event_data values[];
  };
#else
  struct perf_event_data {
    uint64_t value;         /* The value of the event */
    uint64_t time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
    uint64_t time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
    uint64_t id;            /* if PERF_FORMAT_ID */
  };
#endif

static LIST_HEAD(, perf_event_desc) g_pe_list = LIST_HEAD_INITIALIZER(&g_pe_list);
static size_t g_pe_list_length; // all events
static size_t g_pe_count; // enabled events
static int g_group_leader_fd = -1;

static void perf_event_register(struct perf_event_desc *pe) {
  LOG("Registering performance event %s...\n", pe->name);
  pe->config_func(&pe->attrs);

  LIST_INSERT_HEAD(&g_pe_list, pe, list);
  g_pe_list_length++;
}

#if LIBPERF_TLB_REPORT_USE_GROUPS
  #define PE_READ_FORMAT PERF_FORMAT_GROUP | PERF_FORMAT_ID | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING
#else
  #define PE_READ_FORMAT PERF_FORMAT_ID | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING
#endif

#define DEFINE_PERF_COUNTER(NAME) \
  static void CONCAT2(_pe_configure_, NAME)(struct perf_event_attr *this); \
  \
  static struct perf_event_desc CONCAT2(g_pe_, NAME) = { \
    .name = #NAME, \
    .config_func = &CONCAT2(_pe_configure_, NAME), \
    .attrs = { \
      .size = sizeof(struct perf_event_attr), \
      .disabled = 1, \
      .exclude_kernel = 0, \
      .exclude_user = 0, \
      .exclude_hv = 0, \
      .read_format = PE_READ_FORMAT \
    } \
  }; \
  \
  __attribute__((constructor)) \
  void CONCAT2(_register_pe_, NAME)(void) { \
    perf_event_register(&CONCAT2(g_pe_, NAME)); \
  } \
  static void CONCAT2(_pe_configure_, NAME)(struct perf_event_attr *this)

DEFINE_PERF_COUNTER(tlb_data_read_access) {
  this->type = PERF_TYPE_HW_CACHE;
  this->config = PERF_COUNT_HW_CACHE_DTLB
                  | (PERF_COUNT_HW_CACHE_OP_READ << 8)
                  | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);
}

DEFINE_PERF_COUNTER(tlb_data_read_miss) {
  this->type = PERF_TYPE_HW_CACHE;
  this->config = PERF_COUNT_HW_CACHE_DTLB
                  | (PERF_COUNT_HW_CACHE_OP_READ << 8)
                  | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
}

DEFINE_PERF_COUNTER(tlb_data_write_access) {
  this->type = PERF_TYPE_HW_CACHE;
  this->config = PERF_COUNT_HW_CACHE_DTLB
                  | (PERF_COUNT_HW_CACHE_OP_WRITE << 8)
                  | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);
}

DEFINE_PERF_COUNTER(tlb_data_write_miss) {
  this->type = PERF_TYPE_HW_CACHE;
  this->config = PERF_COUNT_HW_CACHE_DTLB
                  | (PERF_COUNT_HW_CACHE_OP_WRITE << 8)
                  | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
}

DEFINE_PERF_COUNTER(tlb_data_prefetch_access) {
  this->type = PERF_TYPE_HW_CACHE;
  this->config = PERF_COUNT_HW_CACHE_DTLB
                  | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8)
                  | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);
}

DEFINE_PERF_COUNTER(tlb_data_prefetch_miss) {
  this->type = PERF_TYPE_HW_CACHE;
  this->config = PERF_COUNT_HW_CACHE_DTLB
                  | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8)
                  | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
}

static long perf_event_open(
  struct perf_event_attr *hw_event,
  pid_t pid,
  int cpu,
  int group_fd,
  unsigned long flags
) {
  int ret;
  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
  return ret;
}

void perfevents_init(void) {

#if DEBUG
  LOG("DEBUG = 1\n");
#else
  LOG("DEBUG = 0\n");
#endif

  LOG("USE_GROUPS = %d\n", LIBPERF_TLB_REPORT_USE_GROUPS);
  LOG("STANDALONE = %d\n", LIBPERF_TLB_REPORT_STANDALONE);

  // initialize all performance events
  struct perf_event_desc *pe;
  LIST_FOREACH(pe, &g_pe_list, list) {
    LOG("Initializing perf event %s...\n", pe->name);

    pe->fd = perf_event_open(
      &pe->attrs,
      /*pid=*/0,
      /*cpu=*/-1,
      #if LIBPERF_TLB_REPORT_USE_GROUPS
        /*group_fd=*/g_group_leader_fd,
      #else
        /*group_fd=*/-1,
      #endif
      /*flags=*/0
    );

    if (pe->fd == -1) {
      #if DEBUG
        LOG("Failed to initialize performance event %s: ", pe->name);
        perror("");
      #endif

      continue;
    }

    if (ioctl(pe->fd, PERF_EVENT_IOC_ID, &pe->id) < 0) {
      #if DEBUG
        LOG("Failed to get performance event ID for %s (fd = %d): ", pe->name, pe->fd);
        perror("");
      #endif

      continue;
    }

    LOG("Perf event initialized with fd = %d and id = %lu\n", pe->fd, pe->id);

    if (g_group_leader_fd == -1)
      g_group_leader_fd = pe->fd;

    pe->initialized = true;
  }

  // enable all performance events
  LIST_FOREACH(pe, &g_pe_list, list) {
    if (!pe->initialized)
      continue;

    if (ioctl(pe->fd, PERF_EVENT_IOC_RESET, 0) < 0) {
      #if DEBUG
        LOG("Failed to reset perf event %s", pe->name);
        perror("");
      #endif

      pe->initialized = false;
      continue;
    }

    if (ioctl(pe->fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
      #if DEBUG
        LOG("Failed to enable perf event %s", pe->name);
        perror("");
      #endif

      pe->initialized = false;
      continue;
    }

    LOG("Perf event enabled: %s\n", pe->name);
    g_pe_count++;
  }

  LOG("Done with initializing all perf events...\n");
}

static void perfevent_print(struct perf_event_desc *pe, uint64_t count, uint64_t time_enabled, uint64_t time_running) {
  PRINT("%s = %lu\n", pe->name, (unsigned long)(count * ((double)time_enabled / time_running)));
  PRINT("\t(raw count: %lu, running %u%%: %lu / %lu enabled)\n", count, (unsigned)((double)time_running / time_enabled * 100), time_running, time_enabled);
}

void perfevents_finalize(void) {
  if (g_group_leader_fd == -1) {
    PRINT("No performance events to report!\n");
    return;
  }

  // disable all performance events
  struct perf_event_desc *pe;
  LIST_FOREACH(pe, &g_pe_list, list) {
    if (!pe->initialized)
      continue;

    ioctl(pe->fd, PERF_EVENT_IOC_DISABLE, 0);
  }

  // report all performance events
  PRINT("\n[Performance events]\n");

  #if LIBPERF_TLB_REPORT_USE_GROUPS
    char data_buffer[sizeof(struct perf_event_group_data) + g_pe_count * sizeof(struct perf_event_data)];
    ssize_t rresult = read(g_group_leader_fd, data_buffer, sizeof(data_buffer));

    struct perf_event_group_data *group_data = (struct perf_event_group_data *)data_buffer;

    if (rresult == 0) {
      PRINT("Error: read EOF\n");
    } else if (rresult < 0) {
      perror("Read error: ");
    } else {
      assert(group_data->nr == g_pe_count);
      assert(rresult == sizeof(data_buffer));

      //PRINT("Time enabled: %lu, time running: %lu\n", group_data->time_enabled, group_data->time_running);

      for (size_t i = 0; i < g_pe_count; i++) {
        LIST_FOREACH(pe, &g_pe_list, list) {
          if (pe->id != group_data->values[i].id)
            continue;

          perfevent_print(pe, group_data->values[i].value, group_data->time_enabled, group_data->time_running);
        }
      }
    }
  #else
    LIST_FOREACH(pe, &g_pe_list, list) {
      if (!pe->initialized)
        continue;

      struct perf_event_data event_data;
      ssize_t rresult = read(pe->fd, &event_data, sizeof(event_data));

      if (rresult == 0) {
        PRINT("%s: EOF read\n", pe->name);
      } else if (rresult < 0) {
        PRINT("%s: ", pe->name);
        perror("Read error: ");
      } else {
        perfevent_print(pe, event_data.value, event_data.time_enabled, event_data.time_running);
      }
    }
  #endif

  // print events that weren't enabled
  LIST_FOREACH(pe, &g_pe_list, list) {
    if (pe->initialized)
      continue;

    PRINT("%s: not run\n", pe->name);
  }

  // close all perf events
  LIST_FOREACH(pe, &g_pe_list, list) {
    if (!pe->initialized)
      continue;

    close(pe->fd);
    pe->initialized = false;
  }

  g_pe_count = 0;
}

#if LIBPERF_TLB_REPORT_STANDALONE
  __attribute__((constructor))
  void perfevents_autoinit(void) {
    perfevents_init();
  }

  __attribute__((destructor))
  void perfevents_autoreport(void) {
    perfevents_finalize();
  }
#endif
