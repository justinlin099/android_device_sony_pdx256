# Xperia 1 VII recovery device tree

Device: `pdx256` (Sony Shimanto); validated model: XQ-FS72.
Branch: `twrp-14.1`; factory baseline: 71.2.A.2.35.

This is a custom recovery port, not an unchanged Sony or TeamWin device tree.
It contains product/board configuration, recovery fstab, recovery helpers,
Sony ramdisk overlays and packaging recipe, and SELinux policy differences.
It builds a kernel-less recovery image and does not require a kernel tree.

Clone into `device/sony/pdx256` inside the companion build-integration repo.
The companion vendor tree, pinned upstream manifest, source patches, and
packaging tools are required. A plain upstream TWRP build alone is insufficient.
Follow the integration repo README; do not flash its raw intermediate image.

See NOTICE.md for source and asset attribution.
