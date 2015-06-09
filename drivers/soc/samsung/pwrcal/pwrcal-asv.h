#ifndef __PWRCAL_ASV_H__
#define __PWRCAL_ASV_H__

struct cal_asv_ops {
	void (*print_info)(void);
	void (*set_grp)(unsigned int id, unsigned int asvgrp);
	void (*set_tablever)(unsigned int version);
	int (*set_rcc_table)(void);
	int (*asv_init)(void);
};

extern struct cal_asv_ops cal_asv_ops;

#endif
