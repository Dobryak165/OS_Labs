#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    char fileName[256];
    write(1, "Введите название файла: ", 43);

    ssize_t len = read(0, fileName, sizeof(fileName) - 1);

    if (len == -1) {
        write(2, "Ошибка чтения\n", 25);
        return 1;
    }
    if (len == 0) {
        write(1, "Ввод завершен\n", 25);
        return 0;
    }

    if (fileName[len - 1] == '\n') {
        fileName[len - 1] = '\0';
    } else {
        fileName[len] = '\0';
    }

    int pipe1[2];
    int pipe2[2];

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        write(2, "Ошибка создания pipe\n", 35);
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        write(2, "Ошибка создания процесса\n", 43);
        return 1;
    }

    if (pid == 0) {
        dup2(pipe1[0], 0);
        dup2(pipe2[1], 4); 

        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);

        char *argv[] = {"./child", fileName, NULL};
        execv(argv[0], argv);

        write(2, "Не удалось запустить ./child\n", 48);
        exit(4); 
    } else {
        close(pipe1[0]);
        close(pipe2[1]);

        char buffer[1024];
        
        write(1, "Введите числа и нажмите Enter: ", 52);
        
        ssize_t bytes = read(0, buffer, sizeof(buffer));

        if (bytes < 0) {
            write(2, "Ошибка чтения ввода\n", 35);
        } 
        else if (bytes > 0) {
            if (write(pipe1[1], buffer, bytes) < 0) {
                write(2, "Ошибка записи в pipe1\n", 36);
            } else {
                ssize_t resultBytes = read(pipe2[0], buffer, sizeof(buffer));

                if (resultBytes < 0) {
                    write(2, "Ошибка чтения ответа из pipe2\n", 49);
                }
                else if (resultBytes > 0) {
                    write(1, "Результат: ", 19);
                    write(1, buffer, resultBytes);
                }
            }
        }

        close(pipe1[1]);
        close(pipe2[0]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int child_status = WEXITSTATUS(status);
            if (child_status == 0) {
                write(1, "Дочерний процесс успешно завершил работу.\n", 76);
            } else if (child_status == 2) {
                write(2, "В дочернем процессе произошло переполнение!\n", 83);
            } else if (child_status == 3) {
                write(2, "Дочерний процесс не смог открыть файл.\n", 72);
            } else if (child_status == 4) {
                write(2, "Исполняемый файл child не найден.\n", 58);
            }
        }

        write(1, "\nРабота завершена\n", 32);
    }

    return 0;
}
