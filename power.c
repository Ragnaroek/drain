#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <dev/acpica/acpiio.h>

#include "power.h"

size_t
power_ring_count(const struct power_ring *r)
{
	return (r->count);
}

void
power_ring_push(struct power_ring *r, struct power_sample v)
{
	r->values[r->head] = v;
	r->head = (r->head + 1) % POWER_RING_CAP;
	if (r->count < POWER_RING_CAP)
		r->count++;
}

struct power_sample
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
	struct power_sample v;

	if (n > r->count)
		n = r->count;
	*min = *max = 0.0;
	for (age = 0; age < n; age++) {
		v = power_ring_at(r, age);
		if (age == 0 || v.drained < *min)
			*min = v.drained;
		if (age == 0 || v.drained > *max)
			*max = v.drained;
	}
}

/* 1 = AC connected, 0 = on battery, -1 = unknown (no ACPI) */
static int
acline(void)
{
	size_t len;
	int v;

	len = sizeof(v);
	if (sysctlbyname("hw.acpi.acline", &v, &len, NULL, 0) == -1)
		return (-1);
	return (v);
}

// return -1 on error reading, -2 if the battery is not discharging
// (and a cpu based read should be taken)
static int
battery_read(const struct power_src *ps, struct power_read_battery *v)
{
    union acpi_battery_ioctl_arg arg;
   	uint32_t mv;
	double mw;

    if (clock_gettime(CLOCK_MONOTONIC, &v->ts) != 0) {
        return (-1);
    }

	mw = 0.0;
    memset(&arg, 0, sizeof(arg));
    arg.unit = 0;

    if (ioctl(ps->acpi_fd, ACPIIO_BATT_GET_BST, &arg) == -1) {
        return (-1);
    }
    if ((arg.bst.state & ACPI_BATT_STAT_DISCHARG) == 0) {
        return (-2);
    }
    if (arg.bst.cap == ACPI_BATT_UNKNOWN)
		return (-1);

    if (ps->batt_units == ACPI_BIX_UNITS_MA) {
		/* mA * mV / 1000 = mW; fall back to design voltage */
		mv = arg.bst.volt != ACPI_BATT_UNKNOWN ?
		    arg.bst.volt : ps->batt_dvol;
		mw += (double)arg.bst.cap * mv / 1000.0;
	} else
		mw = (double)arg.bst.cap;

    v->capacity = mw;

    return (0);
}

int
power_src_init(struct power_src *ps)
{
    union acpi_battery_ioctl_arg arg;
    int units;

    ps->acpi_fd = open("/dev/acpi", O_RDONLY | O_CLOEXEC);
	if (ps->acpi_fd == -1) {
	    ps->has_battery = false; /* no ACPI, so no battery */
		return (0);
	}

	if (ioctl(ps->acpi_fd, ACPIIO_BATT_GET_UNITS, &units) == -1
	    || units <= 0) {
	    close(ps->acpi_fd);
		ps->acpi_fd = -1;
		ps->has_battery = false;
		return (0);
	}

	memset(&arg, 0, sizeof(arg));
	arg.unit = 0;
	// TODO support more than one battery (treat it as a big block
	// summed up battery?)

	if (ioctl(ps->acpi_fd, ACPIIO_BATT_GET_BIX, &arg) == -1) {
	    return (-1);
	}

	ps->batt_units = arg.bix.units;
	ps->has_battery = true;

	if (ps->has_battery) {
	    battery_read(ps, &ps->last_read_battery);
	}

	return (0);
}


static double
timespec_diff_secs(const struct timespec *start, const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec)
         + (double)(end->tv_nsec - start->tv_nsec) / 1e9;
}

static void
battery_power_sample(const struct power_src *ps,
    struct power_read_battery *next_battery_read,
    struct power_sample *next_sample)
{
    double secs;
    double delta_mwh;

    secs = timespec_diff_secs(&ps->last_read_battery.ts,
        &next_battery_read->ts);
    delta_mwh = ((int32_t)next_battery_read->capacity
        - (int32_t)ps->last_read_battery.capacity) * -1.0;

    next_sample->ts = next_battery_read->ts;
    next_sample->drained = ((delta_mwh * 3600.0) / secs) / 1000.0;
}

int
power_read_sample(struct power_src *ps, struct power_sample *next_sample)
{
    struct power_read_battery next_battery_read;

    if (clock_gettime(CLOCK_MONOTONIC, &next_sample->ts) != 0) {
        return (-1);
    }

    if (acline() == 0) {
        if (battery_read(ps, &next_battery_read) != 0) {
            return (-1);
        }
        battery_power_sample(ps, &next_battery_read, next_sample);
        ps->last_read_battery = next_battery_read;
    } else {
        next_sample->src = AC;
        next_sample->drained = 1.11;
    }

    return (0);
}
