#!/system/bin/sh
[ "$ARCH" = arm64 ] || abort "Sony HEIF Entry requires ARM64."
[ "$API" -eq 36 ] || abort "This prototype targets Android 16 (API 36)."
if [ "$ZYGISK_ENABLED" != 1 ] && [ ! -d /data/adb/modules/zygisksu ]; then
  abort "Enable Magisk Zygisk, or install and activate Zygisk Next for KernelSU."
fi
set_perm_recursive "$MODPATH" 0 0 0755 0644
ui_print "Sony HEIF Entry: process hooks only; no system mount or system file writes."
ui_print "Targets: com.miui.gallery and com.tencent.mm main processes."
ui_print "HLG uses approximate SDR tone mapping."
ui_print "Activation needs a framework/zygote reload; no reboot is performed by this module."
