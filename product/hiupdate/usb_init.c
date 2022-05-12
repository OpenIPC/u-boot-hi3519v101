static int usb_stor_init(void)
{
	int ret = -1;
	int is_ohci = 0;

try_again:
	if (usb_stop() < 0) {
		debug("usb_stop failed\n");
		return ret;
	}
#if 0
	if (usb_init_debug() < 0)
		debug("usb_init_debug failed\n");
#endif
	wait_ms(1000);
	ret = usb_init();
	if (ret == -3)
		goto try_again;

	if (ret < 0) {
		debug("usb_init failed for xhci!\n");
		return ret;
	}

	/*
	 * check whether a storage device is attached (assume that it's
	 * a USB memory stick, since nothing else should be attached).
	 */
	ret = usb_stor_scan(0);
	if (-1 == ret) {
		scan_num = 1;
		is_ohci = 1;
		have_xhci_device = 0;
		debug("No USB device found on xhci. Go to scan ohci!\n");
	}

	if (is_ohci) {
		ret = usb_init();
		if (ret < 0) {
			debug("usb_init failed for ohci!\n");
			return ret;
		}
		ret = usb_stor_scan(0);
		if (-1 == ret)
			debug("No USB device found on ohci. Not initialized!\n");
	}
	return ret;
}

static void usb_stor_exit(void)
{
	usb_stop();
}

