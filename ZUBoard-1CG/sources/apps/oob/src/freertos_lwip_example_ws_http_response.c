/*
 * Copyright (C) 2017 - 2020 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <string.h>
#include "ff.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include "freertos_lwip_example_webserver.h"
#include "xil_printf.h"
#include "stts22htr.h"
#include "lps22hhtr.h"
#include "xstatus.h"
#include "platform_gpio.h"
#include "sleep.h"

char *notfound_header =
	"<html> \
	<head> \
		<title>404</title> \
		<style type=\"text/css\"> \
		div#request {background: #eeeeee} \
		</style> \
	</head> \
	<body> \
	<h1>404 Page Not Found</h1> \
	<div id=\"request\">";

char *notfound_footer =
	"</div> \
	</body> \
	</html>";

/* dynamically generate 404 response:
 * this inserts the original request string in betwween the notfound_header &
 * footer strings
 */
int do_404(int sd, char *req, int rlen)
{
	int len, hlen;
	int BUFSIZE = 1024;
	char buf[BUFSIZE];

	len = strlen(notfound_header) + strlen(notfound_footer) + rlen;

	hlen = generate_http_header(buf, "html", len);
	if (lwip_write(sd, buf, hlen) != hlen) {
		xil_printf("error writing http header to socket\r\n");
		xil_printf("http header = %s\n\r", buf);
		return -1;
	}

	len = lwip_write(sd, notfound_header, strlen(notfound_header));
	if (len != strlen(notfound_header)) {
		xil_printf("error writing not found header to socket\r\n");
		xil_printf("not found header = %s\n\r", notfound_header);
		return -1;
	}

	len = lwip_write(sd, req, rlen);
	if (len != rlen) {
		xil_printf("error writing org req to socket\r\n");
		xil_printf("org req = %s\n\r", notfound_footer);
		return -1;
	}

	len = lwip_write(sd, notfound_footer, strlen(notfound_footer));
	if (len != strlen(notfound_footer)) {
		xil_printf("error writing not found footer to socket\r\n");
		xil_printf("http footer = %s\n\r", notfound_footer);
		return -1;
	}


	return 0;
}

int do_http_post(int sd, char *req, int rlen)
{
	int BUFSIZE = 1024;
	int len, n;
	char buf[BUFSIZE];
	char *p;

	if (is_cmd_led(req))
	{
		int32_t ret;
		char *request;
		char str[80];
		int str_len;
		char *rgbled_str;
		char *color_str;
		int led=-1;
		int color=-1;

		// request should look like this: rgbled=0&color=1
		request = strstr(req, "rgbled");
		if (!request){
			xil_printf("Error: led: bad command\r\n", led);
			return XST_FAILURE;
		}

		color_str = strchr(request, '&');
		if (!color_str){
			xil_printf("Error: led: bad command\r\n", led);
			return XST_FAILURE;
		}
		rgbled_str = request;
		rgbled_str[(color_str-rgbled_str)] = '\0';

		// skip the '&' character
		color_str = color_str+1;

		ret = sscanf(rgbled_str, "rgbled=%d", &led);
		if ( ret != 1 )
		{
			xil_printf("Error: led: bad command\r\n", led);
			return XST_FAILURE;
		}

		ret = sscanf(color_str, "color=%d", &color);
		if(ret != 1)
		{
			xil_printf("Error: led: bad command\r\n", led);
			color = -1;
		}
		else
		{
			ret = control_rgb_leds(led, color);
			if (ret)
			{
				xil_printf("Error: gpios: failed to control led %d\r\n", led);
				color = -1;
			}
		}

		str_len = sprintf(str, "rgbled=%d&color=%d", led, color);
		len = generate_http_header(buf, "txt", str_len);
		p = buf + len;
		strcat(p, str);

		len += str_len;
	}
	else if (is_cmd_switch(req))
	{
		unsigned s = get_switch_state();
		int n_switches = 4;

		xil_printf("http POST: switch state: %x\r\n", s);
		len = generate_http_header(buf, "txt", n_switches);
		p = buf + len;
		for (n = 0; n < n_switches; n++) {
			*p++ = '0' + (s & 0x1);
			s >>= 1;
		}
		*p = 0;

		len += n_switches;
	}
	else if (is_cmd_temp(req))
	{
		int32_t ret;
		float temp;
		char str[80];
		int str_len;

		ret = stts22htr_get_temp(&temp);
		if (ret) {
			xil_printf("Error: stts22htr: Failed to get temperature value\r\n");
			return XST_FAILURE;
		}

		printf("http POST: temp read: %f\r\n", temp);

		str_len = sprintf(str, "%.2f", temp);
		len = generate_http_header(buf, "txt", str_len);
		p = buf + len;
		strcat(p, str);

		len += str_len;
	}
	else if (is_cmd_pressure(req))
	{
		int32_t ret;
		float pressure;
		char str[80];
		int str_len;

		ret = lps22hhtr_get_pressure(&pressure);
		if (ret) {
			xil_printf("Error: lps22hhtr: Failed to get pressure value\r\n");
			return XST_FAILURE;
		}

		printf("http POST: pressure read: %f\r\n", pressure);

		str_len = sprintf(str, "%.2f", pressure);
		len = generate_http_header(buf, "txt", str_len);
		p = buf + len;
		strcat(p, str);

		len += str_len;
	}
	else
	{
		xil_printf("http POST: unsupported command\r\n");
		return -1;
	}

	if (lwip_write(sd, buf, len) != len) {
		xil_printf("error writing http POST response to socket\r\n");
		xil_printf("http header = %s\r\n", buf);
		return -1;
	}

	return 0;
}

/* respond for a file GET request */
int do_http_get(int sd, char *req, int rlen)
{
	int BUFSIZE = 1400;
	char filename[MAX_FILENAME];
	char buf[BUFSIZE];
	int fsize, hlen, n;
	char *fext;
	FIL fil;
	FRESULT Res;

	/* determine file name */
	extract_file_name(filename, req, rlen, MAX_FILENAME);

	/* respond with 404 if not present */
	Res = f_open(&fil, filename, FA_READ);
	if (Res) {
		xil_printf("file %s not found, returning 404\r\n", filename);
		do_404(sd, req, rlen);
		return -1;
	}

	/* respond with correct file */

	xil_printf("http GET: %s\r\n", filename);

	/* get a pointer to file extension */
	fext = get_file_extension(filename);

	/* obtain file size */
	fsize = f_size(&fil);

	/* write the http headers */
	hlen = generate_http_header(buf, fext, fsize);
	if (lwip_write(sd, buf, hlen) != hlen) {
		xil_printf("error writing http header to socket\r\n");
		xil_printf("http header = %s\r\n", buf);
		f_close(&fil);
		return -1;
	}

	/* now write the file */
	while (fsize > 0) {
		int w;

		f_read(&fil, (void *)buf, BUFSIZE, (unsigned int *)&n);
		if ((w = lwip_write(sd, buf, n)) != n) {
			xil_printf("error writing file (%s) to socket, remaining unwritten bytes = %d\r\n", filename, fsize - n);
			xil_printf("attempted to lwip_write %d bytes, actual bytes written = %d\r\n", n, w);
			return -1;
		}

		fsize -= n;
	}

	f_close(&fil);
	return 0;
}

enum http_req_type { HTTP_GET, HTTP_POST, HTTP_UNKNOWN };
enum http_req_type decode_http_request(char *req, int l)
{
	char *get_str = "GET";
	char *post_str = "POST";

	if (!strncmp(req, get_str, strlen(get_str)))
		return HTTP_GET;

	if (!strncmp(req, post_str, strlen(post_str)))
		return HTTP_POST;

	return HTTP_UNKNOWN;
}

void dump_payload(char *p, int len)
{
	int i, j;

	for (i = 0; i < len; i += 16) {
		for (j = 0; j < 16; j++)
			xil_printf("%c ", p[i+j]);
		xil_printf("\r\n");
	}
	xil_printf("total len = %d\r\n", len);
}

/* generate and write out an appropriate response for the http request */
int generate_response(int sd, char *http_req, int http_req_len)
{
	enum http_req_type request_type;
	request_type = decode_http_request(http_req, http_req_len);

	switch (request_type) {
	case HTTP_GET:
		return do_http_get(sd, http_req, http_req_len);
	case HTTP_POST:
		return do_http_post(sd, http_req, http_req_len);
	default:
		xil_printf("request_type != GET|POST\r\n");
		dump_payload(http_req, http_req_len);
		return do_404(sd, http_req, http_req_len);
	}
}
