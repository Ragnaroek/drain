#include <stddef.h>

#include "power.h"

size_t
power_ring_count(const struct power_ring *r)
{
	return (r->count);
}

void
power_ring_push(struct power_ring *r, double v)
{
	r->values[r->head] = v;
	r->head = (r->head + 1) % POWER_RING_CAP;
	if (r->count < POWER_RING_CAP)
		r->count++;
}

double
power_ring_at(const struct power_ring *r, size_t age)
{
	return (r->values[(r->head + POWER_RING_CAP - 1 - age)
	    % POWER_RING_CAP]);
}

void
power_ring_minmax(const struct power_ring *r, size_t n, double *min,
    double *max)
{
	size_t age;
	double v;

	if (n > r->count)
		n = r->count;
	*min = *max = 0.0;
	for (age = 0; age < n; age++) {
		v = power_ring_at(r, age);
		if (age == 0 || v < *min)
			*min = v;
		if (age == 0 || v > *max)
			*max = v;
	}
}
