// file to generate .idx file from an xdxf dictinary file.
// License: GPL-2
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>

static const char raw_entrs[] = { '<',   '>',   '&',    '\'',    '\"',    0 };
static const char* xml_entrs[] = { "lt;", "gt;", "amp;", "apos;", "quot;", 0 };
static const int xml_ent_len[] = { 3, 3, 4, 5, 5 };

int main(int argc, char *argv[])
{
	FILE *infile;
	char *buffer;
	char *p;
	struct stat infStat;
	int ret;
	unsigned int flag = 0;
	unsigned int datasize;

	if (argc != 2) {
		fprintf(stderr, "no file specified, usage:\n\t%s dict_file.dict > idx_file.idx\n", argv[0]);
		return 2;
	}
	ret = stat(argv[1], &infStat);
	if (ret) {
		fprintf(stderr, "get file info failed\n");
		return 3;
	}
	infile = fopen(argv[1], "rb");
	if (!infile) {
		fprintf(stderr, "open file failed\n");
		return 4;
	}
	buffer = malloc(infStat.st_size);
	if (!buffer) {
		fprintf(stderr, "malloc buffer failed\n");
		fclose(infile);
		return 5;
	}
	while ((ret = fread(buffer, 1, infStat.st_size, infile)) > 0) {}
	fclose(infile);

	p = buffer;
	while ((p = strstr(p, "<k>"))) {
		unsigned int dataoffset;
		char *pxstart, *pxsave;
		char *pend = strstr(p, "</k>");
		if (!pend) {
			fprintf(stderr, "error: no end key.</k>\n");
			return 6;
		}
		dataoffset = p - buffer;
		if (flag) {
			dataoffset -= datasize;
			dataoffset = htonl(dataoffset);
			fwrite(&dataoffset, 4, 1, stdout);//3
			dataoffset = p - buffer;
		} else {
			flag = 1;
		}
		p += 3;
		*pend = 0;
		pxsave = p;
		while ((pxstart = strchr(pxsave, '&'))) {
			for (ret = 0; raw_entrs[ret]; ++ret) {
				if (strncmp(pxstart + 1, xml_entrs[ret], xml_ent_len[ret]) == 0) {
					*pxstart = raw_entrs[ret];
					while (pxstart[xml_ent_len[ret]]) {
						++pxstart;
						*pxstart = pxstart[xml_ent_len[ret]];
					}
					break;
				}
			}
			if (!raw_entrs[ret]) {//not found in xml_entrs
				pxsave = pxstart + 1;
			}
		}

		printf("%s", p);//1
		putchar(0);
		datasize = dataoffset;
		dataoffset = htonl(dataoffset);
		fwrite(&dataoffset, 4, 1, stdout);//2
		p = pend + 4;
	}
	datasize = infStat.st_size - datasize;
	datasize = htonl(datasize);
	fwrite(&datasize, 4, 1, stdout);//3
	free(buffer);
	return 0;
}
