#include <sys/cdefs.h>
#include <stdio.h>
#include <notcurses/notcurses.h>

#include "power.h"

static struct ncplane	*ui_menue(struct ncplane *, unsigned cols);
static struct ncplane   *ui_sparkline(struct ncplane *parent, unsigned cols);

struct drain_state {
    struct power_ring power;
};

int
main(int argc __unused, char **argv __unused)
{
    struct drain_state st;

   	struct notcurses_options opts = {
		.flags = NCOPTION_SUPPRESS_BANNERS,
	};
	struct notcurses *nc;
	struct ncplane *std;
	struct ncplane *menue;
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

	ncplane_set_fg_default(std);
	ncplane_printf_yx(std, 3, 2, "terminal: %u x %u, state %p", cols, rows, &st);
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

struct ncplane*
ui_sparkline(struct ncplane *parent, unsigned cols)
{
    // TODO render the sparkline diagram and the
    // texts around it!

    return (NULL);
}
