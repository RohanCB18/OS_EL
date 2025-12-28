#ifndef POLICY_H
#define POLICY_H

#define MAX_PATHS 32
#define MAX_LEN   256

typedef struct {
    char protected_files[MAX_PATHS][MAX_LEN];
    int protected_count;
} Policy;

int load_policy(const char *filename, Policy *policy);
void print_policy(const Policy *policy);

#endif
