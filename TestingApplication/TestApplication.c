#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <pthread.h>
#include <time.h>

pthread_mutex_t file_mutex;
int SLEEP_TIME = 2000;

const char* print_current_time() {
    time_t now;
    time(&now); 
    struct tm *local = localtime(&now);

    static char time_str[20];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", local);

    return time_str;
}

void print_with_timestamp(const char* message) {
    printf("%-50s [%s]\n", message, print_current_time()); 
}

void write_file(const char *filename, const char *content) {
    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening file for writing");
        pthread_mutex_unlock(&file_mutex);
        exit(1);
    }
    fprintf(file, "%s", content);
    fclose(file);
    print_with_timestamp("Data written successfully");
    pthread_mutex_unlock(&file_mutex);
}

void read_file(const char *filename) {
    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        pthread_mutex_unlock(&file_mutex);
        exit(1);
    }
    char ch;
    printf("Contents: %-40s [%s]\n", filename, print_current_time());
    
    while ((ch = fgetc(file)) != EOF) {
        putchar(ch);
    }
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
}


void modify_file(const char *filename, const char *additional_content) {
    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        perror("Error opening file for modification");
        pthread_mutex_unlock(&file_mutex);
        exit(1);
    }
    fprintf(file, "%s", additional_content);
    print_with_timestamp("File modified with additional content");
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
}

void delete_file(const char *filename) {
    pthread_mutex_lock(&file_mutex);
    if (remove(filename) == 0) {
        print_with_timestamp("File deleted successfully");
    } else {
        perror("Error deleting file");
    }
    pthread_mutex_unlock(&file_mutex);
}

void* thread_function(void* arg) {
    const char *filename = (const char*) arg;
    const char *initial_content = "Initial content\n";
    const char *additional_content = "Additional content added\n";

    // Write initial content to the file
    write_file(filename, initial_content);
    Sleep(SLEEP_TIME);

    // Read the file content
    read_file(filename);
    Sleep(SLEEP_TIME);

    // Modify the file by appending additional content
    modify_file(filename, additional_content);
    Sleep(SLEEP_TIME);

    // Read the file again to confirm modification
    read_file(filename);
    Sleep(SLEEP_TIME);
    
    // Delete the file
    delete_file(filename);

    return NULL;
}

int main() {
    pthread_t threads[3];
    const char *filenames[3] = {"thread1.txt", "thread2.txt", "thread3.txt"};

    // Initialize the mutex
    if (pthread_mutex_init(&file_mutex, NULL) != 0) {
        perror("Error initializing mutex");
        return 1;
    }

    // Create and start threads
    for (int i = 0; i < 3; i++) {
        if (pthread_create(&threads[i], NULL, thread_function, (void*) filenames[i]) != 0) {
            perror("Error creating thread");
            return 1;
        }
        Sleep(SLEEP_TIME);
    }

    // Join threads
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&file_mutex);

    return 0;
}
