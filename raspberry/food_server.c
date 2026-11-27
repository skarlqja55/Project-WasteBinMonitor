#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <mysql/mysql.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#define BUF_SIZE 1024
#define COLLECT_DROP_PERCENT 20
#define COLLECT_CONFIRM_COUNT 2
#define BIN_COUNT 3
#define CLIENT_TIMEOUT_SECONDS 2

static const char *BIN_NAMES[BIN_COUNT] = { "A", "B", "C" };
static int base_percent[BIN_COUNT];
static int has_base = 0;
static int low_count[BIN_COUNT] = {0, 0, 0};

static int send_bluetooth(const char *message)
{
    int fd = open("/dev/rfcomm0", O_WRONLY | O_NOCTTY);
    if (fd == -1) {
        perror("Bluetooth open");
        return 0;
    }

    size_t remaining = strlen(message);
    while (remaining > 0) {
        ssize_t written = write(fd, message, remaining);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0) {
            close(fd);
            return 0;
        }
        message += written;
        remaining -= (size_t)written;
    }
    close(fd);
    return 1;
}

static void send_bluetooth_status(int a, int b, int c)
{
    char message[64];
    snprintf(message, sizeof(message), "STATUS@%d@%d@%d\n", a, b, c);
    send_bluetooth(message);
}

static const char *env_or(const char *name, const char *fallback)
{
    const char *value = getenv(name);
    return value && *value ? value : fallback;
}

void load_base_from_db(MYSQL *conn)
{
    MYSQL_RES *result;
    MYSQL_ROW row;

    if (mysql_query(conn,
            "SELECT bin_a, bin_b, bin_c FROM food_bin ORDER BY id DESC LIMIT 1")) {
        fprintf(stderr, "BASE LOAD ERROR : %s\n", mysql_error(conn));
        return;
    }

    result = mysql_store_result(conn);
    if (result == NULL)
        return;

    row = mysql_fetch_row(result);
    if (row != NULL && row[0] != NULL && row[1] != NULL && row[2] != NULL) {
        base_percent[0] = atoi(row[0]);
        base_percent[1] = atoi(row[1]);
        base_percent[2] = atoi(row[2]);
        has_base = 1;

        printf("BASE LOADED : A=%d%% B=%d%% C=%d%%\n",
               base_percent[0], base_percent[1], base_percent[2]);
    } else {
        printf("BASE : food_bin is empty, use first data\n");
    }

    mysql_free_result(result);
}

void save_collection(MYSQL *conn, const char *bin_name)
{
    char query[128];

    snprintf(query, sizeof(query),
             "INSERT INTO collection_history (bin_name) VALUES ('%s')",
             bin_name);

    if (mysql_query(conn, query))
        fprintf(stderr, "HISTORY INSERT ERROR : %s\n", mysql_error(conn));
    else
        printf("HISTORY INSERT SUCCESS : BIN %s\n", bin_name);
}

void check_collection(MYSQL *conn, const int cur[BIN_COUNT])
{
    if (!has_base) {
        memcpy(base_percent, cur, sizeof(base_percent));
        has_base = 1;
        printf("BASE SET : A=%d%% B=%d%% C=%d%%\n",
               base_percent[0], base_percent[1], base_percent[2]);
        return;
    }

    for (int i = 0; i < BIN_COUNT; i++) {
        if (cur[i] < 0) {
            low_count[i] = 0;
            continue;
        }
        if (base_percent[i] < 0) {
            base_percent[i] = cur[i];
            continue;
        }
        int drop = base_percent[i] - cur[i];

        if (drop < COLLECT_DROP_PERCENT) {
            low_count[i] = 0;
            base_percent[i] = cur[i];
            continue;
        }

        low_count[i]++;

        if (low_count[i] < COLLECT_CONFIRM_COUNT) {
            printf("COLLECT PENDING : bin %s (%d%% -> %d%%, %d/%d)\n",
                   BIN_NAMES[i], base_percent[i], cur[i],
                   low_count[i], COLLECT_CONFIRM_COUNT);
            continue;
        }

        printf("COLLECTED : bin %s (%d%% -> %d%%)\n",
               BIN_NAMES[i], base_percent[i], cur[i]);

        save_collection(conn, BIN_NAMES[i]);
        low_count[i] = 0;
        base_percent[i] = cur[i];
    }
}

static int read_client_frame(int fd, char *buffer, size_t capacity)
{
    size_t length = 0;
    while (length < capacity - 1) {
        ssize_t received = read(fd, buffer + length, capacity - 1 - length);
        if (received < 0 && errno == EINTR)
            continue;
        if (received < 0)
            return 0;
        buffer[length + (size_t)received] = '\0';
        if (received == 0)
            return length > 0; /* A peer write-close may terminate the frame. */
        char *newline = memchr(buffer + length, '\n', (size_t)received);
        if (newline != NULL) {
            newline[1] = '\0';
            return 1;
        }
        length += (size_t)received;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int serv_sock;
    int clnt_sock;
    int option = 1;
    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;
    char buf[BUF_SIZE];
    char query[256];
    MYSQL *conn;

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        exit(1);
    }

    const char *password = getenv("WASTE_DB_PASSWORD");
    if (password == NULL || *password == '\0') {
        fprintf(stderr, "Set WASTE_DB_PASSWORD before starting the server.\n");
        mysql_close(conn);
        return 1;
    }
    if (mysql_real_connect(conn, env_or("WASTE_DB_HOST", "127.0.0.1"),
                           env_or("WASTE_DB_USER", "iot"), password,
                           env_or("WASTE_DB_NAME", "iotdb"),
                           (unsigned int)atoi(env_or("WASTE_DB_PORT", "3306")), NULL, 0) == NULL) {
        fprintf(stderr, "DB CONNECT ERROR : %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }

    printf("MariaDB Connected\n");
    load_base_from_db(conn);

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) {
        perror("socket");
        mysql_close(conn);
        exit(1);
    }

    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("bind");
        close(serv_sock);
        mysql_close(conn);
        exit(1);
    }

    if (listen(serv_sock, 5) == -1) {
        perror("listen");
        close(serv_sock);
        mysql_close(conn);
        exit(1);
    }

    printf("================================\n");
    printf(" Food Waste TCP Server Start\n");
    printf(" Port         : %s\n", argv[1]);
    printf("================================\n");

    while (1) {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);

        if (clnt_sock == -1) {
            perror("accept");
            continue;
        }

        printf("\nClient Connected : %s\n", inet_ntoa(clnt_addr.sin_addr));

        const struct timeval timeout = { .tv_sec = CLIENT_TIMEOUT_SECONDS };
        if (setsockopt(clnt_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("SO_RCVTIMEO");
            close(clnt_sock);
            continue;
        }
        if (read_client_frame(clnt_sock, buf, sizeof(buf))) {
            printf("RECV : %s", buf);

            char *ptr = strtok(buf, "@\r\n");

            if (ptr != NULL && strcmp(ptr, "REQUEST") == 0) {
                char *bin_str = strtok(NULL, "@\r\n");

                if (bin_str != NULL) {
                    char bin = bin_str[0];

                    if (bin_str[1] == '\0' && (bin == 'A' || bin == 'B' || bin == 'C')) {
                        printf("USER REQUEST : BIN %c\n", bin);
                        char message[16];
                        snprintf(message, sizeof(message), "REQUEST@%c\n", bin);
                        /* This acknowledges a completed RFCOMM write, not MCU processing. */
                        const char *reply = send_bluetooth(message) ? "OK\n" : "ERR\n";
                        send(clnt_sock, reply, strlen(reply), MSG_NOSIGNAL);
                    } else {
                        printf("ERROR : Invalid BIN\n");
                    }
                } else {
                    printf("ERROR : Invalid REQUEST data\n");
                }
            }
            else if (ptr != NULL && strcmp(ptr, "SENSOR") == 0) {
                char *a_str = strtok(NULL, "@\r\n");
                char *b_str = strtok(NULL, "@\r\n");
                char *c_str = strtok(NULL, "@\r\n");

                if (a_str != NULL && b_str != NULL && c_str != NULL) {
                    int bin_a = atoi(a_str);
                    int bin_b = atoi(b_str);
                    int bin_c = atoi(c_str);

                    printf("A = %d%%\n", bin_a);
                    printf("B = %d%%\n", bin_b);
                    printf("C = %d%%\n", bin_c);

                    snprintf(query, sizeof(query),
                             "INSERT INTO food_bin (bin_a, bin_b, bin_c) "
                             "VALUES (%d, %d, %d)",
                             bin_a, bin_b, bin_c);

                    if (mysql_query(conn, query)) {
                        fprintf(stderr, "DB INSERT ERROR : %s\n", mysql_error(conn));
                    } else {
                        printf("DB INSERT SUCCESS\n");

                        int cur[BIN_COUNT] = { bin_a, bin_b, bin_c };
                        check_collection(conn, cur);
                        send_bluetooth_status(bin_a, bin_b, bin_c);
                    }
                } else {
                    printf("ERROR : Invalid SENSOR data\n");
                }
            }
            else {
                printf("ERROR : Unknown command\n");
            }
        }
        else {
            printf("ERROR : No data received\n");
        }

        close(clnt_sock);
    }

    close(serv_sock);
    mysql_close(conn);
    return 0;
}
