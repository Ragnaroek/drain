#include <sys/cdefs.h>
#include <sys/param.h>
#include <stdio.h>
#include <math.h>
#include <notcurses/notcurses.h>

#include "power.h"

#define COL_VALUE	0xFFFFFF
#define COL_MUTED	0x888780
#define COL_SPARK	0x5DCAA5

static const char *blocks[] = {
	" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
};

static struct ncplane	*ui_menue(struct ncplane *, unsigned cols);
static struct ncplane   *ui_sparkline(struct ncplane *parent,
    const struct power_ring *r, unsigned screen_width);
static void             draw_sparkline_graph(struct ncplane *n,
    int x, int y, int width, const struct power_ring *r, double max);

struct drain_state {
    struct power_ring power;
};

static struct drain_state st;

int
main(int argc __unused, char **argv __unused)
{
    // Testdata init
    power_ring_push(&st.power, 5.0);
    power_ring_push(&st.power, 7.0);
    power_ring_push(&st.power, 11.0);
    // Testdata init end


   	struct notcurses_options opts = {
		.flags = NCOPTION_SUPPRESS_BANNERS,
	};
	struct notcurses *nc;
	struct ncplane *std;
	struct ncplane *menue;
	struct ncplane *sparkline;
	struct ncinput ni;
	unsigned rows, cols;
	uint32_t key;

	if ((nc = notcurses_core_init(&opts, NULL)) == NULL)
		return (EXIT_FAILURE);

	std = notcurses_stdplane(nc);
	ncplane_dim_yx(std, &rows, &cols);

	menue = ui_menue(std, cols);
    if (menue == NULL) {
        notcurses_stop(nc);
        return (1);
    }

    sparkline = ui_sparkline(std, &st.power, cols);
    if (sparkline == NULL) {
        notcurses_stop(nc);
        return (1);
    }

	ncplane_set_fg_default(std);
	ncplane_putstr_yx(std, rows - 2, 2, "press q to quit");

	notcurses_render(nc);

	while ((key = notcurses_get_blocking(nc, &ni)) != (uint32_t)-1) {
		if (ni.evtype == NCTYPE_RELEASE)
			continue;
		if (key == 'q')
			break;
	}

	notcurses_stop(nc);
	return (EXIT_SUCCESS);
}

struct ncplane*
ui_menue(struct ncplane *parent, unsigned screen_width)
{
    struct ncplane *n;
    uint64_t channels = 0;
    ncplane_options opts = {
       .x = 0,
       .y = 0,
       .rows = 1,
       .cols = screen_width,
       .name = "menue"
    };
    if ((n = ncplane_create(parent, &opts)) == NULL)
        return (NULL);

    ncchannels_set_fg_rgb(&channels, 0xB5D4F4);
	ncchannels_set_bg_rgb(&channels, 0x1F3A5F);
	ncplane_set_base(n, " ", 0, channels);

	ncplane_set_fg_rgb(n, 0xFFFFFF);
	ncplane_putstr(n, "  drain  ");

	ncplane_set_fg_rgb(n, 0x042C53);
	ncplane_set_bg_rgb(n, 0x85B7EB);
	ncplane_putstr(n, " 1 Overview ");

	ncplane_set_channels(n, channels);

	ncplane_set_fg_rgb(n, 0xFAC775);
    ncplane_putstr(n, "   2");
    ncplane_set_fg_rgb(n, 0xB5D4F4);
    ncplane_putstr(n, " Frequencies ");

    ncplane_set_fg_rgb(n, 0xFAC775);
    ncplane_putstr(n, "   3");
    ncplane_set_fg_rgb(n, 0xB5D4F4);
    ncplane_putstr(n, " Tunables ");

    return (n);
}


#define LAST_READ_WIDTH 11

struct ncplane*
ui_sparkline(struct ncplane *parent, const struct power_ring *r,
    unsigned screen_width)
{
    struct ncplane *n;
    size_t shown;
    double min, max;

    ncplane_options opts = {
       .x = 0,
       .y = 1,
       .rows = 2,
       .cols = screen_width,
       .name = "spark"
    };
    if ((n = ncplane_create(parent, &opts)) == NULL)
        return (NULL);

    if (power_ring_count(r) == 0) {
		ncplane_set_fg_rgb(n, COL_MUTED);
		ncplane_putstr_yx(n, 0, 1, "waiting for first sample…");
		return (n);
	}

    // TODO compute width dynamic from available width!
    //shown = MIN(power_ring_count(r), (size_t)MAX(width, 0));
    shown = power_ring_count(r);
    power_ring_minmax(r, shown, &min, &max);

    // last reading info
   	ncplane_set_fg_rgb(n, COL_VALUE);
    ncplane_on_styles(n, NCSTYLE_BOLD);
	ncplane_printf_yx(n, 0, 1, "%4.1f W", power_ring_at(r, 0));
	ncplane_set_fg_rgb(n, COL_MUTED);
	ncplane_putstr(n, " now");

	draw_sparkline_graph(n, 0, LAST_READ_WIDTH,
	    screen_width - LAST_READ_WIDTH * 2, r, max);

    return (n);
}


// returns a normalize level of a reading in reference
// to the max reading.
static int
spark_level(double v, double max)
{
	int level;

	if (max <= 0.0)
		return (0);
	level = (int)lround(v / max * 8.0);
	if (level < 1 && v > 0.0)
		level = 1;		/* keep nonzero samples visible */
	if (level > 8)
		level = 8;
	return (level);
}

static void
draw_sparkline_graph(struct ncplane *n, int y, int x, int width,
    const struct power_ring *r, double max)
{
	size_t i, shown;
	int c, pad;

	if (width <= 0)
		return;
	shown = MIN(power_ring_count(r), (size_t)width);
	pad = width - (int)shown;

	ncplane_set_fg_rgb(n, COL_SPARK);
	for (c = 0; c < pad; c++)
		ncplane_putstr_yx(n, y, x + c, " ");
	for (i = 0; i < shown; i++)
		ncplane_putstr_yx(n, y, x + pad + (int)i,
		    blocks[spark_level(power_ring_at(r, shown - 1 - i), max)]);
}
