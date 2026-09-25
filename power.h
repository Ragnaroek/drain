
#define POWER_RING_CAP 512

struct power_ring {
    size_t head;
    size_t count;
    double values[POWER_RING_CAP];
};

size_t	power_ring_count(const struct power_ring *r);
void power_ring_push(struct power_ring *r, double v);
void power_ring_minmax(const struct power_ring *r,
    size_t n, double *min, double *max);

/* age 0 is the newest sample; caller ensures age < power_ring_count(r). */
double power_ring_at(const struct power_ring *r, size_t ix);
