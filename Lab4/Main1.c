#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "Lab4.h"

typedef float (*IntegralFunc)(float, float, float);
typedef float (*DerivativeFunc)(float, float);

typedef struct {
    void *handle;
    int number;
    IntegralFunc integral;
    DerivativeFunc derivative;
} Library;

typedef struct {
    char buffer[1024];
    size_t pos;
    size_t end;
} Input;

static void my_print(int fd, const char *str) {
    size_t bytes_left = strlen(str);
    while (bytes_left > 0) {
        ssize_t bytes_written = write(fd, str, bytes_left);
        if (bytes_written < 0 && errno == EINTR) continue;
        if (bytes_written <= 0) _exit(1);
        str += bytes_written;
        bytes_left -= (size_t)bytes_written;
    }
}

void print_error(const char *name, const char *message) {
    my_print(2, name);
    my_print(2, ": ");
    my_print(2, message);
    my_print(2, "\n");
}

static int my_read_line(Input *input, char *line, size_t capacity) {
    size_t length = 0;
    int invalid = 0;

    while (1) {
        if (input->pos == input->end) {
            ssize_t bytes = read(0, input->buffer, sizeof(input->buffer));
            if (bytes < 0 && errno == EINTR) continue;
            if (bytes < 0) return -1;
            if (bytes == 0) {
                if (length == 0 && !invalid) return 0;
                break;
            }
            input->pos = 0;
            input->end = (size_t)bytes;
        }

        char ch = input->buffer[input->pos++];
        if (ch == '\n') break;
        if (ch == '\0' || length == capacity - 1) {
            invalid = 1;
        } else {
            line[length++] = ch;
        }
    }

    line[length] = '\0';
    return invalid ? -2 : 1;
}

static int read_number(float *result) {
    char *token = strtok(NULL, " \t\r");
    if (token == NULL) return 0;

    char *endptr;
    errno = 0;
    float val = strtof(token, &endptr);
    if (endptr == token || *endptr != '\0' || errno == ERANGE || !isfinite(val)) {
        return 0;
    }

    *result = val;
    return 1;
}

static void my_print_float(float result) {
    if (!isfinite(result)) {
        my_print(1, "Ошибка: расчёт с этими аргументами невозможен.\n");
        return;
    }

    double value = fabs(result);
    int exponent = 0;
    while (value >= 10.0) {
        value /= 10.0;
        exponent++;
    }
    while (value > 0.0 && value < 1.0) {
        value *= 10.0;
        exponent--;
    }

    unsigned long digits = (unsigned long)(value * 100000000.0 + 0.5);
    if (digits == 1000000000UL) {
        digits /= 10;
        exponent++;
    }

    char buf[20];
    size_t i = 0;
    if (result < 0.0f) buf[i++] = '-';

    buf[i++] = (char)('0' + digits / 100000000UL);
    buf[i++] = '.';
    for (unsigned long place = 10000000UL; place > 0; place /= 10) {
        buf[i++] = (char)('0' + digits / place % 10);
    }

    if (exponent != 0) {
        buf[i++] = 'e';
        buf[i++] = exponent < 0 ? '-' : '+';
        int power = exponent < 0 ? -exponent : exponent;
        buf[i++] = (char)('0' + power / 10);
        buf[i++] = (char)('0' + power % 10);
    }
    buf[i++] = '\n';
    buf[i] = '\0';
    my_print(1, "Результат: ");
    my_print(1, buf);
}

void print_implementation(int number) {
    my_print(1, number == 1 ? "Реализация: 1\n" : "Реализация: 2\n");
}

int run_commands(Library *library, void (*switch_library)(Library *)) {
    Input input = {0};
    char line[512];
    float a, b, step;

    my_print(1, "1 a b e — интеграл sin(x); 2 a dx — производная cos(x).\n");
    if (switch_library != NULL) my_print(1, "0 — сменить реализацию.\n");
    print_implementation(library->number);

    while (1) {
        int status = my_read_line(&input, line, sizeof(line));
        if (status == 0) return 0;
        if (status == -1) {
            print_error("Ошибка чтения", strerror(errno));
            return 1;
        }
        if (status == -2) {
            my_print(1, "Ошибка: слишком длинная строка или нулевой байт.\n");
            continue;
        }

        char *command = strtok(line, " \t\r");
        if (command == NULL) continue;

        if (strcmp(command, "0") == 0 && strtok(NULL, " \t\r") == NULL) {
            if (switch_library != NULL) {
                switch_library(library);
            } else {
                my_print(1, "Команда 0 доступна только в программе №2.\n");
            }
        } else if (strcmp(command, "1") == 0 && read_number(&a) &&
                   read_number(&b) && read_number(&step) &&
                   strtok(NULL, " \t\r") == NULL) {
            my_print_float(library->integral(a, b, step));
        } else if (strcmp(command, "2") == 0 && read_number(&a) &&
                   read_number(&step) && strtok(NULL, " \t\r") == NULL) {
            my_print_float(library->derivative(a, step));
        } else {
            my_print(1, "Ошибка: ожидается команда 1 a b e или 2 a dx.\n");
        }
    }
}

int main(void) {
    Library library = {NULL, 1, sin_integral, cos_derivative};
    return run_commands(&library, NULL);
}
