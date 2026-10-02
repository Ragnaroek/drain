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
power_ring_push(struct power_ring *r, struct power_read v)
{
	r->values[r->head] = v;
	r->head = (r->head + 1) % POWER_RING_CAP;
	if (r->count < POWER_RING_CAP)
		r->count++;
}

struct power_read
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
	struct power_read v;

	if (n > r->count)
		n = r->count;
	*min = *max = 0.0;
	for (age = 0; age < n; age++) {
		v = power_ring_at(r, age);
		if (age == 0 || v.value < *min)
			*min = v.value;
		if (age == 0 || v.value > *max)
			*max = v.value;
	}
}

int
power_src_init(struct power_src *ps)
{
    union acpi_battery_ioctl_arg arg;
    int units;

    ps->acpi_fd = open("/dev/acpi", O_RDONLY | O_CLOEXEC);
	if (ps->acpi_fd == -1)
		return (-1);			/* no ACPI, so no battery */

	if (ioctl(ps->acpi_fd, ACPIIO_BATT_GET_UNITS, &units) == -1 || units <= 0) {
	    close(ps->acpi_fd);
		ps->acpi_fd = -1;
		return (-1);
	}

	memset(&arg, 0, sizeof(arg));
	arg.unit = 0; // TODO support more than one battery (treat it a big block summed up battery?)

	if (ioctl(ps->acpi_fd, ACPIIO_BATT_GET_BIX, &arg) == -1) {
	    return (-1);
	}

	ps->batt_units = arg.bix.units;
	ps->has_battery = true;

	return (0);
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

/* return -1 on error reading, -2 if the battery is not discharging (and a cpu based read should
 * be taken)
 */
static int
battery_read(const struct power_src *ps, struct power_read *v)
{
    union acpi_battery_ioctl_arg arg;
   	uint32_t mv;
	double mw;

	mw = 0.0;
    memset(&arg, 0, sizeof(arg));
    arg.unit = 0;

    if (ioctl(ps->acpi_fd, ACPIIO_BATT_GET_BST, &arg) == -1) {
        return (-1);
    }
    if ((arg.bst.state & ACPI_BATT_STAT_DISCHARG) == 0) {
        return (-2);
    }
    if (arg.bst.rate == ACPI_BATT_UNKNOWN)
		return (-1);

    if (ps->batt_units == ACPI_BIX_UNITS_MA) {
		/* mA * mV / 1000 = mW; fall back to design voltage */
		mv = arg.bst.volt != ACPI_BATT_UNKNOWN ?
		    arg.bst.volt : ps->batt_dvol;
		mw += (double)arg.bst.rate * mv / 1000.0;
	} else
		mw += (double)arg.bst.rate;

    v->src = BATTERY;
    v->value = mw / 1000.0;

    return (0);
}

int
power_read(const struct power_src *ps, struct power_read *v) {
    if (acline() == 0) {
        return battery_read(ps, v);
    } else {
        v->src = AC;
        v->value = 1.11;
    }
    return (0);
}
