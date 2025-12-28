#include <stdio.h>
#include <string.h>
#include <yaml.h>
#include "policy.h"

int load_policy(const char *filename, Policy *policy) {
    FILE *fh = fopen(filename, "r");
    if (!fh) {
        perror("fopen");
        return -1;
    }

    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fh);

    policy->protected_count = 0;
    int in_protected = 0;

    while (1) {
        yaml_parser_parse(&parser, &event);

        if (event.type == YAML_SCALAR_EVENT) {
            char *val = (char *)event.data.scalar.value;

            if (strcmp(val, "protected_files") == 0) {
                in_protected = 1;
            } else if (in_protected) {
                strncpy(policy->protected_files[policy->protected_count++],
                        val, MAX_LEN);
            }
        }

        if (event.type == YAML_SEQUENCE_END_EVENT) {
            in_protected = 0;
        }

        if (event.type == YAML_STREAM_END_EVENT)
            break;

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fh);
    return 0;
}

void print_policy(const Policy *policy) {
    printf("Protected files:\n");
    for (int i = 0; i < policy->protected_count; i++) {
        printf("  - %s\n", policy->protected_files[i]);
    }
}
