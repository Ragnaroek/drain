#include <sys/types.h>

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define POWER_RING_CAP 512

struct power_read_battery {
    struct timespec ts;
    uint32_t capacity; //in mWh
};

struct power_src {
    int acpi_fd;
    bool has_battery;
    uint32_t batt_units;
    uint32_t batt_dvol; // used for fallback
    // state management
    struct power_read_battery last_read_battery;
};

enum power_sample_src_type {
    BATTERY,
    AC,
};

struct power_sample {
    enum power_sample_src_type src;
    struct timespec ts;
    double drained; // in watt
};

struct power_ring {
    size_t head;
    size_t count;
    struct power_sample values[POWER_RING_CAP];
};

size_t power_ring_count(const struct power_ring *r);
void power_ring_push(struct power_ring *r, struct power_sample v);
void power_ring_minmax(const struct power_ring *r,
    size_t n, double *min, double *max);
/* age 0 is the newest sample; caller ensures age < power_ring_count(r). */
struct power_sample power_ring_at(const struct power_ring *r, size_t ix);

int power_src_init(struct power_src *ps);

/* read one sample, either from the ACPI/battery RAPL/cpu */
int power_read_sample(struct power_src *ps,
    struct power_sample *new_sample);
