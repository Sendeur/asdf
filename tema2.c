/* Sendre Mihai-Alin, 332CC */
/* SO_FILE dynamic library - Operating Systems 2019 */

#define BUFSIZE 4096
#define CHUNKSIZE 512
#define SO_EOF (-1)

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

typedef struct _so_file {
	/* File descriptor for the file */
	int fileno;
	/* Open permissions for the file */
	char *mode;
	/* Pathname to opened file */
	char *pathname;
	/* Cursor into the file */
	int cursor;
	/* Internal buffer */
	char buffer[BUFSIZE];
	/* Offset in buffer */
	int buf_offset;
	/* Data in buffer */
	int buf_bound;
} SO_FILE;

/* Function declarations */
void free_so_file(SO_FILE **file);
SO_FILE* so_fopen(const char *pathname, const char *mode);
int so_fclose(SO_FILE *file);
int so_fileno(SO_FILE *file);
int so_fflush(SO_FILE *file);
int so_fseek(SO_FILE *stream, long offset, int whence);
long so_ftell(SO_FILE *stream);
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream);
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream);
int so_fgetc(SO_FILE *stream);
int so_fputc(int c, SO_FILE *stream);
int so_feof(SO_FILE *stream);
int so_ferror(SO_FILE *stream);
SO_FILE *so_popen(const char *command, const char *type);
int so_pclose(SO_FILE *stream);

/* Function that handles the deallocation of a SO_FILE* */
void free_so_file(SO_FILE **file)
{
	/* free strings */
	free((*file)->mode);
	free((*file)->pathname);
	/* free whole structure */
	free((*file));
}

/* Function that opens a file */
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	/* create a SO_FILE object */
	SO_FILE *file = (SO_FILE *) malloc(sizeof(SO_FILE));

	/* initialize everything to 0 */
	memset(file, 0, sizeof(SO_FILE));

	/* copy arguments to SO_FILE */
	file->pathname = (char *) malloc(strlen(pathname)+1);
	strcpy(file->pathname, pathname);
	file->mode = (char *) malloc(strlen(mode)+1);
	strcpy(file->mode, mode);

	/* syscall to open the file in various modes */
	if (!strcmp(file->mode, "r"))
		file->fileno = open(file->pathname, O_RDONLY);
	else if (!strcmp(file->mode, "r+"))
		file->fileno = open(file->pathname, O_RDWR);
	else if (!strcmp(file->mode, "w"))
		file->fileno = open(file->pathname, O_WRONLY|O_CREAT|O_TRUNC,
									0644);
	else if (!strcmp(file->mode, "w+"))
		file->fileno = open(file->pathname, O_RDWR|O_CREAT|O_TRUNC,
									0644);
	else if (!strcmp(file->mode, "a")) {
		file->fileno = open(file->pathname, O_WRONLY|O_CREAT|O_APPEND,
									0644);
		/* cursor at the end */
		file->cursor = lseek(file->fileno, 0, SEEK_END);
	} else if (!strcmp(file->mode, "a+")) {
		file->fileno = open(file->pathname, O_RDWR|O_CREAT|O_APPEND,
									0644);
		/* cursor at the end */
		file->cursor = lseek(file->fileno, 0, SEEK_END);
	} else
		file->fileno = -1;

	/* if an error occured, return NULL */
	if (file->fileno < 0) {
		free_so_file(&file);
		return NULL;
	}

	return file;
}

/* Function that closes a file */
int so_fclose(SO_FILE *stream)
{
	/* flush to file */
	so_fflush(stream);

	/* close file */
	int rc = close(stream->fileno);

	/* free SO_FILE */
	free_so_file(&stream);

	return rc < 0 ? SO_EOF : 0;
}

/* Function that returns the fileno associated with a file */
int so_fileno(SO_FILE *stream)
{
	return stream->fileno;
}

/* Function that flushes the buffer of a SO_FILE to the file */
int so_fflush(SO_FILE *stream)
{
	/* write to file */
	char *addr = stream->buffer;
	int rc, size = stream->buf_bound;

	do {
		/* make a syscall */
		rc = write(stream->fileno, addr, size);
		/* error check */
		if (rc < 0)
			return SO_EOF;
		/* job done */
		else if (rc < size) {
			stream->cursor += rc;
			break;
		}

		/* adjust address in buffer and size written */
		addr += rc;
		size -= rc;
		/* adjust cursor in file */
		stream->cursor += rc;

	} while (size > 0);

	/* reset SO_FILE parameters */
	stream->buf_offset = 0;
	stream->buf_bound = 0;

	return 0;
}

/* Function that modifies the cursor associated with a file */
int so_fseek(SO_FILE *stream, long offset, int whence)
{
	//TODO retval: 0 - success, -1 - error
	return 0;
}

/* Function that returns the cursor position associated with a file */
long so_ftell(SO_FILE *stream)
{
	return stream->cursor;
}

/* Function that reads from a file */
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	/* number of characters read so far */
	size_t num = 0;
	/* index into SO_FILE buffer */
	int i = 0;

	/* read everything */
	while (num < size * nmemb) {
		/* store character at index i */
		((char *) ptr)[i++] = so_fgetc(stream);
		/* just read a character */
		num++;
	}

	return num;
}

/* Function that writes to a file */
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	/* number of characters written so far */
	size_t num = 0;
	/* index into SO_FILE buffer */
	int i = 0;

	/* write everything */
	while (num < size * nmemb) {
		/* write character at index i */
		int rc = so_fputc(((char *) ptr)[i++], stream);
		/* just wrote a character */
		num++;
	}

	return num;
}

/* Function that reads a character from a file */
int so_fgetc(SO_FILE *stream)
{
	/* read from buffer, if anything there */
	if (stream->buf_offset < stream->buf_bound) {
		return (int) stream->buffer[stream->buf_offset++];
	}

	/* must flush buffer because it's full */
	if (stream->buf_offset == BUFSIZE) {
		stream->buf_offset = 0;
		stream->buf_bound = 0;
	}

	/* must do syscall, but read a chunk, not only one character */
	char *addr = stream->buffer + stream->buf_offset;
	int rc = read(stream->fileno, addr, BUFSIZE);
	/* increment new bound in buffer */
	stream->buf_bound += rc;
	/* adjust cursor in file */
	stream->cursor += rc;

	printf("New cursor: %d\n", stream->cursor);
	return stream->buffer[stream->buf_offset++];
}

/* Function that writes a character to a file */
int so_fputc(int c, SO_FILE *stream)
{
	/* write to buffer, if there's space */
	if (stream->buf_offset < BUFSIZE) {
		stream->buffer[stream->buf_offset++] = c;
		stream->buf_bound++;
		return (int) c;
	}

	/* else, flush buffer to file */
	int rc = so_fflush(stream);

	/* store character into buffer */
	stream->buffer[stream->buf_offset++] = c;
	stream->buf_bound++;

	return (int) c;
}

/* Function that checks whether the cursor is at EOF */
int so_feof(SO_FILE *stream)
{
	//TODO
	return 0;
}

/* Function that checks whether an error has occured */
int so_ferror(SO_FILE *stream)
{
	//TODO
	return 0;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	return NULL;
}

int so_pclose(SO_FILE *stream)
{
	return 0;
}
