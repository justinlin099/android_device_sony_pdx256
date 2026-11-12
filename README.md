# Xperia 1 VII recovery device tree

Device: `pdx256` (Sony Shimanto); validated model: XQ-FS72.
Branch: `twrp-14.1`; factory baseline: 71.2.A.2.35.
Upstream recovery commit: `426b747737e7ce9e9e17da5b4d2ba883f296aec7`
(`android-14.1`), with `TW_MAIN_VERSION_STR` set to `3.7.1_14`.
The upstream manifest branch is separately named `twrp-14.1`.

This is a custom recovery port, not an unchanged Sony or TeamWin device tree.
It contains product/board configuration, recovery fstab, recovery helpers,
Sony ramdisk overlays and packaging recipe, and SELinux policy differences.
It builds a kernel-less recovery image and does not require a kernel tree.

Clone into `device/sony/pdx256` inside the companion build-integration repo.
The companion vendor tree, pinned upstream manifest, source patches, and
packaging tools are required. A plain upstream TWRP build alone is insufficient.
Follow the integration repo README; do not flash its raw intermediate image.

See NOTICE.md for source and asset attribution.

Companion build integration: https://github.com/justinlin099/twrp_sony_pdx256
Vendor dependencies: https://github.com/justinlin099/android_vendor_sony_pdx256
These repositories are currently private.

Icon sources and regeneration scripts are in assets/mtp-icons. The ICOs contain
eight sizes, switching from upper-rear to front/rear artwork at 64 px.
The product explicitly includes the service launcher required by the recovery
recipe. Unused sample-backup and custom-volume tools have been removed; normal
backups use the integrated TWRP backup code.
