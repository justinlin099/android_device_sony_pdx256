# Licensing

The following original project code/configuration is **GPL-3.0-or-later**
(GNU GPL version 3 or, at your option, any later version). See [LICENSE](LICENSE).

- `Android.bp`
- `AndroidProducts.mk`
- `BoardConfig.mk`
- `twrp_pdx256.mk`
- `recovery/service_trace.c`
- `recovery.fstab`
- `assets/mtp-icons/render.py`
- `assets/mtp-icons/pack.py`
- `ramdisk-overlay/system/bin/xperia-recovery-start.sh`
- `ramdisk-overlay/system/etc/twrp.fstab`

Most files carry SPDX headers. The two ramdisk files use adjacent `.license`
files so their checksummed payload bytes remain unchanged. The sidecars are
repository metadata, not additions to the packaged ramdisk.

## Third-party and mixed-origin files

These are outside the original-code GPL grant and retain their existing terms:

- `fstab.stock-reference` and `recovery/root/init.recovery.qcom.rc`: Sony/Qualcomm
  firmware-derived configuration. The reference fstab retains its Linux
  Foundation notice. The init file's original notice is reproduced in its
  adjacent `.license` file.
- Other `ramdisk-overlay/` files: prebuilt ELF/linker libraries, stock-derived
  init/contexts/properties/loader configuration and mixed-origin ICO assets.
  No blanket license is assigned to these files. The only GPL-covered overlay
  files are the two explicitly listed above.
- `sepolicy/`: changes derived from factory SELinux policy; no blanket GPL
  relicensing of the factory policy.
- SVG/PNG/ICO artwork: mixed project illustration, third-party SONY/XPERIA
  wordmarks and TWRP UI imagery. See [NOTICE.md](NOTICE.md). Licensing the Python
  rendering tools does not license their input/output artwork.
- JSON recipes/configuration and other metadata/documents are not included in
  the original-code grant above.

Existing third-party notices remain applicable. Files without an identified
license are not implicitly GPL or public domain.
