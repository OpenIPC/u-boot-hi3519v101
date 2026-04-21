#include "util.h"
#include "higmac.h"
#include "ctrl.h"
#include "mdio.h"

#define CRG_GMAC			REG_ETH_CRG

#if GMAC_AT_LEAST_2PORT
#define HIGMAC_MACIF0_CTRL		(HIGMAC0_IOBASE + 0x300c)
#define HIGMAC_MACIF1_CTRL		(HIGMAC0_IOBASE + 0x3010)
#else
#if (defined(CONFIG_HI3521D))
#define HIGMAC_MACIF0_CTRL		(HIGMAC0_IOBASE + 0x300c)
#else
#define HIGMAC_MACIF0_CTRL		(CRG_REG_BASE + REG_ETH_MAC_IF)
#endif
#endif

#define HIGMAC_DUAL_MAC_CRF_ACK_TH	(HIGMAC0_IOBASE + 0x3004)

/* Ethernet MAC CRG register bit map */
#define BIT_GMAC0_RST			BIT(0)
#define BIT_GMAC0_CLK_EN		BIT(1)
#define BIT_MACIF0_RST			BIT(2)
#define BIT_GMACIF0_CLK_EN		BIT(3)
#define BIT_RMII0_CLKSEL_PAD		BIT(4)
#define BIT_EXT_PHY0_CLK_SELECT		BIT(6)

#if (defined CONFIG_ARCH_HI3519 || defined CONFIG_ARCH_HI3519V101 || defined CONFIG_ARCH_HI3559 || defined CONFIG_ARCH_HI3556 || \
		defined CONFIG_ARCH_HI3516AV200)
#define BIT_EXT_PHY0_RST		BIT(7)
#define PHY0_CLK_25M			0
#define PHY0_CLK_50M			BIT_EXT_PHY0_CLK_SELECT
#else
#define BIT_EXT_PHY0_RST		BIT(5)
#define PHY0_CLK_25M			BIT_EXT_PHY0_CLK_SELECT
#define PHY0_CLK_50M			0
#endif

#if GMAC_AT_LEAST_2PORT
#define BIT_GSF_PUB_CLK_EN		BIT(7)
#define BIT_GMAC1_RST			BIT(8)
#define BIT_GMAC1_CLK_EN		BIT(9)
#define BIT_MACIF1_RST			BIT(10)
#define BIT_GMACIF1_CLK_EN		BIT(11)
#define BIT_RMII1_CLKSEL_PAD		BIT(12)
#define BIT_EXT_PHY1_RST		BIT(13)
#define BIT_EXT_PHY1_CLK_SELECT		BIT(14)
#define PHY1_CLK_25M			BIT_EXT_PHY1_CLK_SELECT
#endif

void higmac_set_macif(struct higmac_netdev_local *ld, int mode, unsigned int speed)
{
	void *p = (void *)CRG_REG_BASE;
	unsigned long v;

	/* enable change: port_mode */
	higmac_writel_bits(ld, 1, MODE_CHANGE_EN, BIT_MODE_CHANGE_EN);
	if (speed == 2)/* FIXME */
		speed = 5;/* 1000M */
	higmac_writel_bits(ld, speed, PORT_MODE, BITS_PORT_MODE);
	/* disable change: port_mode */
	higmac_writel_bits(ld, 0, MODE_CHANGE_EN, BIT_MODE_CHANGE_EN);

	/* soft reset mac_if */
	v = readl(p + CRG_GMAC);
	if (ld->index) {
#if GMAC_AT_LEAST_2PORT
		v |= BIT_MACIF1_RST;
#endif
	} else {
		v |= BIT_MACIF0_RST;
	}
	writel(v, p + CRG_GMAC);

	/* config mac_if */
	if (ld->index) {
#if GMAC_AT_LEAST_2PORT
		writel(mode, HIGMAC_MACIF1_CTRL);
#endif
	} else {
		writel(mode, HIGMAC_MACIF0_CTRL);
	}

	v = readl(p + CRG_GMAC);
	if (ld->index) {
#if GMAC_AT_LEAST_2PORT
		v &= ~BIT_MACIF1_RST;
#endif
	} else {
		v &= ~BIT_MACIF0_RST;
	}
	writel(v, p + CRG_GMAC);
}

int higmac_hw_set_macaddress(struct higmac_netdev_local *ld, unsigned char *mac)
{
	unsigned long reg;

	reg = mac[1] | (mac[0] << 8);
	higmac_writel(ld, reg, STATION_ADDR_HIGH);

	reg = mac[5] | (mac[4] << 8) | (mac[3] << 16) | (mac[2] << 24);
	higmac_writel(ld, reg, STATION_ADDR_LOW);

	return 0;
}

int higmac_hw_get_macaddress(struct higmac_netdev_local *ld, unsigned char *mac)
{
	unsigned long reg;

	reg = higmac_readl(ld, STATION_ADDR_HIGH);
	mac[0] = (reg>>8) & 0xff;
	mac[1] = reg & 0xff;

	reg = higmac_readl(ld, STATION_ADDR_LOW);
	mac[2] = (reg>>24) & 0xff;
	mac[3] = (reg>>16) & 0xff;
	mac[4] = (reg>>8) & 0xff;
	mac[5] = reg & 0xff;

	return 0;
}

static inline int _higmac_read_irqstatus(struct higmac_netdev_local *ld)
{
	int status;

	status = higmac_readl(ld, STATUS_PMU_INT);

	return status;
}

int higmac_clear_irqstatus(struct higmac_netdev_local *ld, int irqs)
{
	int status;

	higmac_writel(ld, irqs, RAW_PMU_INT);
	status = _higmac_read_irqstatus(ld);

	return status;
}

/*FIXME*/
int higmac_glb_preinit_dummy(struct higmac_netdev_local *ld)
{
	/* drop packet enable */
	higmac_writel(ld, 0x3F, REC_FILT_CONTROL);
	higmac_writel_bits(ld, 0, REC_FILT_CONTROL, BIT_BC_DROP_EN);

	/*clear all interrupt status*/
	higmac_clear_irqstatus(ld, RAW_INT_ALL_MASK);

	/* disable interrupts */
	higmac_writel(ld, ~RAW_INT_ALL_MASK, ENA_PMU_INT);

	return 0;
}

void higmac_external_phy_reset(void)
{
	unsigned int v = 0;

#ifdef	HIGAMC_USE_GPIO_RESET_PHY
	/* use GPIO0_1 to reset external phy */
	/* Set Direction output */
	v = readl(HIGMAC_RESET_GPIO_BASE + HIGMAC_RESET_GPIO_DIR_OFS);
	v |= HIGMAC_RESET_GPIO_DIR_OUT;
	writel(v, HIGMAC_RESET_GPIO_BASE + HIGMAC_RESET_GPIO_DIR_OFS);

	/* Set GPIO0_1=1 */
	writel(HIGMAC_RESET_GPIO_VALUE, HIGMAC_RESET_GPIO_BASE
			+ HIGMAC_RESET_GPIO_DATA_OFS);
	udelay(50000);
	/* Set GPIO0_1=0 to reset phy */
	writel(~HIGMAC_RESET_GPIO_VALUE, HIGMAC_RESET_GPIO_BASE
			+ HIGMAC_RESET_GPIO_DATA_OFS);
	udelay(200000);

	/* Set GPIO0_1=1 to cancel reset phy */
	writel(HIGMAC_RESET_GPIO_VALUE, HIGMAC_RESET_GPIO_BASE
			+ HIGMAC_RESET_GPIO_DATA_OFS);
	udelay(50000);
#else
	/* use CRG register to reset external phy */
	v = readl(CRG_REG_BASE + CRG_GMAC);
	v |= BIT_EXT_PHY0_RST; /* reset */
#if GMAC_AT_LEAST_2PORT
	v |= BIT_EXT_PHY1_RST;
#endif
	writel(v, CRG_REG_BASE + CRG_GMAC);

	udelay(50 * 1000); /* wait for phy reset time */

	v = readl(CRG_REG_BASE + CRG_GMAC);
	v &= ~BIT_EXT_PHY0_RST; /*undo reset */
#if GMAC_AT_LEAST_2PORT
	v &= ~BIT_EXT_PHY1_RST;
#endif
	writel(v, CRG_REG_BASE + CRG_GMAC);

	udelay(60 * 1000); /* wait for future MDIO operation */
#endif
}

#if (defined CONFIG_ARCH_HI3519 || defined CONFIG_ARCH_HI3519V101 || defined CONFIG_ARCH_HI3559 || defined CONFIG_ARCH_HI3556 || \
		defined CONFIG_ARCH_HI3516AV200)
#define HIGMAC_IOCFG_ETH	(IO_CONFIG_REG_BASE + 0x140)
#define HIGMAC_IOCFG_RGMII	0x2
#define HIGMAC_IOCFG_RMII		0x3

static void higmac_config_phy_clk(enum if_mode intf)
{
	unsigned long p = (unsigned long)(CRG_REG_BASE);
	volatile unsigned int v;

	v = readl(p + CRG_GMAC);
	v &= ~BIT_EXT_PHY0_CLK_SELECT;
	v |= PHY0_CLK_25M;
	v |= (BIT_GMAC0_CLK_EN | BIT_GMACIF0_CLK_EN);
	writel(v, p + CRG_GMAC);

	/* soft reset MAC */
	v = readl(p + CRG_GMAC);
	v |= BIT_GMAC0_RST;
	writel(v, p + CRG_GMAC);
	udelay(100);
	v = readl(p + CRG_GMAC);
	v &= ~BIT_GMAC0_RST;
	writel(v, p + CRG_GMAC);

	writel(0xe, HIGMAC_DUAL_MAC_CRF_ACK_TH);

	/* reset external PHY */
	v = readl(p + CRG_GMAC);
	v |= BIT_EXT_PHY0_RST;
	writel(v, p + CRG_GMAC);
	udelay(50 * 1000);
	v = readl(p + CRG_GMAC);
	v &= ~BIT_EXT_PHY0_RST;
	writel(v, p + CRG_GMAC);
	udelay(60 * 1000);
}

/* Raw MDIO read — does not need miiphy bus registration */
static u16 higmac_raw_mdio_read(unsigned int phy_addr, unsigned int reg)
{
	unsigned long iobase = HIGMAC0_IOBASE;
	unsigned int cmd;
	int timeout = 1000;

	/* wait for MDIO ready */
	while (--timeout && (readl(iobase + REG_MDIO_SINGLE_CMD) & (1 << 20)))
		udelay(1);
	if (!timeout)
		return 0xffff;

	/* start read */
	cmd = (1 << 20) | (2 << 16) | ((phy_addr & 0x1f) << 8) | (reg & 0x1f);
	writel(cmd, iobase + REG_MDIO_SINGLE_CMD);

	/* wait for completion */
	timeout = 1000;
	while (--timeout && (readl(iobase + REG_MDIO_SINGLE_CMD) & (1 << 20)))
		udelay(1);
	if (!timeout)
		return 0xffff;

	/* check data valid */
	if (readl(iobase + REG_MDIO_RDATA_STATUS) & 1)
		return 0xffff;

	return (u16)(readl(iobase + REG_MDIO_SINGLE_DATA) >> 16);
}

/* Known RMII-only PHY IDs (upper 28 bits) */
#define PHY_ID_KSZ8051		0x00221550
#define PHY_ID_KSZ8081		0x00221560
#define PHY_ID_IP101A		0x02430C50	/* IC Plus IP101A/IP101G */
#define PHY_ID_MASK		0xFFFFFFF0

static int is_rmii_phy(u32 phy_id)
{
	u32 id = phy_id & PHY_ID_MASK;

	return id == PHY_ID_KSZ8051 ||
	       id == PHY_ID_KSZ8081 ||
	       id == PHY_ID_IP101A;
}

/* Read PHY ID and determine interface mode.
 * Returns interface_mode_rmii for known RMII PHYs,
 * interface_mode_rgmii otherwise, or interface_mode_butt if no PHY found. */
static enum if_mode higmac_phy_probe_intf(void)
{
	u16 id1, id2;
	u32 phy_id;

	id1 = higmac_raw_mdio_read(higmac_board_info[0].phy_addr, 2);
	id2 = higmac_raw_mdio_read(higmac_board_info[0].phy_addr, 3);

	phy_id = ((u32)id1 << 16) | id2;
	if (phy_id == 0 || phy_id == 0xffffffff ||
	    phy_id == 0xffff || phy_id == 0xffff0000)
		return interface_mode_butt; /* no PHY */

	printf("PHY ID: 0x%08x\n", phy_id);

	if (is_rmii_phy(phy_id))
		return interface_mode_rmii;

	return interface_mode_rgmii;
}

static void higmac_setup_intf(enum if_mode intf)
{
	if (intf == interface_mode_rmii)
		writel(HIGMAC_IOCFG_RMII, HIGMAC_IOCFG_ETH);
	else
		writel(HIGMAC_IOCFG_RGMII, HIGMAC_IOCFG_ETH);
	higmac_board_info[0].phy_intf = intf;
	higmac_config_phy_clk(intf);
}

static void higmac_detect_phy_intf(void)
{
	char *s;
	enum if_mode detected;

	/* Check for cached setting from previous boot */
	s = getenv("phy_intf");
	if (s) {
		if (!strncmp(s, "rmii", 4))
			higmac_setup_intf(interface_mode_rmii);
		else
			higmac_setup_intf(interface_mode_rgmii);
		return;
	}

	/* Init with RGMII defaults to power up MDIO bus */
	higmac_setup_intf(interface_mode_rgmii);

	/* Read PHY ID to determine interface mode */
	detected = higmac_phy_probe_intf();
	if (detected == interface_mode_butt) {
		printf("No PHY detected, defaulting to RGMII\n");
		return;
	}

	/* Apply detected mode */
	if (detected != interface_mode_rgmii)
		higmac_setup_intf(detected);

	printf("PHY interface: %s (auto-detected)\n",
	       detected == interface_mode_rmii ? "RMII" : "RGMII");
	setenv("phy_intf",
	       detected == interface_mode_rmii ? "rmii" : "rgmii");
	saveenv();
}
#endif

void higmac_sys_init(void)
{
#if (defined CONFIG_ARCH_HI3519 || defined CONFIG_ARCH_HI3519V101 || defined CONFIG_ARCH_HI3559 || defined CONFIG_ARCH_HI3556 || \
		defined CONFIG_ARCH_HI3516AV200)
	higmac_detect_phy_intf();
#else
	unsigned long p = 0;
	volatile unsigned int v = 0;

	p = (unsigned long)(CRG_REG_BASE);

	v = readl(p + CRG_GMAC);

	/* phy clk select 25MHz */
	v &= ~BIT_EXT_PHY0_CLK_SELECT;
	v |= PHY0_CLK_25M;
#if GMAC_AT_LEAST_2PORT
	v &= ~BIT_EXT_PHY1_CLK_SELECT;
	v |= PHY1_CLK_25M;
#endif
	/* enable clk */
	v |= (BIT_GMAC0_CLK_EN | BIT_GMACIF0_CLK_EN);
#if GMAC_AT_LEAST_2PORT
	v |= BIT_GSF_PUB_CLK_EN;
	v |= (BIT_GMAC1_CLK_EN | BIT_GMACIF1_CLK_EN);
#endif

#ifdef CONFIG_HIGMAC_RMII0_CLK_USE_EXTERNAL_PAD
	if (higmac_board_info[0].phy_intf == interface_mode_rmii)
		v |= BIT_RMII0_CLKSEL_PAD; /* rmii select pad clk */
#endif
#if GMAC_AT_LEAST_2PORT
#ifdef CONFIG_HIGMAC_RMII1_CLK_USE_EXTERNAL_PAD
	if (higmac_board_info[1].phy_intf == interface_mode_rmii)
		v |= BIT_RMII1_CLKSEL_PAD; /* rmii select pad clk */
#endif
#endif

	writel(v, p + CRG_GMAC);

	/*soft reset*/
	v = readl(p + CRG_GMAC);
	v |= BIT_GMAC0_RST;
#if GMAC_AT_LEAST_2PORT
	v |= BIT_GMAC1_RST;
#endif
	writel(v, p + CRG_GMAC);

	udelay(100);

	v = readl(p + CRG_GMAC);
	v &= ~BIT_GMAC0_RST;
#if GMAC_AT_LEAST_2PORT
	v &= ~BIT_GMAC1_RST;
#endif
	writel(v, p + CRG_GMAC);

	writel(0xe, HIGMAC_DUAL_MAC_CRF_ACK_TH);

	higmac_external_phy_reset();
#endif
}

void higmac_sys_exit(void)
{

}

void higmac_sys_allstop(void)
{

}

int higmac_set_hwq_depth(struct higmac_netdev_local *ld)
{
	if (HIGMAC_HWQ_RX_FQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH) {
		BUG();
		return -1;
	}

	higmac_writel_bits(ld, 1, RX_FQ_REG_EN, \
		BITS_RX_FQ_DEPTH_EN);

	higmac_writel_bits(ld, HIGMAC_HWQ_RX_FQ_DEPTH << DESC_WORD_SHIFT,
		RX_FQ_DEPTH, BITS_RX_FQ_DEPTH);

	higmac_writel_bits(ld, 0, RX_FQ_REG_EN, \
		BITS_RX_FQ_DEPTH_EN);

	if (HIGMAC_HWQ_RX_BQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH) {
		BUG();
		return -1;
	}

	higmac_writel_bits(ld, 1, RX_BQ_REG_EN, \
		BITS_RX_BQ_DEPTH_EN);

	higmac_writel_bits(ld, HIGMAC_HWQ_RX_BQ_DEPTH << DESC_WORD_SHIFT,
		RX_BQ_DEPTH, BITS_RX_BQ_DEPTH);

	higmac_writel_bits(ld, 0, RX_BQ_REG_EN, \
		BITS_RX_BQ_DEPTH_EN);

	if (HIGMAC_HWQ_TX_BQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH) {
		BUG();
		return -1;
	}

	higmac_writel_bits(ld, 1, TX_BQ_REG_EN, \
		BITS_TX_BQ_DEPTH_EN);

	higmac_writel_bits(ld, HIGMAC_HWQ_TX_BQ_DEPTH << DESC_WORD_SHIFT,
		TX_BQ_DEPTH, BITS_TX_BQ_DEPTH);

	higmac_writel_bits(ld, 0, TX_BQ_REG_EN, \
		BITS_TX_BQ_DEPTH_EN);

	if (HIGMAC_HWQ_TX_RQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH) {
		BUG();
		return -1;
	}

	higmac_writel_bits(ld, 1, TX_RQ_REG_EN, \
		BITS_TX_RQ_DEPTH_EN);

	higmac_writel_bits(ld, HIGMAC_HWQ_TX_RQ_DEPTH << DESC_WORD_SHIFT,
		TX_RQ_DEPTH, BITS_TX_RQ_DEPTH);

	higmac_writel_bits(ld, 0, TX_RQ_REG_EN, \
		BITS_TX_RQ_DEPTH_EN);

	return 0;
}

int higmac_set_rx_fq_hwq_addr(struct higmac_netdev_local *ld,
		unsigned int phy_addr)
{
	higmac_writel_bits(ld, 1, RX_FQ_REG_EN, \
		BITS_RX_FQ_START_ADDR_EN);

	higmac_writel(ld, phy_addr, RX_FQ_START_ADDR);

	higmac_writel_bits(ld, 0, RX_FQ_REG_EN, \
		BITS_RX_FQ_START_ADDR_EN);

	return 0;
}

int higmac_set_rx_bq_hwq_addr(struct higmac_netdev_local *ld,
		unsigned int phy_addr)
{
	higmac_writel_bits(ld, 1, RX_BQ_REG_EN, \
		BITS_RX_BQ_START_ADDR_EN);

	higmac_writel(ld, phy_addr, RX_BQ_START_ADDR);

	higmac_writel_bits(ld, 0, RX_BQ_REG_EN, \
		BITS_RX_BQ_START_ADDR_EN);

	return 0;
}

int higmac_set_tx_bq_hwq_addr(struct higmac_netdev_local *ld,
		unsigned int phy_addr)
{
	higmac_writel_bits(ld, 1, TX_BQ_REG_EN, \
		BITS_TX_BQ_START_ADDR_EN);

	higmac_writel(ld, phy_addr, TX_BQ_START_ADDR);

	higmac_writel_bits(ld, 0, TX_BQ_REG_EN, \
		BITS_TX_BQ_START_ADDR_EN);

	return 0;
}

int higmac_set_tx_rq_hwq_addr(struct higmac_netdev_local *ld,
		unsigned int phy_addr)
{
	higmac_writel_bits(ld, 1, TX_RQ_REG_EN, \
		BITS_TX_RQ_START_ADDR_EN);

	higmac_writel(ld, phy_addr, TX_RQ_START_ADDR);

	higmac_writel_bits(ld, 0, TX_RQ_REG_EN, \
		BITS_TX_RQ_START_ADDR_EN);

	return 0;
}

u32 higmac_desc_enable(struct higmac_netdev_local *ld, u32 desc_ena)
{
	u32 old;

	old = higmac_readl(ld, DESC_WR_RD_ENA);
	higmac_writel(ld, old | desc_ena, DESC_WR_RD_ENA);

	return old;
}

u32 higmac_desc_disable(struct higmac_netdev_local *ld, u32 desc_dis)
{
	u32 old;

	old = higmac_readl(ld, DESC_WR_RD_ENA);
	higmac_writel(ld, old & (~desc_dis), DESC_WR_RD_ENA);

	return old;
}

void higmac_desc_flush(struct higmac_netdev_local *ld)
{
	higmac_writel_bits(ld, 1, STOP_CMD, BITS_TX_STOP_EN);
	while (higmac_readl_bits(ld, FLUSH_CMD, BITS_TX_FLUSH_FLAG) != 1)
		;
	higmac_writel_bits(ld, 1, FLUSH_CMD, BITS_TX_FLUSH_CMD);
	while (higmac_readl_bits(ld, FLUSH_CMD, BITS_TX_FLUSH_FLAG) != 0)
		;
	higmac_writel_bits(ld, 0, FLUSH_CMD, BITS_TX_FLUSH_CMD);
	higmac_writel_bits(ld, 0, STOP_CMD, BITS_TX_STOP_EN);

	higmac_writel_bits(ld, 1, STOP_CMD, BITS_RX_STOP_EN);
	while (higmac_readl_bits(ld, FLUSH_CMD, BITS_RX_FLUSH_FLAG) != 1)
		;
	higmac_writel_bits(ld, 1, FLUSH_CMD, BITS_RX_FLUSH_CMD);
	while (higmac_readl_bits(ld, FLUSH_CMD, BITS_RX_FLUSH_FLAG) != 0)
		;
	higmac_writel_bits(ld, 0, FLUSH_CMD, BITS_RX_FLUSH_CMD);
	higmac_writel_bits(ld, 0, STOP_CMD, BITS_RX_STOP_EN);
}
