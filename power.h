
#define POWER_RING_CAP 512

struct power_ring {
    size_t head;
    size_t count;
    double values[POWER_RING_CAP];
};
