/*
 * A simple id3 tag reader
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MP3FILE "song2.mp3"

struct HEADERV2
{
	uint8_t id[3];
	uint8_t ver[2];
	uint8_t flags;
	uint8_t size[4];
	uint32_t tagsize; /* does NOT include 10 byte header */
} __attribute__((__packed__));

struct HEADERFRAME
{
	uint8_t id[4];
	uint8_t size[4];
	uint8_t flags[2];
	uint32_t framesize;
} __attribute__((__packed__));

struct HEADERV1
{
	uint8_t id[3];
	uint8_t title[30];
	uint8_t artist[30];
	uint8_t album[30];
	uint8_t year[4];
	uint8_t comment[30];
	uint8_t genre;
} __attribute__((__packed__));

int synchsafe(int in);
int unsynchsafe(int in);
void readid3v2header(FILE *f, struct HEADERV2 *header);
void printid3v2headerinfo(struct HEADERV2 *header);
void readid3v2frames(FILE *f, struct HEADERV2 *header);
void printid3v2frameinfo(struct HEADERFRAME *frame);
void readid3v1header(FILE *f, struct HEADERV1 *header);
void printid3v1headerinfo(struct HEADERV1 *header);

int
main(void)
{
	printf("parsing mp3 file %s\n", MP3FILE);
	FILE *f = fopen(MP3FILE, "rb");
	if (!f)
	{
		fprintf(stderr, "fopen failed on %s\n", MP3FILE);
		exit(1);
	}
	struct HEADERV2 *headerv2 = malloc(sizeof(struct HEADERV2));
	if (!headerv2)
	{
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}
	struct HEADERV1 *headerv1 = malloc(sizeof(struct HEADERV1));
	if (!headerv1)
	{
		fprintf(stderr, "malloc failed\n");
		free(headerv2);
		exit(1);
	}
	readid3v1header(f, headerv1);
	printid3v1headerinfo(headerv1);
	puts("");
	readid3v2header(f, headerv2);
	printid3v2headerinfo(headerv2);
	puts("");
	readid3v2frames(f, headerv2);
	fclose(f);
	free(headerv2);
	free(headerv1);
	return 0;
}

int
synchsafe(int in) // not used yet
{
	int out, mask = 0x7F;

	while (mask ^ 0x7FFFFFFF)
	{
		out = in & ~mask;
		out <<= 1;
		out |= in & mask;
		mask = ((mask + 1) << 8) - 1;
		in = out;
	}

	return out;
}

int
unsynchsafe(int in) // not used yet
{
	int out = 0, mask = 0x7F000000;

	while (mask)
	{
		out >>= 1;
		out |= in & mask;
		mask >>= 8;
	}

	return out;
}

void
readid3v1header(FILE *f, struct HEADERV1 *header)
{
	/* read 128 byte id3v1 header */
	fpos_t oldpos;
	/* save our file position */
	fgetpos(f, &oldpos);

	fseek(f, -128L, SEEK_END);
	fread(header, 1, 128, f);

	/* restore our file position */
	fsetpos(f, &oldpos);


}

void
printid3v1headerinfo(struct HEADERV1 *header)
{
	printf("TAG v1 header data\n");
	char title[31];
	char artist[31];
	char album[31];
	char year[5];
	char comment[31];
	strncpy(title, (char *)header->title, 30);
	strncpy(artist, (char *)header->artist, 30);
	strncpy(album, (char *)header->album, 30);
	strncpy(year, (char *)header->year, 4);
	strncpy(comment, (char *)header->comment, 30);
	printf("title:\t%s\n", title);
	printf("artist:\t%s\n", artist);
	printf("album:\t%s\n", album);
	printf("year:\t%c%c%c%c\n", year[0], year[1], year[2], year[3]);
	printf("comment:\t%s\n", comment);
}

void
readid3v2header(FILE *f, struct HEADERV2 *header)
{
	/*
	 * read 10 byte id3v2 header
	 * 3 bytes->id
	 * 2 bytes->version
	 * 1 bytes->flags
	 * 4 bytes->size
	 * the size is encoded into 4 bytes as a synchsafe integer, the most significant bit is ignored
	 * so we need to convert it
	 */
	fread(header, 1, 10, f);
	/*
	 * the tag size does NOT include the 10 byte header
	 * so it is safe to read tagsize bytes from the file
	 * to obtain the entire header
	 */
	header->tagsize =  header->size[0] << 21 |
			   header->size[1] << 14 |
			   header->size[2] << 7	 |
			   header->size[3];
}

void
printid3v2headerinfo(struct HEADERV2 *header)
{
	puts("TAGv2 header raw bytes");
	int i;
	printf("id:\t");
	for (i = 0; i < 3; i++)
		printf("%02X ", header->id[i]);
	printf("(%c%c%c)\n", header->id[0], header->id[1], header->id[2]);
	printf("ver:\t");
	for (i = 0; i < 2; i++)
		printf("%02X ", header->ver[i]);
	puts("");

	printf("flags:\t");
	printf("%02X\n", header->flags);
	printf("size:\t");
	for (i = 0; i < 4; i++)
		printf("%02X ", header->size[i]);
	printf("(%u bytes)\n", header->tagsize);

}

void
readid3v2frames(FILE *f, struct HEADERV2 *header)
{
	/* we read at absolute most header->tagsize bytes */
	size_t bytesread = 0;
	struct HEADERFRAME *frame = malloc(sizeof(struct HEADERFRAME));
	bytesread += fread(frame, 1, 10, f);
	frame->framesize = frame->size[0] << 21 |
			   frame->size[1] << 14 |
			   frame->size[2] << 7  |
			   frame->size[3];

	printid3v2frameinfo(frame);
	free(frame);
}

void
printid3v2frameinfo(struct HEADERFRAME *frame)
{
	puts("TAGv2 frame raw bytes");
	int i;
	printf("id:\t");
	for (i = 0; i < 4; i++)
		printf("%02X ", frame->id[i]);
	printf("(%c%c%c%c)\n", frame->id[0], frame->id[1], frame->id[2], frame->id[3]);
	printf("size:\t");
	for (i = 0; i < 4; i++)
		printf("%02X ", frame->size[i]);
	printf("(%u bytes)\n", frame->framesize);

}
