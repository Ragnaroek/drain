#include <sys/cdefs.h>
#include <stdio.h>
#include <notcurses/notcurses.h>

int
main(int argc __unused, char **argv __unused) {
   	struct notcurses_options opts = {
		.flags = NCOPTION_SUPPRESS_BANNERS,
	};
	struct notcurses *nc;
	struct ncplane *std;
	struct ncinput ni;
	unsigned rows, cols;
	uint32_t key;

	if ((nc = notcurses_core_init(&opts, NULL)) == NULL)
		return (EXIT_FAILURE);

	std = notcurses_stdplane(nc);
	ncplane_dim_yx(std, &rows, &cols);

	ncplane_set_fg_rgb8(std, 0x80, 0xff, 0x80);
	ncplane_putstr_yx(std, 1, 2, "drain - power usage");

	ncplane_set_fg_default(std);
	ncplane_printf_yx(std, 3, 2, "terminal: %u x %u", cols, rows);
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
