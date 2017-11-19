#include <stdio.h>
#include <XCOFF.h>
#include <MacTypes.h>

long getblob(char *fileName, char **bufpp)
{
	FILE *fp;
	FileHdr xc_head;

	fp = fopen(fileName, "rb");
	if(!fp) return -1;

	fread(&xc_head, 1, sizeof xc_head, fp);			/* first, is this open file an XCOFF? */

	if(!feof(fp) && xc_head.f_magic == U802TOCMAGIC) {	/* YES, XCOFF */
		SectionHdrEntry xc_sec;

		/* we have an xcoff! */
		//if(xc_head.f_nscns != 0) fprintf(stderr, "(XCOFF) %s has %d (!= 1) sections.\n", fileName, xc_head.f_nscns+1);

		fseek(fp, xc_head.f_opthdr, SEEK_CUR);			/* skip the aux header */

		fread(&xc_sec, 1, sizeof xc_sec, fp);
		if(feof(fp)) return -2;							/* couldnt get a section header entry */

		*bufpp = (char *)malloc(xc_sec.s_size);
		if(!*bufpp) return -3;							/* oom */

		fseek(fp, xc_sec.s_scnptr, SEEK_SET);
		fread(*bufpp, 1, xc_sec.s_size, fp);
		if(feof(fp)) return -4;							/* could not read section */

		return xc_sec.s_size;

	} else {											/* NO, RAW FILE */
		long size;

		fseek(fp, 0, SEEK_END);							/* apparently this passes for an idiom in C */
		size = ftell(fp);

		*bufpp = (char *)malloc(size);
		if(!*bufpp) return -3;							/* oom */

		fseek(fp, 0, SEEK_SET);
		fread(*bufpp, 1, size, fp);
		if(feof(fp)) return -4;							/* could not read section */

		return size;
	}
}

int main(int argc, char *argv[])
{
	FILE *fp;

	char *blob[2];
	long size[2];
	int i;

	if(argc != 3) {
		fprintf(stderr, "Usage: %s ROM WEDGE\n", argv[0]);
		return 1;
	}

	for(i=0; i<2; i++) /* Two or more, use a for. */
	{
		char *fname = argv[i+1];

		size[i] = getblob(fname, &(blob[i]));
		if(size[i] <= 0) {
			fprintf(stderr, "%s: Failed to load: %s\n", argv[0], fname);
			return 1;
		}
	}

	printf("Linking Wedge into %s\n", argv[1]);

	if(size[0] != 0x400000) {
		fprintf(stderr, "%s: ROM is 0x%X bytes, failing\n", argv[0], size[0]);
		return 1;
	}

	if(size[1] > 0x20000) {
		fprintf(stderr, "%s: WEDGE is 0x%X bytes, to large\n", argv[0], size[1]);
		return 1;
	}

	if(blob[0][0x340000] == 0) {
		/* NanoKernel needs to be moved */
		printf("Moving NanoKernel from 310000 to 340000.\n");
		memcpy(blob[0] + 0x340000, blob[0] + 0x310000, 0x20000);

		printf("Erasing original NanoKernel location.\n");
		memset(blob[0] + 0x310000, 0, 0x20000);
	} else {
		printf("NanoKernel already moved to 340000. Erasing old Wedge.\n");
		memset(blob[0] + 0x310000, 0, 0x20000);
	}

	printf("Copying Wedge (0x%X bytes).\n", size[1]);
	memcpy(blob[0] + 0x310000, blob[1], size[1]);

	printf("Writing out.\n");

	fp = fopen(argv[1], "wb");
	if(!fp) {
		fprintf(stderr, "%s: Failed to open for writing: %s\n", argv[0], argv[1]);
		return 1;
	}

	if(fwrite(blob[0], size[0], 1, fp) != 1) {
		fprintf(stderr, "%s: Failed to write: %s\n", argv[0], argv[1]);
		return 1;
	}

	if(fclose(fp)) {
		fprintf(stderr, "%s: Failed to close: %s\n", argv[0], argv[1]);
		return 1;
	}

	return 0;
}
