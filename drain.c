#include <sys/cdefs.h>
#include <sys/event.h>
#include <sys/param.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include <notcurses/notcurses.h>

#include "power.h"

#define COL_VALUE	    0xFFFFFF
#define COL_MUTED	    0x888780
#define COL_SPARK_BATT	0x5DCAA5
#define COL_SPARK_AC    0x85B7EB

#define SAMPLE_SECONDS 5

static const char *blocks[] = {
	" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
};

struct drain_state {
    struct power_ring power;

    // ui state:
    double spark_scale;
};

static void sample_data(struct drain_state *st, struct power_src *ps);
static int ui_update(struct notcurses *nc, struct drain_state *st);
static struct ncplane	*ui_menue(struct ncplane *, unsigned cols);
static struct ncplane   *ui_sparkline(struct ncplane *parent,
    struct drain_state *st, const struct power_ring *r, unsigned screen_width);
static void             draw_sparkline_graph(struct ncplane *n,
    int x, int y, int width, struct drain_state *st, const struct power_ring *r, double max);

static struct drain_state state;
static struct power_src power_source;

int
main(int argc __unused, char **argv __unused)
{
   	struct notcurses_options opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS,
    };
    int kq, nev;
   	struct kevent ch[2], ev;
    bool quit, dirty;
   	struct notcurses *nc;
	struct ncinput ni;
	uint32_t key;

	// power source setup
    if (power_src_init(&power_source) != 0)
       err(1, "power_src_init");

    // notcurses setup

	if ((nc = notcurses_core_init(&opts, NULL)) == NULL)
	    return (EXIT_FAILURE);

	if (ui_update(nc, &state) != 0) {
		notcurses_stop(nc);
	    err(1, "ui_update");
	}

	// kqueue setup
    if ((kq = kqueue()) == -1)
        err(1, "kqueue");

	EV_SET(&ch[0], 1, EVFILT_TIMER, EV_ADD, NOTE_SECONDS, SAMPLE_SECONDS,
        NULL);
	EV_SET(&ch[1], notcurses_inputready_fd(nc), EVFILT_READ, EV_ADD, 0, 0,
        NULL);

	if (kevent(kq, ch, 2, NULL, 0, NULL) == -1)
	    err(1, "kevent");

	// event loop
	for (quit = false; !quit; ) {
	    dirty = false;
	    nev = kevent(kq, NULL, 0, &ev, 1, NULL);
		if (nev == -1) {
		    if (errno == EINTR)
				continue;
			break;
		}
        if (ev.filter == EVFILT_TIMER) {
            sample_data(&state, &power_source);
            dirty = true;
        } else {
            while ((key = notcurses_get_blocking(nc, &ni)) != (uint32_t)-1) {
                if (ni.evtype == NCTYPE_RELEASE)
                    continue;
                if (key == 'q') {
                    quit = true;
                    break;
                }
                dirty = true;
           	}
		}
		if (dirty) {
		    ui_update(nc, &state);
		}
	}

	close(kq);
	notcurses_stop(nc);
	return (EXIT_SUCCESS);
}

void
sample_data(struct drain_state *st, struct power_src *ps)
{
    struct power_sample v;

    power_read_sample(ps, &v);
    power_ring_push(&st->power, v);
}

int
ui_update(struct notcurses *nc, struct drain_state *st)
{
   	struct ncplane *std;
	struct ncplane *menue;
	struct ncplane *sparkline;
    unsigned rows, cols;

    std = notcurses_stdplane(nc);
	ncplane_dim_yx(std, &rows, &cols);

	menue = ui_menue(std, cols);
    if (menue == NULL) {
        return (1);
    }

    sparkline = ui_sparkline(std, st, &st->power, cols);
    if (sparkline == NULL) {
        notcurses_stop(nc);
        return (1);
    }

	ncplane_set_fg_default(std);
	ncplane_putstr_yx(std, rows - 2, 2, "press q to quit");

	notcurses_render(nc);
	return (0);
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

#define LAST_READ_WIDTH 9

struct ncplane*
ui_sparkline(struct ncplane *parent, struct drain_state *st,
    const struct power_ring *r, unsigned screen_width)
{
    struct ncplane *n;
    size_t shown, l;
    double min, max;
    int width;
    char buf[10];

    ncplane_options opts = {
       .x = 0,
       .y = 1,
       .rows = 3,
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
	ncplane_printf_yx(n, 0, 1, "%4.1f W ", power_ring_at(r, 0).drained);
	ncplane_set_fg_rgb(n, COL_MUTED);
	ncplane_putstr_yx(n, 1, 4, "now");

	width = screen_width - LAST_READ_WIDTH * 2;

	draw_sparkline_graph(n, 0, LAST_READ_WIDTH, width, st, r, max);

	// caption
	ncplane_set_fg_rgb(n, COL_MUTED);
	l = snprintf(buf, sizeof buf, "0-%.0f W", st->spark_scale);
	ncplane_putstr_yx(n, 2, LAST_READ_WIDTH + (width - l) , buf);

    return (n);
}

static double
nice_ceil(double w)
{
	static const double steps[] = {
		1, 2, 5, 10, 15, 20, 30, 40, 50, 75, 100, 150, 200, 300, 500
	};
	size_t i;

	for (i = 0; i < nitems(steps); i++)
		if (steps[i] >= w)
			return (steps[i]);
	return (ceil(w / 100.0) * 100.0);
}

/* Grow at once; shrink only once the data uses less than half the scale. */
static double
update_scale(double scale, double max)
{
	double want;

	want = nice_ceil(max * 1.1);
	if (want > scale || max < scale * 0.5)
		return (want);
	return (scale);
}

static void
draw_bar(struct ncplane *n, int y, int x, double v, double scale)
{
    int bot, level, top;

    level = scale > 0.0 ? (int)lround(v / scale * 16.0) : 0;
   	if (level < 1 && v > 0.0)
		level = 1;		/* keep nonzero samples visible */
	if (level > 16)
		level = 16;
	bot = MIN(level, 8);
	top = level - bot;
	ncplane_putstr_yx(n, y, x, blocks[top]);
	ncplane_putstr_yx(n, y + 1, x, blocks[bot]);
}

static void
draw_sparkline_graph(struct ncplane *n, int y, int x, int width,
    struct drain_state *st, const struct power_ring *r, double max)
{
	size_t i, shown;
	int c, pad;
	struct power_sample v;

	if (width <= 0)
		return;
	shown = MIN(power_ring_count(r), (size_t)width);
	pad = width - (int)shown;

	st->spark_scale = update_scale(st->spark_scale, max);

	for (c = 0; c < pad; c++)
		ncplane_putstr_yx(n, y, x + c, " ");

	for (i = 0; i < shown; i++) {
	    v = power_ring_at(r, shown - 1 - i);
		if (v.src == BATTERY)
		    ncplane_set_fg_rgb(n, COL_SPARK_BATT);
		else
		    ncplane_set_fg_rgb(n, COL_SPARK_AC);

		draw_bar(n, y, x + pad + (int)i, v.drained, st->spark_scale);
	}
}
