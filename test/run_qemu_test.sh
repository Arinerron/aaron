#!/bin/bash
# Build the module, package an initramfs, boot QEMU, check results.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
WORK_DIR=$(mktemp -d)
KERNEL="/usr/lib/modules/$(uname -r)/vmlinuz"

cleanup() {
	rm -rf "$WORK_DIR"
}
trap cleanup EXIT

echo ">>> Building module"
make -C "$PROJECT_DIR" -j4 2>&1 || { echo "BUILD FAILED"; exit 1; }

echo ">>> Building initramfs"
INITRAMFS_DIR="$WORK_DIR/initramfs"
mkdir -p "$INITRAMFS_DIR"/{bin,sbin,dev,proc,sys}

# Install busybox with only needed applets
cp /usr/bin/busybox "$INITRAMFS_DIR/bin/busybox"
for cmd in sh cat echo grep ip wget dmesg sleep mount; do
	ln -sf busybox "$INITRAMFS_DIR/bin/$cmd"
done
for cmd in insmod rmmod poweroff; do
	ln -sf /bin/busybox "$INITRAMFS_DIR/sbin/$cmd"
done

# Copy module
cp "$PROJECT_DIR/aaron.ko" "$INITRAMFS_DIR/aaron.ko"

# Install init
cp "$SCRIPT_DIR/guest_init.sh" "$INITRAMFS_DIR/init"
chmod +x "$INITRAMFS_DIR/init"

# Pack initramfs
(cd "$INITRAMFS_DIR" && find . | cpio -oH newc 2>/dev/null) | gzip > "$WORK_DIR/initramfs.cpio.gz"

echo ">>> Booting QEMU"

qemu-system-x86_64 \
	-kernel "$KERNEL" \
	-initrd "$WORK_DIR/initramfs.cpio.gz" \
	-append "console=ttyS0 rdinit=/init panic=-1" \
	-display none \
	-serial file:"$WORK_DIR/serial.log" \
	-no-reboot \
	-m 256M \
	-cpu host \
	-enable-kvm \
	-net none &
QPID=$!

# Wait for QEMU to exit (poweroff) or timeout
TIMEOUT=30
ELAPSED=0
while kill -0 "$QPID" 2>/dev/null; do
	sleep 1
	ELAPSED=$((ELAPSED + 1))
	if [ "$ELAPSED" -ge "$TIMEOUT" ]; then
		echo ">>> QEMU timed out after ${TIMEOUT}s"
		kill "$QPID" 2>/dev/null
		wait "$QPID" 2>/dev/null
		break
	fi
done
wait "$QPID" 2>/dev/null

echo ""
echo "=== SERIAL OUTPUT ==="
cat "$WORK_DIR/serial.log"
echo ""

echo ">>> Checking results"
if grep -q "TEST_RESULT: PASS" "$WORK_DIR/serial.log"; then
	echo "ALL TESTS PASSED"
	exit 0
elif grep -q "TEST_RESULT: FAIL" "$WORK_DIR/serial.log"; then
	echo "SOME TESTS FAILED"
	exit 1
else
	echo "ERROR: no test result found in output"
	exit 2
fi
