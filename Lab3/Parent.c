#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>

#define SHM_SIZE 1344

typedef struct {
    char fileName[256];
    char inputStr[1024];
    char resultStr[64];
} SharedData;

void print_str(const char *str) {
    write(1, str, strlen(str));
}

void int_to_string(int number, char *output_buffer) {
    int pos = 0;
    int is_negative = 0;
    long long temp_number = number; 
    
    if (temp_number < 0) { 
        is_negative = 1; 
        temp_number = -temp_number; 
    }
    
    if (temp_number == 0) {
        output_buffer[pos++] = '0';
    }
    
    while (temp_number > 0) {
        output_buffer[pos++] = (temp_number % 10) + '0';
        temp_number /= 10;
    }
    
    if (is_negative) {
        output_buffer[pos++] = '-';
    }
    output_buffer[pos] = '\0';
    
    int left = 0;
    int right = pos - 1;
    while (left < right) {
        char temp_char = output_buffer[left];
        output_buffer[left] = output_buffer[right];
        output_buffer[right] = temp_char;
        left++; 
        right--;
    }
}

void generate_name(char *dest, const char *prefix, int pid) {
    char pid_str[16];
    int_to_string(pid, pid_str);
    int i = 0;
    while (prefix[i] != '\0') {
        dest[i] = prefix[i];
        i++;
    }
    int j = 0;
    while (pid_str[j] != '\0') {
        dest[i++] = pid_str[j++];
    }
    dest[i] = '\0';
}

void cleanup(pid_t pid, int fd, sem_t *sem_p, sem_t *sem_c, char *shm_name, char *sem_p_name, char *sem_c_name, SharedData *shm_ptr, int kill_child) {
    if (kill_child && pid > 0) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }
    if (shm_ptr && shm_ptr != MAP_FAILED) munmap(shm_ptr, SHM_SIZE);
    if (fd != -1) close(fd);
    if (sem_p && sem_p != SEM_FAILED) sem_close(sem_p);
    if (sem_c && sem_c != SEM_FAILED) sem_close(sem_c);
    if (shm_name && shm_name[0]) shm_unlink(shm_name);
    if (sem_p_name && sem_p_name[0]) sem_unlink(sem_p_name);
    if (sem_c_name && sem_c_name[0]) sem_unlink(sem_c_name);
}

int main() {
    int shm_fd = -1;
    sem_t *sem_parent = SEM_FAILED;
    sem_t *sem_child = SEM_FAILED;
    SharedData *shm_ptr = MAP_FAILED;
    pid_t child_pid = -1;
    
    char shm_name[64] = {0};
    char sem_parent_name[64] = {0};
    char sem_child_name[64] = {0};
    
    pid_t pid = getpid();
    generate_name(shm_name, "/lab3_shm_", pid);
    generate_name(sem_parent_name, "/lab3_sem_p_", pid);
    generate_name(sem_child_name, "/lab3_sem_c_", pid);

    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) { print_str("Ошибка shm_open\n"); return 1; }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) { 
        print_str("Ошибка ftruncate\n"); 
        cleanup(child_pid, shm_fd, sem_parent, sem_child, shm_name, sem_parent_name, sem_child_name, shm_ptr, 0);
        return 1; 
    }

    shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) { 
        print_str("Ошибка mmap\n"); 
        cleanup(child_pid, shm_fd, sem_parent, sem_child, shm_name, sem_parent_name, sem_child_name, shm_ptr, 0);
        return 1; 
    }

    memset(shm_ptr, 0, SHM_SIZE);

    sem_parent = sem_open(sem_parent_name, O_CREAT, 0666, 0);
    sem_child = sem_open(sem_child_name, O_CREAT, 0666, 0);
    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
        print_str("Ошибка sem_open\n"); 
        cleanup(child_pid, shm_fd, sem_parent, sem_child, shm_name, sem_parent_name, sem_child_name, shm_ptr, 0);
        return 1;
    }

    print_str("Введите название файла: ");
    ssize_t bytes = read(0, shm_ptr->fileName, 255);
    if (bytes > 0) {
        if (shm_ptr->fileName[bytes - 1] == '\n') shm_ptr->fileName[bytes - 1] = '\0';
        else shm_ptr->fileName[bytes] = '\0';
    } else {
        shm_ptr->fileName[0] = '\0';
    }

    print_str("Введите числа и нажмите Enter: ");
    bytes = read(0, shm_ptr->inputStr, 1023);
    if (bytes > 0) {
        if (shm_ptr->inputStr[bytes - 1] == '\n') shm_ptr->inputStr[bytes - 1] = '\0';
        else shm_ptr->inputStr[bytes] = '\0';
    } else {
        shm_ptr->inputStr[0] = '\0';
    }

    child_pid = fork();
    if (child_pid == -1) { 
        print_str("Ошибка fork\n"); 
        cleanup(child_pid, shm_fd, sem_parent, sem_child, shm_name, sem_parent_name, sem_child_name, shm_ptr, 0);
        return 1; 
    }

    if (child_pid == 0) {
        char *args[] = {"./child", shm_name, sem_parent_name, sem_child_name, NULL};
        execv(args[0], args);
        
        print_str("Ошибка execv\n");
        sem_post(sem_child); 
        exit(4);
    } else {
        sem_post(sem_parent);
        sem_wait(sem_child); 

        int status;
        waitpid(child_pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 0) {
                print_str("Результат: ");
                print_str(shm_ptr->resultStr);
                print_str("Работа завершена\n");
            } else if (exit_code == 2) {
                print_str("В дочернем процессе произошло переполнение\n");
            } else if (exit_code == 3) {
                print_str("Дочерний процесс не смог открыть файл\n");
            } else if (exit_code == 4) {
                print_str("Исполняемый файл child не найден\n");
            } else {
                print_str("Дочерний процесс завершился с неизвестной ошибкой\n");
            }
        }

        cleanup(-1, shm_fd, sem_parent, sem_child, shm_name, sem_parent_name, sem_child_name, shm_ptr, 0);
    }
    return 0;
}