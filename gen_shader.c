#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *read_file(const char *path) {

    FILE *file;
    long size;
    char *buffer;

    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[size] = '\0';
    fclose(file);

    return buffer;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return 1;
    }

    const char *shader_path = argv[1];
    const char *shader_source = read_file(shader_path);

    size_t len = strlen(shader_source);

    FILE *out_file = fopen("gen_shader.h", "w");
    if (out_file == NULL) {
        return 1;
    }

    const char *line_start = shader_source;

    fprintf(out_file, "/* GEN_SHADER_SOURCE is the bytes from the following file content:\n");
    fprintf(out_file, " *\n * %s\n * ```\n", shader_path);
    while (*line_start != '\0') {
        const char *line_end = strchr(line_start, '\n');
        if (line_end == NULL) {
            fprintf(out_file, "%s\n", line_start);
            break;
        }
        fprintf(out_file, " * %.*s\n", (int)(line_end - line_start), line_start);

        line_start = line_end + 1;
    }
    fprintf(out_file, " * ```\n */\n");

    fprintf(out_file, "const char GEN_SHADER_SOURCE[] = {\n");

    for (size_t i = 0; i < len; ++i) {
        if (i % 12 == 0) {
            if (i != 0) {
                fprintf(out_file, "\n");
            }
            fprintf(out_file, "    ");
        }
        fprintf(out_file, "0x%02x", shader_source[i]);
        if (i < len-1) {
            fprintf(out_file, ", ", shader_source[i]);
        }
    }

    fprintf(out_file, "\n};\n");
    //fprintf(out_file, "const size_t GEN_SHADER_SOURCE_LEN = %ld;\n", len);
    fprintf(out_file, "const size_t GEN_SHADER_SOURCE_LEN = sizeof(GEN_SHADER_SOURCE) / sizeof(GEN_SHADER_SOURCE[0]);\n", len);

    if (fclose(out_file) == EOF) {
        return 1;
    }

    return 0;
}
