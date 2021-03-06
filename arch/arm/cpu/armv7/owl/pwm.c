#include <libfdt_env.h>
#include <fdtdec.h>
#include <fdt.h>
#include <libfdt.h>

#include <asm/arch/clocks.h>
#include <asm/arch/regs.h>
#include <asm/arch/owlxx_lcd.h>
#include <asm/io.h>
#include <asm/arch/pwm.h>

#include <common.h>

struct pwm_info {

	int pwm_node;			/* PWM device tree node */
} pwm_chip;

#define NS_IN_HZ (1000000000UL)

#define PWM_PARENT_HOSC	0
#define PWM_PARENT_LOSC	1

static int pwm_clk_set(int hwpwm, u32 parent, int rate)
{
	u32 parent_rate;
	u32 div;
	u32 tmp;
	u32 cmu_pwmclk_reg;

	cmu_pwmclk_reg = CMU_PWM0CLK + hwpwm * 0x4;

	tmp = readl(cmu_pwmclk_reg);

	if (parent == PWM_PARENT_HOSC) {
		parent_rate = 24000000;
		tmp |= (1 << 12);
	} else {
		parent_rate = 32768;
		tmp &= (~(1 << 12));
	}

	div = DIV_ROUND(parent_rate, rate);
	div -= 1;
	tmp &= (~(0x3ff));
	tmp |= div;

	writel(tmp, cmu_pwmclk_reg);

	return 0;
}

int pwm_enable(int pwm_id)
{
	setbits_le32(CMU_DEVCLKEN1, 0x1 << (23 + pwm_id));
	return 0;
}

void pwm_disable(int pwm_id)
{
	clrbits_le32(CMU_DEVCLKEN1, 0x1 << (23 + pwm_id));
}

int pwm_config(int hwpwm, int duty_ns,
	int period_ns, enum pwm_polarity polarity)
{
	u32 tmp = 0, val = 0;
	u32 pwm_ctl_reg;
	u32 rate;
	u32 parent;

	debug("pwm:request duty = %d\n", duty_ns);
	debug("pwm:request period_ns = %d\n", period_ns);

	if (period_ns > NS_IN_HZ || duty_ns > NS_IN_HZ)
		return -1;

	rate = NS_IN_HZ / period_ns;

	if (rate < 512) {
		parent = PWM_PARENT_LOSC;
	} else if (rate <= 375000) {
		parent = PWM_PARENT_HOSC;
	} else {
		rate = 375000;
		parent = PWM_PARENT_HOSC;
		debug("pwm freq will be 375kHZ at most!\n");
	}

	/*pwm clk has a pre dividor of 64*/
	/*so we should multi 64 first*/
	pwm_clk_set(hwpwm, parent, rate << 6);

	/*round up or down*/
	val = ((((duty_ns) << 6) << 1) / (period_ns) + 1) >> 1;
	val = (val) ? (val-1) : 0;

	pwm_ctl_reg = PWM_CTL0 + (hwpwm << 2);
#if defined(CONFIG_ATM7059A)
	tmp = ((val<<10) + 63);
	
	if (polarity == PWM_POLARITY_NORMAL)
		tmp |= (0x1 << 20);
	else
		tmp &= (~(0x1 << 20));
#else
	tmp = readl(pwm_ctl_reg);
	tmp &= (~0x3f);
	tmp |= (val & 0x3f);
	
	if (polarity == PWM_POLARITY_NORMAL)
		tmp |= (0x1 << 8);
	else
		tmp &= (~(0x1 << 8));
#endif


	writel(tmp, pwm_ctl_reg);

	return 0;
}

static int fdtdec_pwm_parse(const void *blob, int node,
		const char *prop_name, struct pwm_device *pwm)
{
	int pwm_node;
	u32 data[4];

	if (fdtdec_get_int_array(
			blob, node, prop_name, data,
			ARRAY_SIZE(data))) {
		printf("%s: Cannot decode PWM property '%s'\n", __func__,
		      prop_name);
		return -1;
	}

	pwm_node = fdt_node_offset_by_phandle(blob, data[0]);
	printf("pwm_node=%x, pwm_chip.pwm_node=%x\n", pwm_node, pwm_chip.pwm_node);
	if (pwm_node != pwm_chip.pwm_node) {
		printf(
			"%s: PWM property '%s' phandle %d not recognised\n",
			__func__, prop_name, data[0]);
		return -1;
	}

	if (data[1] >= PWM_NUM_CHANNELS) {
		printf("%s: PWM property '%s': invalid channel %u\n", __func__,
		      prop_name, data[1]);
		return -1;
	}

	pwm->hwpwm = data[1];
	pwm->period = data[2];
	pwm->polarity =
		((data[3] == 0) ? PWM_POLARITY_NORMAL : PWM_POLARITY_INVERSED);

	printf("pwm id = %d\n", pwm->hwpwm);
	printf("pwm period = %d\n", pwm->period);
	printf("pwm polarity = %d\n", pwm->polarity);

	return 0;
}

int fdtdec_pwm_get(const void *blob, int node,
		const char *prop_name, struct pwm_device *pwm)
{
	int ret;
	int depth;
	int offset;

	printf("%s: fdtdec_pwm_get(%p, %p, %s, %d)\n", __func__, blob, node, prop_name, pwm->hwpwm);
	ret = fdtdec_pwm_parse(blob, node, prop_name, pwm);
	if (ret) {
		printf("%s: fdtdec_pwm_get failed\n", __func__);
		return -1;
	}

	offset = pwm_chip.pwm_node;
	/*get pwm pins*/
	for (depth = 0; (offset >= 0) && (depth >= 0);
		offset = fdt_next_node(blob, offset, &depth)) {
		if ((depth == 1) && offset > 0) {
			ret = fdtdec_get_int(blob, offset,
				"id", -1);
			if (ret != pwm->hwpwm)
				continue;

			ret = owlxx_device_fdtdec_set_pinctrl_default(offset);
			if (ret) {
				printf("%s: owlxx_device_fdtdec_set_pinctrl_default offset%d failed\n", __func__, offset);
				return -1;
			}
			return 0;
		}
	}

	printf("%s: fdtdec_pwm_get not found\n", __func__);
	/*pwm id not found*/
	return -1;
}

int pwm_init(const void *blob)
{
	printf("pwm_init, blob %p\n", blob);

	pwm_chip.pwm_node = fdtdec_next_compatible(blob, 0,
		COMPAT_ACTIONS_OWLXX_PWM);
	if (pwm_chip.pwm_node < 0) {
		printf("%s: Cannot find device tree node\n", __func__);
		return -1;
	}

	printf("PWM init, node %d\n", pwm_chip.pwm_node);

	return 0;

}
