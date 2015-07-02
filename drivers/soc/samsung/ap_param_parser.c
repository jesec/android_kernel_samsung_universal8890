#include <mach/ap_param_parser.h>

#include <plat/map-base.h>
#include <plat/map-s5p.h>

#define ALIGNMENT_SIZE	 4

/* Variable */

static struct ap_param_info ap_param_list[];

static char ap_parameter_signature[] = "PARA";
#if defined(CONFIG_AP_PARAM_DUMP)
static struct class *ap_param_class;
#endif

static struct map_desc exynos_iodesc_ap_param __initdata = {
	.type           = MT_DEVICE_CACHED,
};

/* API for internal */

static void ap_param_parse_integer(void **address, void *value)
{
	*((unsigned int *)value) = __raw_readl(*address);
	*address += sizeof(uint32_t);
}

static int ap_param_parse_string(void **address, char **value, unsigned int *length)
{
	ap_param_parse_integer(address, length);
	(*length)++;

	*value = *address;

	if (*length % ALIGNMENT_SIZE != 0)
		*address += *length + ALIGNMENT_SIZE - (*length % ALIGNMENT_SIZE);
	else
		*address += *length;

	return 0;
}

static int ap_param_parse_dvfs_domain(void *address, struct ap_param_dvfs_domain *domain)
{
	int i;
	char *clock_name;
	int length;

	ap_param_parse_integer(&address, &domain->max_frequency);
	ap_param_parse_integer(&address, &domain->min_frequency);
	ap_param_parse_integer(&address, &domain->num_of_clock);
	ap_param_parse_integer(&address, &domain->num_of_level);

	domain->list_clock = kzalloc(sizeof(char *) * domain->num_of_clock, GFP_KERNEL);
	if (domain->list_clock == NULL)
		return -ENOMEM;

	for (i = 0; i < domain->num_of_clock; ++i) {
		if (ap_param_parse_string(&address, &clock_name, &length))
			return -EINVAL;

		domain->list_clock[i] = clock_name;
	}

	domain->list_level = address;
	address += sizeof(struct ap_param_dvfs_level) * domain->num_of_level;

	domain->list_dvfs_value = address;

	return 0;
}

static int ap_param_parse_dvfs_header(void *address, struct ap_param_info *info)
{
	int i;
	char *domain_name;
	unsigned int length, offset;
	struct ap_param_dvfs_header *ap_param_dvfs_header;
	struct ap_param_dvfs_domain *ap_param_dvfs_domain;
	void *address_dvfs_header = address;

	if (address == NULL)
		return -EINVAL;

	ap_param_dvfs_header = kzalloc(sizeof(struct ap_param_dvfs_header), GFP_KERNEL);
	if (ap_param_dvfs_header == NULL)
		return -ENOMEM;

	ap_param_parse_integer(&address, &ap_param_dvfs_header->parser_version);
	ap_param_parse_integer(&address, &ap_param_dvfs_header->version);
	ap_param_parse_integer(&address, &ap_param_dvfs_header->num_of_domain);

	ap_param_dvfs_header->domain_list = kzalloc(sizeof(struct ap_param_dvfs_domain) * ap_param_dvfs_header->num_of_domain,
						GFP_KERNEL);
	if (ap_param_dvfs_header->domain_list == NULL)
		return -EINVAL;

	for (i = 0; i < ap_param_dvfs_header->num_of_domain; ++i) {
		if (ap_param_parse_string(&address, &domain_name, &length))
			return -EINVAL;

		ap_param_parse_integer(&address, &offset);

		ap_param_dvfs_domain = &ap_param_dvfs_header->domain_list[i];
		ap_param_dvfs_domain->domain_name = domain_name;
		ap_param_dvfs_domain->domain_offset = offset;
	}

	for (i = 0; i < ap_param_dvfs_header->num_of_domain; ++i) {
		ap_param_dvfs_domain = &ap_param_dvfs_header->domain_list[i];

		if (ap_param_parse_dvfs_domain(address_dvfs_header + ap_param_dvfs_domain->domain_offset, ap_param_dvfs_domain))
			return -EINVAL;
	}

	info->block_handle = ap_param_dvfs_header;

	return 0;
}

static int ap_param_parse_pll(void *address, struct ap_param_pll *ap_param_pll)
{
	ap_param_parse_integer(&address, &ap_param_pll->type_pll);
	ap_param_parse_integer(&address, &ap_param_pll->num_of_frequency);

	ap_param_pll->frequency_list = address;

	return 0;
}

static int ap_param_parse_pll_header(void *address, struct ap_param_info *info)
{
	int i;
	char *pll_name;
	unsigned int length, offset;
	struct ap_param_pll_header *ap_param_pll_header;
	struct ap_param_pll *ap_param_pll;
	void *address_pll_header = address;

	if (address == NULL)
		return -EINVAL;

	ap_param_pll_header = kzalloc(sizeof(struct ap_param_pll_header), GFP_KERNEL);
	if (ap_param_pll_header == NULL)
		return -ENOMEM;

	ap_param_parse_integer(&address, &ap_param_pll_header->parser_version);
	ap_param_parse_integer(&address, &ap_param_pll_header->version);
	ap_param_parse_integer(&address, &ap_param_pll_header->num_of_pll);

	ap_param_pll_header->pll_list = kzalloc(sizeof(struct ap_param_pll) * ap_param_pll_header->num_of_pll,
							GFP_KERNEL);
	if (ap_param_pll_header->pll_list == NULL)
		return -ENOMEM;

	for (i = 0; i < ap_param_pll_header->num_of_pll; ++i) {

		if (ap_param_parse_string(&address, &pll_name, &length))
			return -EINVAL;

		ap_param_parse_integer(&address, &offset);

		ap_param_pll = &ap_param_pll_header->pll_list[i];
		ap_param_pll->pll_name = pll_name;
		ap_param_pll->pll_offset = offset;
	}

	for (i = 0; i < ap_param_pll_header->num_of_pll; ++i) {
		ap_param_pll = &ap_param_pll_header->pll_list[i];

		if (ap_param_parse_pll(address_pll_header + ap_param_pll->pll_offset, ap_param_pll))
			return -EINVAL;
	}

	info->block_handle = ap_param_pll_header;

	return 0;
}

static int ap_param_parse_voltage_table(void **address, struct ap_param_voltage_domain *domain, struct ap_param_voltage_table *table)
{
	int num_of_data = domain->num_of_group * domain->num_of_level;

	ap_param_parse_integer(address, &table->table_version);
	table->voltages = *address;
	*address += sizeof(int32_t) * num_of_data;

	return 0;
}

static int ap_param_parse_voltage_domain(void *address, struct ap_param_voltage_domain *domain)
{
	int i;

	ap_param_parse_integer(&address, &domain->num_of_group);
	ap_param_parse_integer(&address, &domain->num_of_level);
	ap_param_parse_integer(&address, &domain->num_of_table);

	domain->level_list = address;
	address += sizeof(int32_t) * domain->num_of_level;

	domain->table_list = kzalloc(sizeof(struct ap_param_voltage_table) * domain->num_of_table, GFP_KERNEL);
	if (domain->table_list == NULL)
		return -ENOMEM;

	for (i = 0; i < domain->num_of_table; ++i) {
		if (ap_param_parse_voltage_table(&address, domain, &domain->table_list[i]))
			return -EINVAL;
	}

	return 0;
}

static int ap_param_parse_voltage_header(void *address, struct ap_param_info *info)
{
	int i;
	char *domain_name;
	unsigned int length, offset;
	struct ap_param_voltage_header *ap_param_voltage_header;
	struct ap_param_voltage_domain *ap_param_voltage_domain;
	void *address_voltage_header = address;

	if (address == NULL)
		return -EINVAL;

	ap_param_voltage_header = kzalloc(sizeof(struct ap_param_voltage_header), GFP_KERNEL);
	if (ap_param_voltage_header == NULL)
		return -EINVAL;

	ap_param_parse_integer(&address, &ap_param_voltage_header->parser_version);
	ap_param_parse_integer(&address, &ap_param_voltage_header->version);
	ap_param_parse_integer(&address, &ap_param_voltage_header->num_of_domain);

	ap_param_voltage_header->domain_list = kzalloc(sizeof(struct ap_param_voltage_domain) * ap_param_voltage_header->num_of_domain,
							GFP_KERNEL);
	if (ap_param_voltage_header->domain_list == NULL)
		return -ENOMEM;

	for (i = 0; i < ap_param_voltage_header->num_of_domain; ++i) {
		if (ap_param_parse_string(&address, &domain_name, &length))
			return -EINVAL;

		ap_param_parse_integer(&address, &offset);

		ap_param_voltage_domain = &ap_param_voltage_header->domain_list[i];
		ap_param_voltage_domain->domain_name = domain_name;
		ap_param_voltage_domain->domain_offset = offset;
	}

	for (i = 0; i < ap_param_voltage_header->num_of_domain; ++i) {
		ap_param_voltage_domain = &ap_param_voltage_header->domain_list[i];

		if (ap_param_parse_voltage_domain(address_voltage_header + ap_param_voltage_domain->domain_offset, ap_param_voltage_domain))
			return -EINVAL;
	}

	info->block_handle = ap_param_voltage_header;

	return 0;
}

static int ap_param_parse_rcc_table(void **address, struct ap_param_rcc_domain *domain, struct ap_param_rcc_table *table)
{
	int num_of_data = domain->num_of_group * domain->num_of_level;

	ap_param_parse_integer(address, &table->table_version);

	table->rcc = *address;
	*address += sizeof(int32_t) * num_of_data;

	return 0;
}

static int ap_param_parse_rcc_domain(void *address, struct ap_param_rcc_domain *domain)
{
	int i;

	ap_param_parse_integer(&address, &domain->num_of_group);
	ap_param_parse_integer(&address, &domain->num_of_level);
	ap_param_parse_integer(&address, &domain->num_of_table);

	domain->level_list = address;
	address += sizeof(int32_t) * domain->num_of_level;

	domain->table_list = kzalloc(sizeof(struct ap_param_rcc_table) * domain->num_of_table, GFP_KERNEL);
	if (domain->table_list == NULL)
		return -ENOMEM;

	for (i = 0; i < domain->num_of_table; ++i) {
		if (ap_param_parse_rcc_table(&address, domain, &domain->table_list[i]))
			return -EINVAL;
	}

	return 0;
}

static int ap_param_parse_rcc_header(void *address, struct ap_param_info *info)
{
	int i;
	char *domain_name;
	unsigned int length, offset;
	struct ap_param_rcc_header *ap_param_rcc_header;
	struct ap_param_rcc_domain *ap_param_rcc_domain;
	void *address_rcc_header = address;

	if (address == NULL)
		return -EINVAL;

	ap_param_rcc_header = kzalloc(sizeof(struct ap_param_rcc_header), GFP_KERNEL);

	if (ap_param_rcc_header == NULL)
		return -EINVAL;

	ap_param_parse_integer(&address, &ap_param_rcc_header->parser_version);
	ap_param_parse_integer(&address, &ap_param_rcc_header->version);
	ap_param_parse_integer(&address, &ap_param_rcc_header->num_of_domain);

	ap_param_rcc_header->domain_list = kzalloc(sizeof(struct ap_param_rcc_domain) * ap_param_rcc_header->num_of_domain,
							GFP_KERNEL);

	if (ap_param_rcc_header->domain_list == NULL)
		return -ENOMEM;

	for (i = 0; i < ap_param_rcc_header->num_of_domain; ++i) {
		if (ap_param_parse_string(&address, &domain_name, &length))
			return -EINVAL;

		ap_param_parse_integer(&address, &offset);

		ap_param_rcc_domain = &ap_param_rcc_header->domain_list[i];
		ap_param_rcc_domain->domain_name = domain_name;
		ap_param_rcc_domain->domain_offset = offset;
	}

	for (i = 0; i < ap_param_rcc_header->num_of_domain; ++i) {
		ap_param_rcc_domain = &ap_param_rcc_header->domain_list[i];

		if (ap_param_parse_rcc_domain(address_rcc_header + ap_param_rcc_domain->domain_offset, ap_param_rcc_domain))
			return -EINVAL;
	}

	info->block_handle = ap_param_rcc_header;

	return 0;
}

static int ap_param_parse_mif_thermal_header(void *address, struct ap_param_info *info)
{
	struct ap_param_mif_thermal_header *ap_param_mif_thermal_header;

	if (address == NULL)
		return -EINVAL;

	ap_param_mif_thermal_header = kzalloc(sizeof(struct ap_param_mif_thermal_header), GFP_KERNEL);
	if (ap_param_mif_thermal_header == NULL)
		return -EINVAL;

	ap_param_parse_integer(&address, &ap_param_mif_thermal_header->parser_version);
	ap_param_parse_integer(&address, &ap_param_mif_thermal_header->version);
	ap_param_parse_integer(&address, &ap_param_mif_thermal_header->num_of_level);

	ap_param_mif_thermal_header->level = address;

	info->block_handle = ap_param_mif_thermal_header;

	return 0;
}

static int ap_param_parse_ap_thermal_function(void *address, struct ap_param_ap_thermal_function *function)
{
	int i;
	struct ap_param_ap_thermal_range *range;

	ap_param_parse_integer(&address, &function->num_of_range);

	function->range_list = kzalloc(sizeof(struct ap_param_ap_thermal_range) * function->num_of_range, GFP_KERNEL);

	for (i = 0; i < function->num_of_range; ++i) {
		range = &function->range_list[i];

		ap_param_parse_integer(&address, &range->lower_bound_temperature);
		ap_param_parse_integer(&address, &range->upper_bound_temperature);
		ap_param_parse_integer(&address, &range->max_frequency);
		ap_param_parse_integer(&address, &range->sw_trip);
		ap_param_parse_integer(&address, &range->flag);
	}

	return 0;
}

static int ap_param_parse_ap_thermal_header(void *address, struct ap_param_info *info)
{
	int i;
	char *function_name;
	unsigned int length, offset;
	struct ap_param_ap_thermal_header *ap_param_ap_thermal_header;
	struct ap_param_ap_thermal_function *ap_param_ap_thermal_function;
	void *address_thermal_header = address;

	if (address == NULL)
		return -EINVAL;

	ap_param_ap_thermal_header = kzalloc(sizeof(struct ap_param_ap_thermal_header), GFP_KERNEL);
	if (ap_param_ap_thermal_header == NULL)
		return -EINVAL;

	ap_param_parse_integer(&address, &ap_param_ap_thermal_header->parser_version);
	ap_param_parse_integer(&address, &ap_param_ap_thermal_header->version);
	ap_param_parse_integer(&address, &ap_param_ap_thermal_header->num_of_function);

	ap_param_ap_thermal_header->function_list = kzalloc(sizeof(struct ap_param_ap_thermal_function) * ap_param_ap_thermal_header->num_of_function,
								GFP_KERNEL);
	if (ap_param_ap_thermal_header->function_list == NULL)
		return -ENOMEM;

	for (i = 0; i < ap_param_ap_thermal_header->num_of_function; ++i) {
		if (ap_param_parse_string(&address, &function_name, &length))
			return -EINVAL;

		ap_param_parse_integer(&address, &offset);

		ap_param_ap_thermal_function = &ap_param_ap_thermal_header->function_list[i];
		ap_param_ap_thermal_function->function_name = function_name;
		ap_param_ap_thermal_function->function_offset = offset;
	}

	for (i = 0; i < ap_param_ap_thermal_header->num_of_function; ++i) {
		ap_param_ap_thermal_function = &ap_param_ap_thermal_header->function_list[i];

		if (ap_param_parse_ap_thermal_function(address_thermal_header + ap_param_ap_thermal_function->function_offset,
					ap_param_ap_thermal_function))
			return -EINVAL;
	}

	info->block_handle = ap_param_ap_thermal_header;

	return 0;
}

static int ap_param_parse_margin_domain(void *address, struct ap_param_margin_domain *domain)
{
	ap_param_parse_integer(&address, &domain->num_of_group);
	ap_param_parse_integer(&address, &domain->num_of_level);

	domain->offset = address;

	return 0;
}

static int ap_param_parse_margin_header(void *address, struct ap_param_info *info)
{
	int i;
	char *domain_name;
	unsigned int length, offset;
	struct ap_param_margin_header *ap_param_margin_header;
	struct ap_param_margin_domain *ap_param_margin_domain;
	void *address_margin_header = address;

	if (address == NULL)
		return -EINVAL;

	ap_param_margin_header = kzalloc(sizeof(struct ap_param_margin_header), GFP_KERNEL);
	if (ap_param_margin_header == NULL)
		return -EINVAL;

	ap_param_parse_integer(&address, &ap_param_margin_header->parser_version);
	ap_param_parse_integer(&address, &ap_param_margin_header->version);
	ap_param_parse_integer(&address, &ap_param_margin_header->num_of_domain);

	ap_param_margin_header->domain_list = kzalloc(sizeof(struct ap_param_margin_domain) * ap_param_margin_header->num_of_domain,
								GFP_KERNEL);
	if (ap_param_margin_header->domain_list == NULL)
		return -ENOMEM;
	for (i = 0; i < ap_param_margin_header->num_of_domain; ++i) {
		if (ap_param_parse_string(&address, &domain_name, &length))
			return -EINVAL;

		ap_param_parse_integer(&address, &offset);

		ap_param_margin_domain = &ap_param_margin_header->domain_list[i];
		ap_param_margin_domain->domain_name = domain_name;
		ap_param_margin_domain->domain_offset = offset;
	}

	for (i = 0; i < ap_param_margin_header->num_of_domain; ++i) {
		ap_param_margin_domain = &ap_param_margin_header->domain_list[i];

		if (ap_param_parse_margin_domain(address_margin_header + ap_param_margin_domain->domain_offset,
					ap_param_margin_domain))
			return -EINVAL;
	}

	info->block_handle = ap_param_margin_header;

	return 0;
}

static int ap_param_parse_timing_param_size(void *address, struct ap_param_timing_param_size *size)
{
	ap_param_parse_integer(&address, &size->num_of_timing_param);
	ap_param_parse_integer(&address, &size->num_of_level);

	size->timing_parameter = address;

	return 0;
}

static int ap_param_parse_timing_param_header(void *address, struct ap_param_info *info)
{
	int i;
	struct ap_param_timing_param_header *ap_param_timing_param_header;
	struct ap_param_timing_param_size *ap_param_timing_param_size;
	void *address_param_header = address;

	if (address == NULL)
		return -EINVAL;

	ap_param_timing_param_header = kzalloc(sizeof(struct ap_param_timing_param_header), GFP_KERNEL);
	if (ap_param_timing_param_header == NULL)
		return -ENOMEM;

	ap_param_parse_integer(&address, &ap_param_timing_param_header->parser_version);
	ap_param_parse_integer(&address, &ap_param_timing_param_header->version);
	ap_param_parse_integer(&address, &ap_param_timing_param_header->num_of_size);

	ap_param_timing_param_header->size_list = kzalloc(sizeof(struct ap_param_timing_param_size) * ap_param_timing_param_header->num_of_size,
								GFP_KERNEL);
	if (ap_param_timing_param_header->size_list == NULL)
		return -ENOMEM;

	for (i = 0; i < ap_param_timing_param_header->num_of_size; ++i) {
		ap_param_timing_param_size = &ap_param_timing_param_header->size_list[i];

		ap_param_parse_integer(&address, &ap_param_timing_param_size->memory_size);
		ap_param_parse_integer(&address, &ap_param_timing_param_size->offset);
	}

	for (i = 0; i < ap_param_timing_param_header->num_of_size; ++i) {
		ap_param_timing_param_size = &ap_param_timing_param_header->size_list[i];

		if (ap_param_parse_timing_param_size(address_param_header + ap_param_timing_param_size->offset,
					ap_param_timing_param_size))
			return -EINVAL;
	}

	info->block_handle = ap_param_timing_param_header;

	return 0;
}

static int ap_param_parse_minlock_domain(void *address, struct ap_param_minlock_domain *domain)
{
	ap_param_parse_integer(&address, &domain->num_of_level);

	domain->level = address;

	return 0;
}

static int ap_param_parse_minlock_header(void *address, struct ap_param_info *info)
{
	int i;
	char *domain_name;
	unsigned int length, offset;
	struct ap_param_minlock_header *ap_param_minlock_header;
	struct ap_param_minlock_domain *ap_param_minlock_domain;
	void *address_minlock_header = address;

	if (address == NULL)
		return -EINVAL;

	ap_param_minlock_header = kzalloc(sizeof(struct ap_param_minlock_header), GFP_KERNEL);
	if (ap_param_minlock_header == NULL)
		return -ENOMEM;

	ap_param_parse_integer(&address, &ap_param_minlock_header->parser_version);
	ap_param_parse_integer(&address, &ap_param_minlock_header->version);
	ap_param_parse_integer(&address, &ap_param_minlock_header->num_of_domain);

	ap_param_minlock_header->domain_list = kzalloc(sizeof(struct ap_param_minlock_domain) * ap_param_minlock_header->num_of_domain,
							GFP_KERNEL);
	if (ap_param_minlock_header->domain_list == NULL)
		return -ENOMEM;

	for (i = 0; i < ap_param_minlock_header->num_of_domain; ++i) {
		if (ap_param_parse_string(&address, &domain_name, &length))
			return -EINVAL;

		ap_param_parse_integer(&address, &offset);

		ap_param_minlock_domain = &ap_param_minlock_header->domain_list[i];
		ap_param_minlock_domain->domain_name = domain_name;
		ap_param_minlock_domain->domain_offset = offset;
	}

	for (i = 0; i < ap_param_minlock_header->num_of_domain; ++i) {
		ap_param_minlock_domain = &ap_param_minlock_header->domain_list[i];

		if (ap_param_parse_minlock_domain(address_minlock_header + ap_param_minlock_domain->domain_offset,
					ap_param_minlock_domain))
			return -EINVAL;
	}

	info->block_handle = ap_param_minlock_header;

	return 0;
}

static int ap_param_parse_gen_param_size(void *address, struct ap_param_gen_param_table *size)
{
	ap_param_parse_integer(&address, &size->num_of_col);
	ap_param_parse_integer(&address, &size->num_of_row);

	size->parameter = address;

	return 0;
}

static int ap_param_parse_gen_param_header(void *address, struct ap_param_info *info)
{
	int i;
	char *table_name;
	unsigned int length, offset;
	struct ap_param_gen_param_header *ap_param_gen_param_header;
	struct ap_param_gen_param_table *ap_param_gen_param_table;
	void *address_param_header = address;

	if (address == NULL)
		return -EINVAL;

	ap_param_gen_param_header = kzalloc(sizeof(struct ap_param_gen_param_header), GFP_KERNEL);
	if (ap_param_gen_param_header == NULL)
		return -ENOMEM;

	ap_param_parse_integer(&address, &ap_param_gen_param_header->parser_version);
	ap_param_parse_integer(&address, &ap_param_gen_param_header->version);
	ap_param_parse_integer(&address, &ap_param_gen_param_header->num_of_table);

	ap_param_gen_param_header->table_list = kzalloc(sizeof(struct ap_param_gen_param_table) * ap_param_gen_param_header->num_of_table,
								GFP_KERNEL);
	if (ap_param_gen_param_header->table_list == NULL)
		return -ENOMEM;

	for (i = 0; i < ap_param_gen_param_header->num_of_table; ++i) {
		if (ap_param_parse_string(&address, &table_name, &length))
			return -EINVAL;

		ap_param_parse_integer(&address, &offset);

		ap_param_gen_param_table = &ap_param_gen_param_header->table_list[i];
		ap_param_gen_param_table->table_name = table_name;
		ap_param_gen_param_table->offset = offset;
	}

	for (i = 0; i < ap_param_gen_param_header->num_of_table; ++i) {
		ap_param_gen_param_table = &ap_param_gen_param_header->table_list[i];

		if (ap_param_parse_gen_param_size(address_param_header + ap_param_gen_param_table->offset,
					ap_param_gen_param_table))
			return -EINVAL;
	}

	info->block_handle = ap_param_gen_param_header;

	return 0;
}


struct ap_param_header *ap_param_header;
#if defined(CONFIG_AP_PARAM_DUMP)
static ssize_t ap_param_dump_header(struct class *class,
				struct class_attribute *attr, char *buf)
{
	if (ap_param_header == NULL) {
		pr_info("[AP PARAM] : there is no AP Parameter Information\n");
		return 0;
	}

	pr_info("[AP PARAM] : AP Parameter Information\n");
	pr_info("\t[VA] : %p\n", S5P_VA_AP_PARAMETER);
	pr_info("\t[SIGN] : %c%c%c%c\n",
			ap_param_header->sign[0],
			ap_param_header->sign[1],
			ap_param_header->sign[2],
			ap_param_header->sign[3]);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_header->version[0],
			ap_param_header->version[1],
			ap_param_header->version[2],
			ap_param_header->version[3]);
	pr_info("\t[TOTAL SIZE] : %d\n", ap_param_header->total_size);
	pr_info("\t[NUM OF HEADER] : %d\n", ap_param_header->num_of_header);

	return 0;
}
static CLASS_ATTR(header, S_IRUGO, ap_param_dump_header, NULL);

static ssize_t ap_param_dump_dvfs(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i, j, k;
	struct ap_param_info *info = container_of(attr, struct ap_param_info, sysfs_attr);
	struct ap_param_dvfs_header *ap_param_dvfs_header = info->block_handle;
	struct ap_param_dvfs_domain *domain;
	char buffer[512];
	int buffer_index = 0;

	if (ap_param_dvfs_header == NULL) {
		pr_info("[AP PARAM] : there is no dvfs information\n");
		return 0;
	}

	pr_info("[AP PARAM] : DVFS Information\n");
	pr_info("\t[PARSER VERSION] : %d\n", ap_param_dvfs_header->parser_version);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_dvfs_header->version[0],
			ap_param_dvfs_header->version[1],
			ap_param_dvfs_header->version[2],
			ap_param_dvfs_header->version[3]);
	pr_info("\t[NUM OF DOMAIN] : %d\n", ap_param_dvfs_header->num_of_domain);

	for (i = 0; i < ap_param_dvfs_header->num_of_domain; ++i) {
		domain = &ap_param_dvfs_header->domain_list[i];

		pr_info("\t\t[DOMAIN NAME] : %s\n", domain->domain_name);
		pr_info("\t\t[MAX FREQ] : %u\n", domain->max_frequency);
		pr_info("\t\t[MIN FREQ] : %u\n", domain->min_frequency);
		pr_info("\t\t[NUM OF CLOCK] : %d\n", domain->num_of_clock);

		for (j = 0; j < domain->num_of_clock; ++j) {
			pr_info("\t\t\t[CLOCK NAME] : %s\n", domain->list_clock[j]);
		}

		pr_info("\t\t[NUM OF LEVEL] : %d\n", domain->num_of_level);

		for (j = 0; j < domain->num_of_level; ++j) {
			pr_info("\t\t\t[LEVEL] : %u(%c)\n",
					domain->list_level[j].level,
					domain->list_level[j].level_en ? 'O' : 'X');
		}

		pr_info("\t\t\t\t[TABLE]\n");
		for (j = 0; j < domain->num_of_level; ++j) {
			buffer_index = 0;
			buffer_index += snprintf(buffer + buffer_index,
						sizeof(buffer) - buffer_index,
						"\t\t\t\t");
			for (k = 0; k < domain->num_of_clock; ++k) {
				buffer_index += snprintf(buffer + buffer_index,
						sizeof(buffer) - buffer_index,
						"%u ", domain->list_dvfs_value[j * domain->num_of_clock + k]);
			}
			pr_info("%s\n", buffer);
		}
	}

	return 0;
}

static ssize_t ap_param_dump_pll(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i, j;
	struct ap_param_info *info = container_of(attr, struct ap_param_info, sysfs_attr);
	struct ap_param_pll_header *ap_param_pll_header = info->block_handle;
	struct ap_param_pll *pll;
	struct ap_param_pll_frequency *frequency;

	if (ap_param_pll_header == NULL) {
		pr_info("[AP PARAM] : there is no pll information\n");
		return 0;
	}

	pr_info("[AP PARAM] : PLL Information\n");
	pr_info("\t[PARSER VERSION] : %d\n", ap_param_pll_header->parser_version);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_pll_header->version[0],
			ap_param_pll_header->version[1],
			ap_param_pll_header->version[2],
			ap_param_pll_header->version[3]);
	pr_info("\t[NUM OF PLL] : %d\n", ap_param_pll_header->num_of_pll);

	for (i = 0; i < ap_param_pll_header->num_of_pll; ++i) {
		pll = &ap_param_pll_header->pll_list[i];

		pr_info("\t\t[PLL NAME] : %s\n", pll->pll_name);
		pr_info("\t\t[PLL TYPE] : %d\n", pll->type_pll);
		pr_info("\t\t[NUM OF FREQUENCY] : %d\n", pll->num_of_frequency);

		for (j = 0; j < pll->num_of_frequency; ++j) {
			frequency = &pll->frequency_list[j];

			pr_info("\t\t\t[FREQUENCY] : %u\n", frequency->frequency);
			pr_info("\t\t\t[P] : %d\n", frequency->p);
			pr_info("\t\t\t[M] : %d\n", frequency->m);
			pr_info("\t\t\t[S] : %d\n", frequency->s);
			pr_info("\t\t\t[K] : %d\n", frequency->k);
		}
	}

	return 0;
}

static ssize_t ap_param_dump_voltage(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i, j, k, l;
	struct ap_param_info *info = container_of(attr, struct ap_param_info, sysfs_attr);
	struct ap_param_voltage_header *ap_param_voltage_header = info->block_handle;
	struct ap_param_voltage_domain *domain;
	char buffer[512];
	int buffer_index;

	if (ap_param_voltage_header == NULL) {
		pr_info("[AP PARAM] : there is no asv information\n");
		return 0;
	}
	pr_info("[AP PARAM] : ASV Voltage Information\n");
	pr_info("\t[PARSER VERSION] : %d\n", ap_param_voltage_header->parser_version);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_voltage_header->version[0],
			ap_param_voltage_header->version[1],
			ap_param_voltage_header->version[2],
			ap_param_voltage_header->version[3]);
	pr_info("\t[NUM OF DOMAIN] : %d\n", ap_param_voltage_header->num_of_domain);

	for (i = 0; i < ap_param_voltage_header->num_of_domain; ++i) {
		domain = &ap_param_voltage_header->domain_list[i];

		pr_info("\t\t[DOMAIN NAME] : %s\n", domain->domain_name);
		pr_info("\t\t[NUM OF ASV GROUP] : %d\n", domain->num_of_group);
		pr_info("\t\t[NUM OF LEVEL] : %d\n", domain->num_of_level);

		for (j = 0; j < domain->num_of_level; ++j) {
			pr_info("\t\t\t[FREQUENCY] : %u\n", domain->level_list[j]);
		}

		pr_info("\t\t[NUM OF TABLE] : %d\n", domain->num_of_table);

		for (j = 0; j < domain->num_of_table; ++j) {
			pr_info("\t\t\t[TABLE VERSION] : %d\n", domain->table_list[j].table_version);

			pr_info("\t\t\t\t[TABLE]\n");
			for (k = 0; k < domain->num_of_level; ++k) {
				buffer_index = 0;
				buffer_index += snprintf(buffer + buffer_index,
							sizeof(buffer) - buffer_index,
							"\t\t\t\t");
				for (l = 0; l < domain->num_of_group; ++l) {
					buffer_index += snprintf(buffer + buffer_index,
							sizeof(buffer) - buffer_index,
							"%u ", domain->table_list[j].voltages[k * domain->num_of_group + l]);
				}
				pr_info("%s\n", buffer);
			}
		}
	}

	return 0;
}

static ssize_t ap_param_dump_rcc(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i, j, k, l;
	struct ap_param_info *info = container_of(attr, struct ap_param_info, sysfs_attr);
	struct ap_param_rcc_header *ap_param_rcc_header = info->block_handle;
	struct ap_param_rcc_domain *domain;
	char buffer[512];
	int buffer_index;

	if (ap_param_rcc_header == NULL) {
		pr_info("[AP PARAM] : there is no rcc information\n");
		return 0;
	}
	pr_info("[AP PARAM] : RCC Information\n");
	pr_info("\t[PARSER VERSION] : %d\n", ap_param_rcc_header->parser_version);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_rcc_header->version[0],
			ap_param_rcc_header->version[1],
			ap_param_rcc_header->version[2],
			ap_param_rcc_header->version[3]);
	pr_info("\t[NUM OF DOMAIN] : %d\n", ap_param_rcc_header->num_of_domain);

	for (i = 0; i < ap_param_rcc_header->num_of_domain; ++i) {
		domain = &ap_param_rcc_header->domain_list[i];

		pr_info("\t\t[DOMAIN NAME] : %s\n", domain->domain_name);
		pr_info("\t\t[NUM OF ASV GROUP] : %d\n", domain->num_of_group);
		pr_info("\t\t[NUM OF LEVEL] : %d\n", domain->num_of_level);

		for (j = 0; j < domain->num_of_level; ++j) {
			pr_info("\t\t\t[FREQUENCY] : %u\n", domain->level_list[j]);
		}

		pr_info("\t\t[NUM OF TABLE] : %d\n", domain->num_of_table);

		for (j = 0; j < domain->num_of_table; ++j) {
			pr_info("\t\t\t[TABLE VERSION] : %d\n", domain->table_list[j].table_version);

			pr_info("\t\t\t\t[TABLE]\n");
			for (k = 0; k < domain->num_of_level; ++k) {
				buffer_index = 0;
				buffer_index += snprintf(buffer + buffer_index,
							sizeof(buffer) - buffer_index,
							"\t\t\t\t");
				for (l = 0; l < domain->num_of_group; ++l) {
					buffer_index += snprintf(buffer + buffer_index,
							sizeof(buffer) - buffer_index,
							"%u ", domain->table_list[j].rcc[k * domain->num_of_group + l]);
				}
				pr_info("%s\n", buffer);
			}
		}
	}


	return 0;
}

static ssize_t ap_param_dump_mif_thermal(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i;
	struct ap_param_info *info = container_of(attr, struct ap_param_info, sysfs_attr);
	struct ap_param_mif_thermal_header *ap_param_mif_thermal_header = info->block_handle;
	struct ap_param_mif_thermal_level *level;

	if (ap_param_mif_thermal_header == NULL) {
		pr_info("[AP PARAM] : there is no mif thermal information\n");
		return 0;
	}

	pr_info("[AP PARAM] : MIF Thermal Information\n");
	pr_info("\t[PARSER VERSION] : %d\n", ap_param_mif_thermal_header->parser_version);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_mif_thermal_header->version[0],
			ap_param_mif_thermal_header->version[1],
			ap_param_mif_thermal_header->version[2],
			ap_param_mif_thermal_header->version[3]);
	pr_info("\t[NUM OF LEVEL] : %d\n", ap_param_mif_thermal_header->num_of_level);

	for (i = 0; i < ap_param_mif_thermal_header->num_of_level; ++i) {
		level = &ap_param_mif_thermal_header->level[i];

		pr_info("\t\t[MR4 LEVEL] : %d\n", level->mr4_level);
		pr_info("\t\t[MAX FREQUENCY] : %u\n", level->max_frequency);
		pr_info("\t\t[MIN FREQUENCY] : %u\n", level->min_frequency);
		pr_info("\t\t[REFRESH RATE] : %u\n", level->refresh_rate_value);
		pr_info("\t\t[POLLING PERIOD] : %u\n", level->polling_period);
		pr_info("\t\t[SW TRIP] : %u\n", level->sw_trip);
	}

	return 0;
}

static ssize_t ap_param_dump_ap_thermal(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i, j;
	struct ap_param_info *info = container_of(attr, struct ap_param_info, sysfs_attr);
	struct ap_param_ap_thermal_header *ap_param_ap_thermal_header = info->block_handle;
	struct ap_param_ap_thermal_function *function;
	struct ap_param_ap_thermal_range *range;

	if (ap_param_ap_thermal_header == NULL) {
		pr_info("[AP PARAM] : there is no ap thermal information\n");
		return 0;
	}

	pr_info("[AP PARAM] : AP Thermal Information\n");
	pr_info("\t[PARSER VERSION] : %d\n", ap_param_ap_thermal_header->parser_version);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_ap_thermal_header->version[0],
			ap_param_ap_thermal_header->version[1],
			ap_param_ap_thermal_header->version[2],
			ap_param_ap_thermal_header->version[3]);
	pr_info("\t[NUM OF FUNCTION] : %d\n", ap_param_ap_thermal_header->num_of_function);

	for (i = 0; i < ap_param_ap_thermal_header->num_of_function; ++i) {
		function = &ap_param_ap_thermal_header->function_list[i];

		pr_info("\t\t[FUNCTION NAME] : %s\n", function->function_name);
		pr_info("\t\t[NUM OF RANGE] : %d\n", function->num_of_range);

		for (j = 0; j < function->num_of_range; ++j) {
			range = &function->range_list[j];

			pr_info("\t\t\t[LOWER BOUND TEMPERATURE] : %u\n", range->lower_bound_temperature);
			pr_info("\t\t\t[UPPER BOUND TEMPERATURE] : %u\n", range->upper_bound_temperature);
			pr_info("\t\t\t[MAX FREQUENCY] : %u\n", range->max_frequency);
			pr_info("\t\t\t[SW TRIP] : %u\n", range->sw_trip);
			pr_info("\t\t\t[FLAG] : %u\n", range->flag);
		}
	}

	return 0;
}

static ssize_t ap_param_dump_margin(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i, j, k;
	struct ap_param_info *info = container_of(attr, struct ap_param_info, sysfs_attr);
	struct ap_param_margin_header *ap_param_margin_header = info->block_handle;
	struct ap_param_margin_domain *domain;
	char buffer[512];
	int buffer_index;

	if (ap_param_margin_header == NULL) {
		pr_info("[AP PARAM] : there is no margin information\n");
		return 0;
	}

	pr_info("[AP PARAM] : Margin Information\n");
	pr_info("\t[PARSER VERSION] : %d\n", ap_param_margin_header->parser_version);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_margin_header->version[0],
			ap_param_margin_header->version[1],
			ap_param_margin_header->version[2],
			ap_param_margin_header->version[3]);
	pr_info("\t[NUM OF DOMAIN] : %d\n", ap_param_margin_header->num_of_domain);

	for (i = 0; i < ap_param_margin_header->num_of_domain; ++i) {
		domain = &ap_param_margin_header->domain_list[i];

		pr_info("\t\t[DOMAIN NAME] : %s\n", domain->domain_name);
		pr_info("\t\t[NUM OF GROUP] : %d\n", domain->num_of_group);
		pr_info("\t\t[NUM OF LEVEL] : %d\n", domain->num_of_level);

		pr_info("\t\t\t[TABLE]\n");
		for (j = 0; j < domain->num_of_level; ++j) {
			buffer_index = 0;
			buffer_index += snprintf(buffer + buffer_index,
						sizeof(buffer) - buffer_index,
						"\t\t\t");
			for (k = 0; k < domain->num_of_group; ++k) {
				buffer_index += snprintf(buffer + buffer_index,
						sizeof(buffer) - buffer_index,
						"%u ", domain->offset[j * domain->num_of_group + k]);
			}
			pr_info("%s\n", buffer);
		}
	}

	return 0;
}

static ssize_t ap_param_dump_timing_parameter(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i, j, k;
	struct ap_param_info *info = container_of(attr, struct ap_param_info, sysfs_attr);
	struct ap_param_timing_param_header *ap_param_timing_param_header = info->block_handle;
	struct ap_param_timing_param_size *size;
	char buffer[512];
	int buffer_index;

	if (ap_param_timing_param_header == NULL) {
		pr_info("[AP PARAM] : there is no timing parameter information\n");
		return 0;
	}

	pr_info("[AP PARAM] : Timing-Parameter Information\n");
	pr_info("\t[PARSER VERSION] : %d\n", ap_param_timing_param_header->parser_version);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_timing_param_header->version[0],
			ap_param_timing_param_header->version[1],
			ap_param_timing_param_header->version[2],
			ap_param_timing_param_header->version[3]);
	pr_info("\t[NUM OF SIZE] : %d\n", ap_param_timing_param_header->num_of_size);

	for (i = 0; i < ap_param_timing_param_header->num_of_size; ++i) {
		size = &ap_param_timing_param_header->size_list[i];

		pr_info("\t\t[MEMORY SIZE] : %u\n", size->memory_size);
		pr_info("\t\t[NUM OF TIMING PARAMETER] : %d\n", size->num_of_timing_param);
		pr_info("\t\t[NUM OF LEVEL] : %d\n", size->num_of_level);

		pr_info("\t\t\t[TABLE]\n");
		for (j = 0; j < size->num_of_level; ++j) {
			buffer_index = 0;
			buffer_index += snprintf(buffer + buffer_index,
						sizeof(buffer) - buffer_index,
						"\t\t\t");
			for (k = 0; k < size->num_of_timing_param; ++k) {
				buffer_index += snprintf(buffer + buffer_index,
						sizeof(buffer) - buffer_index,
						"%u ", size->timing_parameter[j * size->num_of_timing_param + k]);
			}
			pr_info("%s\n", buffer);
		}
	}

	return 0;
}

static ssize_t ap_param_dump_minlock(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i, j;
	struct ap_param_info *info = container_of(attr, struct ap_param_info, sysfs_attr);
	struct ap_param_minlock_header *ap_param_minlock_header = info->block_handle;
	struct ap_param_minlock_domain *domain;

	if (ap_param_minlock_header == NULL) {
		pr_info("[AP PARAM] : there is no minlock information\n");
		return 0;
	}

	pr_info("[AP PARAM] : Minlock Information\n");
	pr_info("\t[PARSER VERSION] : %d\n", ap_param_minlock_header->parser_version);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_minlock_header->version[0],
			ap_param_minlock_header->version[1],
			ap_param_minlock_header->version[2],
			ap_param_minlock_header->version[3]);
	pr_info("\t[NUM OF DOMAIN] : %d\n", ap_param_minlock_header->num_of_domain);

	for (i = 0; i < ap_param_minlock_header->num_of_domain; ++i) {
		domain = &ap_param_minlock_header->domain_list[i];

		pr_info("\t\t[DOMAIN NAME] : %s\n", domain->domain_name);

		for (j = 0; j < domain->num_of_level; ++j) {
			pr_info("\t\t\t[Frequency] : (MAIN)%u, (SUB)%u\n",
					domain->level[j].main_frequencies,
					domain->level[j].sub_frequencies);
		}
	}

	return 0;
}

static ssize_t ap_param_dump_gen_parameter(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i, j, k;
	struct ap_param_info *info = container_of(attr, struct ap_param_info, sysfs_attr);
	struct ap_param_gen_param_header *ap_param_gen_param_header = info->block_handle;
	struct ap_param_gen_param_table *table;
	char buffer[512];
	int buffer_index;

	if (ap_param_gen_param_header == NULL) {
		pr_info("[AP PARAM] : there is no general parameter information\n");
		return 0;
	}

	pr_info("[AP PARAM] : General-Parameter Information\n");
	pr_info("\t[PARSER VERSION] : %d\n", ap_param_gen_param_header->parser_version);
	pr_info("\t[VERSION] : %c%c%c%c\n",
			ap_param_gen_param_header->version[0],
			ap_param_gen_param_header->version[1],
			ap_param_gen_param_header->version[2],
			ap_param_gen_param_header->version[3]);
	pr_info("\t[NUM OF TABLE] : %d\n", ap_param_gen_param_header->num_of_table);

	for (i = 0; i < ap_param_gen_param_header->num_of_table; ++i) {
		table = &ap_param_gen_param_header->table_list[i];

		pr_info("\t\t[TABLE NAME] : %s\n", table->table_name);
		pr_info("\t\t[NUM OF COLUMN] : %d\n", table->num_of_col);
		pr_info("\t\t[NUM OF ROW] : %d\n", table->num_of_row);

		pr_info("\t\t\t[TABLE]\n");
		for (j = 0; j < table->num_of_row; ++j) {
			buffer_index = 0;
			buffer_index += snprintf(buffer + buffer_index,
						sizeof(buffer) - buffer_index,
						"\t\t\t");
			for (k = 0; k < table->num_of_col; ++k) {
				buffer_index += snprintf(buffer + buffer_index,
						sizeof(buffer) - buffer_index,
						"%u ", table->parameter[j * table->num_of_col + k]);
			}
			pr_info("%s\n", buffer);
		}
	}

	return 0;
}
#else

#define ap_param_dump_ap_thermal	NULL
#define ap_param_dump_voltage		NULL
#define ap_param_dump_dvfs		NULL
#define ap_param_dump_margin		NULL
#define ap_param_dump_mif_thermal	NULL
#define ap_param_dump_pll		NULL
#define ap_param_dump_rcc		NULL
#define ap_param_dump_timing_parameter	NULL
#define ap_param_dump_minlock		NULL
#define ap_param_dump_gen_parameter	NULL

#endif

static struct ap_param_info ap_param_list[] = {
	{
		.block_name = BLOCK_AP_THERMAL,
		.block_name_length = sizeof(BLOCK_AP_THERMAL) - 1,
		.parser = ap_param_parse_ap_thermal_header,
		.sysfs_attr = __ATTR(ap_thermal, S_IRUGO, ap_param_dump_ap_thermal, NULL),
		.block_handle = NULL,
	}, {
		.block_name = BLOCK_ASV,
		.block_name_length = sizeof(BLOCK_ASV) - 1,
		.parser = ap_param_parse_voltage_header,
		.sysfs_attr = __ATTR(voltage, S_IRUGO, ap_param_dump_voltage, NULL),
		.block_handle = NULL,
	}, {
		.block_name = BLOCK_DVFS,
		.block_name_length = sizeof(BLOCK_DVFS) - 1,
		.parser = ap_param_parse_dvfs_header,
		.sysfs_attr = __ATTR(dvfs, S_IRUGO, ap_param_dump_dvfs, NULL),
		.block_handle = NULL,
	}, {
		.block_name = BLOCK_MARGIN,
		.block_name_length = sizeof(BLOCK_MARGIN) - 1,
		.parser = ap_param_parse_margin_header,
		.sysfs_attr = __ATTR(margin, S_IRUGO, ap_param_dump_margin, NULL),
		.block_handle = NULL,
	}, {
		.block_name = BLOCK_MIF_THERMAL,
		.block_name_length = sizeof(BLOCK_MIF_THERMAL) - 1,
		.parser = ap_param_parse_mif_thermal_header,
		.sysfs_attr = __ATTR(mif_thermal, S_IRUGO, ap_param_dump_mif_thermal, NULL),
		.block_handle = NULL,
	}, {
		.block_name = BLOCK_PLL,
		.block_name_length = sizeof(BLOCK_PLL) - 1,
		.parser = ap_param_parse_pll_header,
		.sysfs_attr = __ATTR(pll, S_IRUGO, ap_param_dump_pll, NULL),
		.block_handle = NULL,
	}, {
		.block_name = BLOCK_RCC,
		.block_name_length = sizeof(BLOCK_RCC) - 1,
		.parser = ap_param_parse_rcc_header,
		.sysfs_attr = __ATTR(rcc, S_IRUGO, ap_param_dump_rcc, NULL),
		.block_handle = NULL,
	}, {
		.block_name = BLOCK_TIMING_PARAM,
		.block_name_length = sizeof(BLOCK_TIMING_PARAM) - 1,
		.parser = ap_param_parse_timing_param_header,
		.sysfs_attr = __ATTR(timing_parameter, S_IRUGO, ap_param_dump_timing_parameter, NULL),
		.block_handle = NULL,
	}, {
		.block_name = BLOCK_MINLOCK,
		.block_name_length = sizeof(BLOCK_MINLOCK) - 1,
		.parser = ap_param_parse_minlock_header,
		.sysfs_attr = __ATTR(minlock, S_IRUGO, ap_param_dump_minlock, NULL),
		.block_handle = NULL,
	}, {
		.block_name = BLOCK_GEN_PARAM,
		.block_name_length = sizeof(BLOCK_GEN_PARAM) - 1,
		.parser = ap_param_parse_gen_param_header,
		.sysfs_attr = __ATTR(gen_parameter, S_IRUGO, ap_param_dump_gen_parameter, NULL),
		.block_handle = NULL,
	}
};

#if defined(CONFIG_AP_PARAM_DUMP)
static int ap_param_dump_init(void)
{
	int i;

	ap_param_class = class_create(THIS_MODULE, "ap_param");
	if (IS_ERR(ap_param_class)) {
		pr_err("%s: couln't create class\n", __FILE__);
		return PTR_ERR(ap_param_class);
	}

	if (class_create_file(ap_param_class, &class_attr_header))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(ap_param_list); ++i) {
		if (ap_param_list[i].block_handle == NULL)
			continue;

		if (class_create_file(ap_param_class, &ap_param_list[i].sysfs_attr))
			return -EINVAL;

	}

	return 0;
}
late_initcall_sync(ap_param_dump_init);
#endif

/* API for external */

static phys_addr_t ap_param_address;
static phys_addr_t ap_param_size;

void ap_param_init(phys_addr_t address, phys_addr_t size)
{
	for (ap_param_size = 1; ap_param_size < size;)
		ap_param_size = ap_param_size << 1;

	exynos_iodesc_ap_param.length = ap_param_size;
	exynos_iodesc_ap_param.pfn = __phys_to_pfn(address);
	exynos_iodesc_ap_param.virtual = (unsigned long)S5P_VA_AP_PARAMETER;

	ap_param_address = (phys_addr_t)S5P_VA_AP_PARAMETER;
}

void *ap_param_get_block(char *block_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ap_param_list); ++i) {
		if (ap_param_strcmp(block_name, ap_param_list[i].block_name) == 0)
			return ap_param_list[i].block_handle;
	}

	return NULL;
}

struct ap_param_dvfs_domain *ap_param_dvfs_get_domain(void *block, char *domain_name)
{
	int i;
	struct ap_param_dvfs_header *header;
	struct ap_param_dvfs_domain *domain;

	if (block == NULL ||
		domain_name == NULL)
		return NULL;

	header = (struct ap_param_dvfs_header *)block;

	for (i = 0; i < header->num_of_domain; ++i) {
		domain = &header->domain_list[i];

		if (ap_param_strcmp(domain_name, domain->domain_name) == 0)
			return domain;
	}

	return NULL;
}

struct ap_param_pll *ap_param_pll_get_pll(void *block, char *pll_name)
{
	int i;
	struct ap_param_pll_header *header;
	struct ap_param_pll *pll;

	if (block == NULL ||
		pll_name == NULL)
		return NULL;

	header = (struct ap_param_pll_header *)block;

	for (i = 0; i < header->num_of_pll; ++i) {
		pll = &header->pll_list[i];

		if (ap_param_strcmp(pll_name, pll->pll_name) == 0)
			return pll;
	}

	return NULL;
}

struct ap_param_voltage_domain *ap_param_asv_get_domain(void *block, char *domain_name)
{
	int i;
	struct ap_param_voltage_header *header;
	struct ap_param_voltage_domain *domain;

	if (block == NULL ||
		domain_name == NULL)
		return NULL;

	header = (struct ap_param_voltage_header *)block;

	for (i = 0; i < header->num_of_domain; ++i) {
		domain = &header->domain_list[i];

		if (ap_param_strcmp(domain_name, domain->domain_name) == 0)
			return domain;
	}

	return NULL;
}

struct ap_param_rcc_domain *ap_param_rcc_get_domain(void *block, char *domain_name)
{
	int i;
	struct ap_param_rcc_header *header;
	struct ap_param_rcc_domain *domain;

	if (block == NULL ||
		domain_name == NULL)
		return NULL;

	header = (struct ap_param_rcc_header *)block;

	for (i = 0; i < header->num_of_domain; ++i) {
		domain = &header->domain_list[i];

		if (ap_param_strcmp(domain_name, domain->domain_name) == 0)
			return domain;
	}

	return NULL;
}

struct ap_param_mif_thermal_level *ap_param_mif_thermal_get_level(void *block, int mr4_level)
{
	int i;
	struct ap_param_mif_thermal_header *header;
	struct ap_param_mif_thermal_level *level;

	if (block == NULL)
		return NULL;

	header = (struct ap_param_mif_thermal_header *)block;

	for (i = 0; i < header->num_of_level; ++i) {
		level = &header->level[i];

		if (level->mr4_level == mr4_level)
			return level;
	}

	return NULL;
}

struct ap_param_ap_thermal_function *ap_param_ap_thermal_get_function(void *block, char *function_name)
{
	int i;
	struct ap_param_ap_thermal_header *header;
	struct ap_param_ap_thermal_function *function;

	if (block == NULL ||
		function_name == NULL)
		return NULL;

	header = (struct ap_param_ap_thermal_header *)block;

	for (i = 0; i < header->num_of_function; ++i) {
		function = &header->function_list[i];

		if (ap_param_strcmp(function_name, function->function_name) == 0)
			return function;
	}

	return NULL;
}

struct ap_param_margin_domain *ap_param_margin_get_domain(void *block, char *domain_name)
{
	int i;
	struct ap_param_margin_header *header;
	struct ap_param_margin_domain *domain;

	if (block == NULL ||
		domain_name == NULL)
		return NULL;

	header = (struct ap_param_margin_header *)block;

	for (i = 0; i < header->num_of_domain; ++i) {
		domain = &header->domain_list[i];

		if (ap_param_strcmp(domain_name, domain->domain_name) == 0)
			return domain;
	}

	return NULL;
}

struct ap_param_timing_param_size *ap_param_timing_param_get_size(void *block, int dram_size)
{
	int i;
	struct ap_param_timing_param_header *header;
	struct ap_param_timing_param_size *size;

	if (block == NULL)
		return NULL;

	header = (struct ap_param_timing_param_header *)block;

	for (i = 0; i < header->num_of_size; ++i) {
		size = &header->size_list[i];

		if (size->memory_size == dram_size)
			return size;
	}

	return NULL;
}

struct ap_param_minlock_domain *ap_param_minlock_get_domain(void *block, char *domain_name)
{
	int i;
	struct ap_param_minlock_header *header;
	struct ap_param_minlock_domain *domain;

	if (block == NULL ||
		domain_name == NULL)
		return NULL;

	header = (struct ap_param_minlock_header *)block;

	for (i = 0; i < header->num_of_domain; ++i) {
		domain = &header->domain_list[i];

		if (ap_param_strcmp(domain_name, domain->domain_name) == 0)
			return domain;
	}

	return NULL;
}

struct ap_param_gen_param_table *ap_param_gen_param_get_table(void *block, char *table_name)
{
	int i;
	struct ap_param_gen_param_header *header;
	struct ap_param_gen_param_table *table;

	if (block == NULL)
		return NULL;

	header = (struct ap_param_gen_param_header *)block;

	for (i = 0; i < header->num_of_table; ++i) {
		table = &header->table_list[i];

		if (ap_param_strcmp(table->table_name, table_name) == 0)
			return table;
		}

	 return NULL;
}

int ap_param_parse_binary_header(void)
{
	int i, j;
	char *block_name;
	void *address;
	unsigned int length, offset;

	address = (void *)ap_param_address;
	if (address == NULL)
		return -EINVAL;

	ap_param_header = kzalloc(sizeof(struct ap_param_header), GFP_KERNEL);

	ap_param_parse_integer(&address, ap_param_header->sign);
	ap_param_parse_integer(&address, ap_param_header->version);
	ap_param_parse_integer(&address, &ap_param_header->total_size);
	ap_param_parse_integer(&address, &ap_param_header->num_of_header);

	if (memcmp(ap_param_header->sign, ap_parameter_signature, sizeof(ap_parameter_signature) - 1))
		return -EINVAL;

	for (i = 0; i < ap_param_header->num_of_header; ++i) {
		if (ap_param_parse_string(&address, &block_name, &length)) {
			return -EINVAL;
		}

		ap_param_parse_integer(&address, &offset);

		for (j = 0; j < ARRAY_SIZE(ap_param_list); ++j) {
			if (strncmp(block_name, ap_param_list[j].block_name, ap_param_list[j].block_name_length) != 0)
				continue;

			if (ap_param_list[j].parser((void *)ap_param_address + offset, ap_param_list + j)) {
				pr_err("[AP PARAM] : parse error %s\n", block_name);
				return -EINVAL;
			}
		}
	}

	return 0;
}

int ap_param_strcmp(char *src1, char *src2)
{
	for ( ; *src1 == *src2; src1++, src2++)
		if (*src1 == '\0')
			return 0;

	return ((*(unsigned char *)src1 < *(unsigned char *)src2) ? -1 : +1);
}

void ap_param_init_map_io(void)
{
	iotable_init(&exynos_iodesc_ap_param, 1);
}
