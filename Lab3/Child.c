#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

#define SHM_SIZE 1344

typedef struct {
    char fileName[256];
    char inputStr[1024];
    char resultStr[64];
} SharedData;

int parse_string_to_int(const char *str, int bytes, int *result_array, int max_count) {
    int count = 0, i = 0;
    while (i < bytes && count < max_count) {
        while (i < bytes && (str[i] < '0' || str[i] > '9') && str[i] != '-') i++;
        if (i >= bytes) break;
        int is_negative = 0;
        if (str[i] == '-') {
            if (i + 1 < bytes && str[i + 1] >= '0' && str[i + 1] <= '9') {
                is_negative = 1; i++;
            } else { i++; continue; }
        }
        int current_number = 0;
        while (i < bytes && str[i] >= '0' && str[i] <= '9') {
            current_number = current_number * 10 + (str[i] - '0'); i++;
        }
        if (is_negative) current_number = -current_number;
        result_array[count++] = current_number;
    }
    return count;
}

void reverse(char *str, int len) {
    int start = 0, end = len - 1;
    while (start < end) {
        char temp = str[start]; str[start++] = str[end]; str[end--] = temp;
    }
}

int int_to_str(char *dest, int n) {
    int i = 0;
    if (n == 0) { dest[i++] = '0'; dest[i] = '\0'; return i; }
    long long nbr = n;
    int is_negative = 0;
    if (nbr < 0) { is_negative = 1; nbr = -nbr; }
    while (nbr > 0) { dest[i++] = (nbr % 10) + '0'; nbr /= 10; }
    if (is_negative) dest[i++] = '-';
    dest[i] = '\0';
    reverse(dest, i);
    return i;
}

int main(int argc, char *argv[]) {
    if (argc < 4) exit(5);

    sem_t *sem_parent = sem_open(argv[2], 0);
    sem_t *sem_child = sem_open(argv[3], 0);
    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) exit(1);

    int shm_fd = shm_open(argv[1], O_RDWR, 0666);
    if (shm_fd == -1) {
        sem_post(sem_child);
        exit(1);
    }

    SharedData *shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        close(shm_fd);
        sem_post(sem_child);
        exit(1);
    }

    sem_wait(sem_parent);

    int file = open(shm_ptr->fileName, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (file == -1) {
        sem_post(sem_child); 
        munmap(shm_ptr, SHM_SIZE);
        close(shm_fd);
        exit(3); 
    }

    int int_massive[512];
    int len_int_massive = parse_string_to_int(shm_ptr->inputStr, strlen(shm_ptr->inputStr), int_massive, 512);
    int result = 0;

    for (int i = 0; i < len_int_massive; i++) {
        int temp = int_massive[i];
        if ((temp > 0 && result > INT_MAX - temp) || (temp < 0 && result < INT_MIN - temp)) {
            close(file);
            sem_post(sem_child);
            munmap(shm_ptr, SHM_SIZE);
            close(shm_fd);
            exit(2); 
        }
        result += temp;
    }

    char resultStr[64];
    int len = int_to_str(resultStr, result);
    resultStr[len++] = '\n';
    resultStr[len] = '\0';

    write(file, resultStr, len);
    close(file);

    strcpy(shm_ptr->resultStr, resultStr);
    sem_post(sem_child);

    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    sem_close(sem_parent);
    sem_close(sem_child);

    exit(0);
}