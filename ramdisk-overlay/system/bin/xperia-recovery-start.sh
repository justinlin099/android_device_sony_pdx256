#!/system/bin/sh
# The recovery service owns this wrapper and execs the UI only after setup.
# No credential or Android startup arguments are consumed by the bootstrap.
umask 022
exec >>/tmp/recovery.log 2>&1
chmod 0644 /tmp/recovery.log
echo 'Sony startup: wrapper entered'
failed=/tmp/xperia-fbe-startup-failed
rm -f "$failed"
ready=0
if [ "$(getprop sys.xperia.fbe_probe)" = 1 ] ||
    timeout 25 /system/bin/recovery --xperia-service-bootstrap; then
    echo 'Sony startup: bootstrap accepted'
    if timeout 48 /system/bin/sh -c '
        while [ "$(getprop sys.xperia.fbe_client)" != 1 ] ||
            ! cmp -s /xperia_twrp_native_ld.config.txt /linkerconfig/ld.config.txt; do
            sleep 1
        done
    '; then
        echo 'Sony startup: services ready'
        if cmp -s /xperia_twrp_native_ld.config.txt /linkerconfig/ld.config.txt; then
            ready=1
            echo 'Sony startup: init completed linker handoff'
        else
            echo 'Sony startup: linker handoff content verification failed'
        fi
    fi
fi
if [ "$ready" != 1 ]; then
    echo 'Sony startup failed; starting UI without FBE' >&2
    touch "$failed" || exit 1
fi
# The kernel tried ADSP before the read-only firmware partition was mounted.
# Request one retry through init only for the verified offline ADSP instance.
if [ "$ready" = 1 ] && [ -s /lib/firmware/adsp.mdt ]; then
  for adsp_index in 0 1 2 3 4; do
    adsp=/sys/class/remoteproc/remoteproc$adsp_index
    [ -r "$adsp/name" ] || continue
    read -r adsp_name < "$adsp/name"
    [ "$adsp_name" = 3000000.remoteproc-adsp ] || continue
    read -r adsp_firmware < "$adsp/firmware"
    read -r adsp_state < "$adsp/state"
    if [ "$adsp_name" = 3000000.remoteproc-adsp ] &&
       [ "$adsp_firmware" = adsp.mdt ] && [ "$adsp_state" = offline ]; then
        if setprop sys.xperia.adsp_index "$adsp_index"; then
            attempt=0
            while [ "$attempt" -lt 5 ]; do
                read -r adsp_state < "$adsp/state"
                [ "$adsp_state" = running ] && break
                sleep 1
                attempt=$((attempt + 1))
            done
            echo "Sony startup: ADSP retry index=$adsp_index state=$adsp_state"
        else
            echo 'Sony startup: ADSP retry request failed'
        fi
    else
        echo "Sony startup: ADSP retry skipped, name=$adsp_name firmware=$adsp_firmware state=$adsp_state"
    fi
    break
  done
fi
# Start the stock health HAL after the final service manager and ADSP attempt.
if [ "$ready" = 1 ]; then
    if [ "$adsp_state" = running ]; then
        attempt=0
        while [ ! -d /sys/class/power_supply/battery ] && [ "$attempt" -lt 5 ]; do
            sleep 1
            attempt=$((attempt + 1))
        done
    fi
    if ! setprop sys.xperia.health_start 1; then
        echo 'Sony startup: health start request failed'
    fi
fi
echo "Sony startup: handing off to UI, ready=$ready"
exec /system/bin/recovery "$@"
