#!/bin/busybox sh
# Guest init script — runs inside QEMU as PID 1.
# Loads the aaron module, runs HTTP tests, reports results on console.

export PATH=/bin:/sbin:/usr/bin:/usr/sbin

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev

PASS=0
FAIL=0

pass() {
	PASS=$((PASS + 1))
	echo "PASS: $1"
}

fail() {
	FAIL=$((FAIL + 1))
	echo "FAIL: $1"
}

# --- Load module ---
echo "=== Loading aaron module ==="
insmod /aaron.ko port=8080
if [ $? -eq 0 ]; then
	pass "insmod aaron.ko"
else
	fail "insmod aaron.ko"
	echo "TEST_RESULT: FAIL"
	poweroff -f
fi

sleep 1

# Verify module is loaded
if grep -q "^aaron " /proc/modules; then
	pass "module present in /proc/modules"
else
	fail "module present in /proc/modules"
fi

# Check dmesg for listening message
if dmesg | grep -q "aaron: listening on port 8080"; then
	pass "dmesg reports listening"
else
	fail "dmesg reports listening"
fi

# --- HTTP tests ---
echo "=== HTTP tests ==="

# Configure loopback
ip link set lo up
ip addr add 127.0.0.1/8 dev lo

# Test 1: GET / returns 200 with expected body
RESP=$(wget -q -O - http://127.0.0.1:8080/ 2>/dev/null)
if echo "$RESP" | grep -q "<h1>aaron</h1>"; then
	pass "GET / returns aaron page"
else
	fail "GET / returns aaron page (got: $RESP)"
fi

# Test 2: GET / response includes HTTP headers
RAW=$(wget -S -O /dev/null http://127.0.0.1:8080/ 2>&1)
if echo "$RAW" | grep -q "200 OK"; then
	pass "GET / returns 200 status"
else
	fail "GET / returns 200 status (got: $RAW)"
fi

if echo "$RAW" | grep -q "Content-Type: text/html"; then
	pass "GET / has Content-Type header"
else
	fail "GET / has Content-Type header"
fi

if echo "$RAW" | grep -q "Content-Length:"; then
	pass "GET / has Content-Length header"
else
	fail "GET / has Content-Length header"
fi

# Test 3: GET /nonexistent returns 404
RAW404=$(wget -S -O /dev/null http://127.0.0.1:8080/nonexistent 2>&1)
if echo "$RAW404" | grep -q "404 Not Found"; then
	pass "GET /nonexistent returns 404"
else
	fail "GET /nonexistent returns 404 (got: $RAW404)"
fi

# Test 4: Multiple sequential requests work
for i in 1 2 3 4 5; do
	wget -q -O /dev/null http://127.0.0.1:8080/ 2>/dev/null
done
# If we get here without hanging, the accept loop is stable
RESP2=$(wget -q -O - http://127.0.0.1:8080/ 2>/dev/null)
if echo "$RESP2" | grep -q "<h1>aaron</h1>"; then
	pass "sequential requests stable (5+1 requests)"
else
	fail "sequential requests stable"
fi

# --- Unload module ---
echo "=== Unloading module ==="
rmmod aaron
if [ $? -eq 0 ]; then
	pass "rmmod aaron"
else
	fail "rmmod aaron"
fi

if ! grep -q "^aaron " /proc/modules; then
	pass "module removed from /proc/modules"
else
	fail "module removed from /proc/modules"
fi

# --- Summary ---
TOTAL=$((PASS + FAIL))
echo "==========================="
echo "RESULTS: $PASS/$TOTAL passed, $FAIL failed"
if [ "$FAIL" -eq 0 ]; then
	echo "TEST_RESULT: PASS"
else
	echo "TEST_RESULT: FAIL"
fi

poweroff -f
