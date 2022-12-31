#include <stdio.h>
#include <stdarg.h>
#include <glib.h>

#include "main.h"

#include "out.h"

int outline = 1;
static gboolean in_out = TRUE;
int outhline = 1;
static gboolean in_outh = TRUE;
int outphline = 1;
static gboolean in_outph = TRUE;

extern char file_sep;

static int
strchrcnt(char *s, char c)
{
	int cnt = 0;
	for(;*s;s++)
		if(*s == c)
			cnt++;
	return cnt;
}

void
out_printf(FILE *fp, const char *format,...)
{
	va_list ap;
	char *s;

	if (no_write)
		return;

	va_start(ap, format);
	s = g_strdup_vprintf(format, ap);
	va_end(ap);

	if(fp == out)
		outline += strchrcnt(s, '\n');
	else if(fp == outh)
		outhline += strchrcnt(s, '\n');
	else if(fp == outph)
		outphline += strchrcnt(s, '\n');
	else
		g_assert_not_reached();

	fprintf(fp, "%s", s);
	g_free(s);
}

void
out_addline_infile(FILE *fp, int line)
{
	if(no_lines || no_write)
		return;

	if(fp == out) {
		outline++;
		in_out = FALSE;
	} else if(fp == outh) {
		outhline++;
		in_outh = FALSE;
	} else if(fp == outph) {
		outphline++;
		in_outph = FALSE;
	} else
		g_assert_not_reached();

	fprintf(fp, "#line %d \"%s\"\n", line, filename);
}

void
out_addline_outfile(FILE *fp)
{
	if(no_lines || no_write)
		return;

	if(fp == out) {
		if(in_out) return;
		outline++;
		if(!for_cpp)
			fprintf(fp,"#line %d \"%s.c\"\n",outline,filebase);
		else
			fprintf(fp,"#line %d \"%s.cc\"\n",outline,filebase);
		in_out = TRUE;
	} else if(fp == outh) {
		if(in_outh) return;
		outhline++;
		fprintf(fp,"#line %d \"%s.h\"\n",outhline,filebase);
		in_outh = TRUE;
	} else if(fp == outph) {
		char sep[2] = {0,0};
		if(in_outph) return;
		outphline++;
		if (file_sep != 0)
			sep[0] = file_sep;
		fprintf(fp,"#line %d \"%s%sprivate.h\"\n",outphline,filebase,sep);
		in_outph = TRUE;
	} else
		g_assert_not_reached();
}
