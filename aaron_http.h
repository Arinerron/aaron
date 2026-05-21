#ifndef AARON_HTTP_H
#define AARON_HTTP_H

#include <linux/types.h>

#define AARON_HTTP_MAX_PATH    256
#define AARON_HTTP_MAX_HEADERS 32
#define AARON_HTTP_BUF_SIZE    4096

enum aaron_http_method {
	AARON_HTTP_GET,
	AARON_HTTP_HEAD,
	AARON_HTTP_POST,
	AARON_HTTP_UNKNOWN,
};

struct aaron_http_header {
	const char *name;
	size_t name_len;
	const char *value;
	size_t value_len;
};

/*
 * Parsed request — all pointers reference into the raw receive buffer,
 * so the buffer must outlive this struct.
 */
struct aaron_http_request {
	enum aaron_http_method method;
	const char *path;
	size_t path_len;
	int version_minor; /* 0 for HTTP/1.0, 1 for HTTP/1.1 */
	struct aaron_http_header headers[AARON_HTTP_MAX_HEADERS];
	int nr_headers;
};

/*
 * Parse a raw HTTP request from buf (length len).
 * Returns 0 on success, -EINVAL on malformed input, -EAGAIN if incomplete.
 */
int aaron_http_parse_request(const char *buf, size_t len,
			     struct aaron_http_request *req);

/*
 * Build an HTTP response into buf (capacity cap).
 * Returns number of bytes written, or -ENOMEM if buf is too small.
 */
int aaron_http_build_response(char *buf, size_t cap, int status,
			      const char *content_type,
			      const char *body, size_t body_len);

/* Map status code to reason phrase. */
const char *aaron_http_status_str(int status);

#endif /* AARON_HTTP_H */
