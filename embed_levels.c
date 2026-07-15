#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_PATH 1024
#define MAX_LINE 256

// Function to process a single level file
void process_level_file(FILE* output, const char* filename, int is_last) {
    FILE* input;
    char line[MAX_LINE];
    char* base_name;

    // Get base name from path
    base_name = strrchr(filename, '/');
    if (base_name) {
        base_name++; // Skip the '/'
    } else {
        base_name = (char*)filename;
    }

    // Open the input file
    input = fopen(filename, "r");
    if (!input) {
        fprintf(stderr, "Error: Could not open file: %s\n", filename);
        return;
    }

    printf("Processing %s...\n", base_name);

    // Start the level entry
    fprintf(output, "    {\n");
    fprintf(output, "        \"%s\",\n", base_name);
    fprintf(output, "        \"");

    // Process each line of the level file
    while (fgets(line, sizeof(line), input)) {
        char* p = line;
        size_t len = strlen(line);

        // Remove trailing newline if present
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
            if (len > 0 && line[len-1] == '\r') {
                line[--len] = '\0';
            }
        }

        // Escape backslashes and double quotes
        while (*p) {
            if (*p == '\\' || *p == '\"') {
                fprintf(output, "\\%c", *p);
            } else if (*p == '$') {
                // For dollar sign, we need to escape it differently to avoid warnings
                fprintf(output, "$");
            } else {
                fprintf(output, "%c", *p);
            }
            p++;
        }

        // Add newline escape sequence
        fprintf(output, "\\n");
    }

    // End the string and close the level entry
    if (is_last) {
        fprintf(output, "\"\n    }\n");
    } else {
        fprintf(output, "\"\n    },\n");
    }

    fclose(input);
}

// Function to sort file names
int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

int main(void) {
    DIR* dir;
    struct dirent* entry;
    FILE* output;
    char level_dir[] = "levels";
    char output_file[] = "embedded_levels.h";
    char full_path[MAX_PATH];
    char** file_list = NULL;
    int file_count = 0;
    int file_capacity = 10;
    int i;

    // Open the levels directory
    dir = opendir(level_dir);
    if (!dir) {
        fprintf(stderr, "Error: Could not open directory: %s\n", level_dir);
        return 1;
    }

    // Allocate memory for the file list
    file_list = (char**)malloc(file_capacity * sizeof(char*));
    if (!file_list) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        closedir(dir);
        return 1;
    }

    // Read all .sok files in the directory
    while ((entry = readdir(dir)) != NULL) {
        size_t name_len = strlen(entry->d_name);

        if (name_len > 4 && strcmp(entry->d_name + name_len - 4, ".sok") == 0) {
            // Resize file list if needed
            if (file_count >= file_capacity) {
                file_capacity *= 2;
                file_list = (char**)realloc(file_list, file_capacity * sizeof(char*));
                if (!file_list) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    closedir(dir);
                    return 1;
                }
            }

            // Create full path to the file
            sprintf(full_path, "%s/%s", level_dir, entry->d_name);

            // Allocate memory for and store the file path
            file_list[file_count] = strdup(full_path);
            if (!file_list[file_count]) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                closedir(dir);
                for (i = 0; i < file_count; i++) {
                    free(file_list[i]);
                }
                free(file_list);
                return 1;
            }

            file_count++;
        }
    }

    closedir(dir);

    // Check if any .sok files were found
    if (file_count == 0) {
        fprintf(stderr, "Error: No .sok files found in '%s'\n", level_dir);
        free(file_list);
        return 1;
    }

    // Sort the file list
    qsort(file_list, file_count, sizeof(char*), compare_strings);

    // Open the output file
    output = fopen(output_file, "w");
    if (!output) {
        fprintf(stderr, "Error: Could not create output file: %s\n", output_file);
        for (i = 0; i < file_count; i++) {
            free(file_list[i]);
        }
        free(file_list);
        return 1;
    }

    // Write the header file header
    fprintf(output, "#ifndef EMBEDDED_LEVELS_H\n");
    fprintf(output, "#define EMBEDDED_LEVELS_H\n\n");
    fprintf(output, "/* Auto-generated file containing embedded Sokoban levels */\n\n");
    fprintf(output, "/* Level data structure */\n");
    fprintf(output, "typedef struct {\n");
    fprintf(output, "    const char* name;\n");
    fprintf(output, "    const char* data;\n");
    fprintf(output, "} EmbeddedLevel;\n\n");
    fprintf(output, "/* Array of embedded levels */\n");
    fprintf(output, "static const EmbeddedLevel embedded_levels[] = {\n");

    // Process each .sok file
    for (i = 0; i < file_count; i++) {
        process_level_file(output, file_list[i], i == file_count - 1);
        free(file_list[i]);
    }

    // Write the header file footer
    fprintf(output, "};\n\n");
    fprintf(output, "/* Number of embedded levels */\n");
    fprintf(output, "#define NUM_EMBEDDED_LEVELS %d\n\n", file_count);
    fprintf(output, "#endif /* EMBEDDED_LEVELS_H */\n");

    fclose(output);
    free(file_list);

    printf("Successfully generated %s with %d levels\n", output_file, file_count);

    return 0;
}
