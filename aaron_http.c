#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/string.h>
#include <linux/kernel.h>
#include "aaron_http.h"

static enum aaron_http_method parse_method(const char *s, size_t len)
{
	if (len == 3 && !memcmp(s, "GET", 3))
		return AARON_HTTP_GET;
	if (len == 4 && !memcmp(s, "HEADs", 4))
		return AARON_HTTP_HEAD;
	if (len == 4 && !memcmp(s, "POST", 4))
		return AARON_HTTP_POST;
	return AARON_HTTP_UNKNOWN;
}

int aaron_http_parse_request(const char *buf, size_t len,
			     struct aaron_http_request *req)
{
	const char *p, *end, *line_end;
	const char *method_end, *path_start, *path_end;

	if (!buf || !len || !req)
		return -EINVAL;

	end = buf + len;
	memset(req, 0, sizeof(*req));

	/* Find end of request line. */
	line_end = strnchr(buf, len, '\n');
	if (!line_end)
		return -EAGAIN;

	/* Method */
	method_end = strnchr(buf, line_end - buf, ' ');
	if (!method_end)
		return -EINVAL;

	req->method = parse_method(buf, method_end - buf);

	/* Path */
	path_start = method_end + 1;
	path_end = strnchr(path_start, line_end - path_start, ' ');
	if (!path_end)
		return -EINVAL;

	req->path = path_start;
	req->path_len = path_end - path_start;

	/* Version — just grab minor digit. */
	p = path_end + 1;
	if (line_end - p >= 8 && !memcmp(p, "HTTP/1.", 7))
		req->version_minor = p[7] - '0';

	/* Headers */
	p = line_end + 1;
	req->nr_headers = 0;

	while (p < end && req->nr_headers < AARON_HTTP_MAX_HEADERS) {
		const char *colon, *val;

		line_end = strnchr(p, end - p, '\n');
		if (!line_end)
			break;

		/* Strip trailing \r */
		{
			const char *eol = line_end;
			if (eol > p && *(eol - 1) == '\r')
				eol--;
			/* Empty line = end of headers */
			if (eol == p)
				break;

			colon = strnchr(p, eol - p, ':');
			if (!colon) {
				p = line_end + 1;
				continue;
			}

			req->headers[req->nr_headers].name = p;
			req->headers[req->nr_headers].name_len = colon - p;

			val = colon + 1;
			while (val < eol && *val == ' ')
				val++;

			req->headers[req->nr_headers].value = val;
			req->headers[req->nr_headers].value_len = eol - val;
			req->nr_headers++;
		}

		p = line_end + 1;
	}

	return 0;
}

const char *aaron_http_status_str(int status)
{
	switch (status) {
	case 200: return "OK";
	case 301: return "Moved Permanently";
	case 400: return "Bad Request";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 500: return "Internal Server Error";
	default:  return "Unknown";
	}
}

int aaron_http_build_response(char *buf, size_t cap, int status,
			      const char *content_type,
			      const char *body, size_t body_len)
{
	int hdr_len;

	hdr_len = snprintf(buf, cap,
			   "HTTP/1.1 %d %s\r\n"
			   "Content-Type: %s\r\n"
			   "Content-Length: %zu\r\n"
			   "Connection: close\r\n"
			   "\r\n",
			   status, aaron_http_status_str(status),
			   content_type, body_len);

	if (hdr_len < 0 || (size_t)hdr_len + body_len > cap)
		return -ENOMEM;

	memcpy(buf + hdr_len, body, body_len);
	return hdr_len + body_len;
}
