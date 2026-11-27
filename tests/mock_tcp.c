#include <assert.h>

#define main server_main
#include "../raspberry/food_server.c"
#undef main

static const char *chunks[3];
static size_t index_in_stream;
static int final_error;

ssize_t __wrap_read(int fd, void *buffer, size_t count)
{
    assert(fd == 12);
    const char *chunk = chunks[index_in_stream];
    if (chunk == NULL) {
        errno = final_error;
        return final_error ? -1 : 0;
    }
    size_t length = strlen(chunk);
    assert(length <= count);
    memcpy(buffer, chunk, length);
    ++index_in_stream;
    return (ssize_t)length;
}

static int receive(char *buffer, size_t size, const char *first, const char *second, int error)
{
    chunks[0] = first;
    chunks[1] = second;
    index_in_stream = 0;
    final_error = error;
    return read_client_frame(12, buffer, size);
}

int main(void)
{
    char frame[64];
    assert(receive(frame, sizeof(frame), "SEN", "SOR@80@35@91\n", 0));
    assert(strcmp(frame, "SENSOR@80@35@91\n") == 0);
    assert(receive(frame, sizeof(frame), "REQUEST@", "A\r\n", 0));
    assert(strcmp(frame, "REQUEST@A\r\n") == 0);
    assert(receive(frame, sizeof(frame), "REQUEST@B", NULL, 0));
    assert(strcmp(frame, "REQUEST@B") == 0);
    assert(!receive(frame, sizeof(frame), "REQUEST@C", NULL, EAGAIN));
    assert(!receive(frame, 10, "REQUEST@A", NULL, 0));
    assert(!receive(frame, sizeof(frame), NULL, NULL, 0));
    puts("PASS: fragmented messages, LF/CRLF, EOF, timeout, full buffer and empty connection");
}
