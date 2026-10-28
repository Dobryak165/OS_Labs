#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;
int active_threads = 0;
int max_threads = 0;

int try_enter_thread() {
    int success = 0;
    pthread_mutex_lock(&thread_lock);
    if (active_threads < max_threads) {
        active_threads++;
        success = 1;
    }
    pthread_mutex_unlock(&thread_lock);
    return success;
}

void leave_thread() {
    pthread_mutex_lock(&thread_lock);
    active_threads--;
    pthread_mutex_unlock(&thread_lock);
}

typedef struct {
    int *array;
    int start;
    int count;
    int direct; 
} ThreadArgs;

void compare_and_swap(int *array, int i, int j, int direct) {
    if ((array[i] > array[j] && direct == 1) || (array[i] < array[j] && direct == 0)) {
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

void* bitonic_merge_parallel(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    
    if (args->count > 1) {
        int k = args->count / 2;
        for (int i = args->start; i < args->start + k; i++) {
            compare_and_swap(args->array, i, i + k, args->direct);
        }
        
        ThreadArgs args1 = {args->array, args->start, k, args->direct};
        ThreadArgs args2 = {args->array, args->start + k, k, args->direct};
        
        pthread_t tid1, tid2;
        int spawned1 = 0, spawned2 = 0;

        if (try_enter_thread()) {
            pthread_create(&tid1, NULL, bitonic_merge_parallel, &args1);
            spawned1 = 1;
        } else {
            bitonic_merge_parallel(&args1); 
        }

        if (try_enter_thread()) {
            pthread_create(&tid2, NULL, bitonic_merge_parallel, &args2);
            spawned2 = 1;
        } else {
            bitonic_merge_parallel(&args2); 
        }

        if (spawned1) { 
            pthread_join(tid1, NULL); 
            leave_thread(); 
        }
        if (spawned2) { 
            pthread_join(tid2, NULL); 
            leave_thread(); 
        }
    }
    return NULL;
}

void bitonic_merge_serial(int *array, int start, int count, int direct) {
    if (count > 1) {
        int k = count / 2;
        for (int i = start; i < start + k; i++) {
            compare_and_swap(array, i, i + k, direct);
        }
        bitonic_merge_serial(array, start, k, direct);
        bitonic_merge_serial(array, start + k, k, direct);
    }
}

void* bitonic_sort_parallel(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    
    if (args->count > 1) {
        int k = args->count / 2;
        
        ThreadArgs args1 = {args->array, args->start, k, 1}; 
        ThreadArgs args2 = {args->array, args->start + k, k, 0}; 
        
        pthread_t tid1, tid2;
        int spawned1 = 0, spawned2 = 0;

        if (try_enter_thread()) {
            pthread_create(&tid1, NULL, bitonic_sort_parallel, &args1);
            spawned1 = 1;
        } else {
            bitonic_sort_parallel(&args1);
        }

        if (try_enter_thread()) {
            pthread_create(&tid2, NULL, bitonic_sort_parallel, &args2);
            spawned2 = 1;
        } else {
            bitonic_sort_parallel(&args2);
        }

        if (spawned1) { 
            pthread_join(tid1, NULL); 
            leave_thread(); 
        }
        if (spawned2) { 
            pthread_join(tid2, NULL); 
            leave_thread(); 
        }
        
        bitonic_merge_parallel(args);
    }
    return NULL;
}

void bitonic_sort_serial(int *array, int start, int count, int direct) {
    if (count > 1) {
        int k = count / 2;
        bitonic_sort_serial(array, start, k, 1);
        bitonic_sort_serial(array, start + k, k, 0);
        bitonic_merge_serial(array, start, count, direct);
    }
}

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

void my_print(const char *str) {
    write(1, str, strlen(str));
}

void my_print_int(int n) {
    char buf[16];
    int i = 0;
    if (n == 0) buf[i++] = '0';
    else {
        if (n < 0) { write(1, "-", 1); n = -n; }
        while (n > 0) {
            buf[i++] = (n % 10) + '0';
            n /= 10;
        }
    }
    for (int j = i - 1; j >= 0; j--) {
        write(1, &buf[j], 1);
    }
}

char* my_read_line() {
    size_t capacity = 128;
    size_t length = 0;
    char *line = malloc(capacity);
    if (!line) return NULL;

    char ch;
    ssize_t bytes_read;
    while ((bytes_read = read(0, &ch, 1)) == 1) {
        if (ch == '\n') break;
        if (length + 1 >= capacity) {
            capacity *= 2;
            char *temp = realloc(line, capacity);
            if (!temp) { free(line); return NULL; }
            line = temp;
        }
        line[length++] = ch;
    }
    line[length] = '\0';
    return line;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        my_print("Использование: ./program <количество_потоков>\n");
        return 1;
    }

    char *endptr;
    errno = 0;
    long threads_val = strtol(argv[1], &endptr, 10);

    if (errno == ERANGE || threads_val > INT_MAX || threads_val <= 0 || *endptr != '\0') {
        my_print("Ошибка: количество потоков должно быть положительным целым числом.\n");
        return 1;
    }
    max_threads = (int)threads_val;

    my_print("Введите размер массива: ");
    char *n_str = my_read_line();
    if (!n_str) return 1;
    
    errno = 0;
    long N_long = strtol(n_str, &endptr, 10);
    
    if (errno == ERANGE || N_long <= 0 || N_long > INT_MAX || *endptr != '\0' || endptr == n_str) {
        my_print("Ошибка чтения размера массива (введите положительное число).\n");
        free(n_str);
        return 1;
    }
    
    free(n_str);
    int N = (int)N_long;

    int M = 1;
    while (M < N) {
        M <<= 1;
    }

    int *arr_serial = malloc(M * sizeof(int));
    int *arr_parallel = malloc(M * sizeof(int));
    
    if (!arr_serial || !arr_parallel) {
        my_print("Ошибка выделения памяти.\n");
        return 1;
    }

    my_print("Введите элементы через пробел:\n");

    char *line = my_read_line();
    if (!line) return 1;

    int parsed_count = 0;
    char *token = strtok(line, " \t\n");
    while (token != NULL) {
        if (parsed_count >= N) {
            my_print("Введено больше элементов, чем надо.\n");
            free(line);
            return 1;
        }

        errno = 0;
        char *num_endptr; 
        long val = strtol(token, &num_endptr, 10);

        if (errno == ERANGE || val > INT_MAX || val < INT_MIN) {
            my_print("Число выходит за пределы int.\n");
            free(line);
            return 1;
        }
        if (*num_endptr != '\0') {
            my_print("Введено не целое число.\n");
            free(line);
            return 1;
        }

        arr_parallel[parsed_count] = (int)val;
        arr_serial[parsed_count] = (int)val;
        parsed_count++;

        token = strtok(NULL, " \t\n");
    }
    free(line);

    if (parsed_count < N) {
        my_print("Введено меньше элементов, чем надо.\n");
        free(arr_serial);   
        free(arr_parallel); 
        return 1;
    }

    for (int i = N; i < M; i++) {
        arr_serial[i] = INT_MAX;
        arr_parallel[i] = INT_MAX;
    }

    double start = get_time();
    bitonic_sort_serial(arr_serial, 0, M, 1);
    double end = get_time();
    double time_serial = (end - start) * 1000.0;

    start = get_time();
    ThreadArgs root = {arr_parallel, 0, M, 1};
    pthread_t root_tid;
    pthread_create(&root_tid, NULL, bitonic_sort_parallel, &root);
    pthread_join(root_tid, NULL);
    end = get_time();
    double time_parallel = (end - start) * 1000.0;
    
    my_print("Отсортированный массив: ");
    for (int i = 0; i < N; i++) {
        my_print_int(arr_parallel[i]);
        my_print(" ");
    }
    my_print("\n");

    my_print("Время для последовательного варианта: ");
    my_print_int((int)time_serial);
    my_print(" мс\n");

    my_print("Время для параллельного варианта: ");
    my_print_int((int)time_parallel);
    my_print(" мс\n");

    double speedup = time_serial / time_parallel;
    double efficiency = speedup / max_threads;
    
    my_print("Ускорение: ");
    my_print_int((int)(speedup * 100)); 
    my_print("\n");

    my_print("Эффективность: ");
    my_print_int((int)(efficiency * 100));
    my_print("\n");

    free(arr_serial);
    free(arr_parallel);
    return 0;
}